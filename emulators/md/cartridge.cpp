#include "cartridge.h"
#include "../../utilities/zip_reader.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace md {

namespace {

constexpr u32 BANK_SIZE = 0x80000;  // 512 KB per SSF2 bank slot

// Trim padding and collapse runs of spaces in the 48-byte header title fields.
std::string cleanTitle(const u8* src, size_t length) {
    std::string out;
    out.reserve(length);
    bool lastSpace = true;
    for (size_t i = 0; i < length; i++) {
        char c = static_cast<char>(src[i]);
        if (c == '\0') break;
        if (c == ' ') {
            if (!lastSpace) out.push_back(' ');
            lastSpace = true;
        } else {
            out.push_back(c);
            lastSpace = false;
        }
    }
    while (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}

} // namespace

Cartridge::Cartridge() = default;

Cartridge::~Cartridge() {
    if (m_loaded && m_hasSram) {
        saveBattery();
    }
}

// SMD images store each 16 KB block as 8 KB of odd bytes followed by 8 KB of
// even bytes, preceded by a 512-byte header.
void Cartridge::deinterleaveSMD(std::vector<u8>& data) {
    if (data.size() <= 512) return;

    std::vector<u8> src(data.begin() + 512, data.end());
    size_t blocks = src.size() / 0x4000;
    std::vector<u8> out(blocks * 0x4000);

    for (size_t b = 0; b < blocks; b++) {
        const u8* in = src.data() + b * 0x4000;
        u8* dst = out.data() + b * 0x4000;
        for (size_t i = 0; i < 0x2000; i++) {
            dst[i * 2 + 1] = in[i];
            dst[i * 2 + 0] = in[i + 0x2000];
        }
    }

    data = std::move(out);
}

bool Cartridge::load(const fs::path& filename) {
    std::vector<u8> data;
    bool smd = false;

    fs::path ext = filename.extension();
    std::string extStr = ext.string();
    std::transform(extStr.begin(), extStr.end(), extStr.begin(), ::tolower);

    if (extStr == ".zip") {
        util::ZipReader zip;
        if (!zip.open(filename)) {
            log_error("Failed to open ZIP: %s", filename.string().c_str());
            return false;
        }
        std::string found;
        const std::set<std::string> exts = { ".gen", ".md", ".smd" };
        if (!zip.findAndExtractFile(exts, data, found)) {
            zip.close();
            log_error("No Mega Drive ROM found in ZIP: %s", filename.string().c_str());
            return false;
        }
        zip.close();
        fs::path inner = found;
        std::string innerExt = inner.extension().string();
        std::transform(innerExt.begin(), innerExt.end(), innerExt.begin(), ::tolower);
        smd = (innerExt == ".smd");
    } else {
        FILE* file = fopen(filename.string().c_str(), "rb");
        if (!file) {
            log_error("Failed to open ROM: %s", filename.string().c_str());
            return false;
        }
        fseek(file, 0, SEEK_END);
        long size = ftell(file);
        fseek(file, 0, SEEK_SET);
        if (size <= 0) {
            fclose(file);
            log_error("Empty ROM: %s", filename.string().c_str());
            return false;
        }
        data.resize(static_cast<size_t>(size));
        if (fread(data.data(), 1, data.size(), file) != data.size()) {
            fclose(file);
            log_error("Failed to read ROM: %s", filename.string().c_str());
            return false;
        }
        fclose(file);
        smd = (extStr == ".smd") || ((data.size() % 0x4000) == 512);
    }

    if (smd) {
        deinterleaveSMD(data);
    }

    if (data.size() < 0x200) {
        log_error("ROM too small: %s", filename.string().c_str());
        return false;
    }

    m_rom = std::move(data);

    parseHeader();

    // Default identity bank mapping.
    for (u8 i = 0; i < 8; i++) m_banks[i] = i;

    m_savePath = filename;
    m_savePath.replace_extension(".srm");

    if (m_hasSram) {
        u32 sramSize = m_sramEnd - m_sramStart + 1;
        // Clamp to something sane; a few headers contain garbage ranges.
        if (sramSize == 0 || sramSize > 0x10000) sramSize = 0x10000;
        m_sram.assign(sramSize, 0xFF);
        loadBattery();
    }

    m_loaded = true;

    log_info("Loaded ROM: %s", m_title.c_str());
    log_info("  Size: %zu KB", m_rom.size() / 1024);
    log_info("  Region: %s", m_pal ? "PAL" : "NTSC");
    log_info("  SRAM: %s", m_hasSram ? "yes" : "no");

    return true;
}

void Cartridge::parseHeader() {
    if (m_rom.size() < 0x200) return;

    // Prefer the overseas title, fall back to the domestic one.
    m_title = cleanTitle(&m_rom[0x150], 48);
    if (m_title.empty()) m_title = cleanTitle(&m_rom[0x120], 48);
    if (m_title.empty()) m_title = "Unknown";

    // Region field: any of E/A/4/8/C implies PAL-capable; prefer NTSC when the
    // cartridge supports both.
    bool ntscCapable = false;
    bool palCapable = false;
    for (int i = 0; i < 3; i++) {
        char c = static_cast<char>(m_rom[0x1F0 + i]);
        switch (c) {
            case 'J': case 'U': case '1': case '5': case '6': case 'B':
                ntscCapable = true; break;
            case 'E':
                palCapable = true; break;
            case 'A': case '8': case 'C': case '4':
                palCapable = true; ntscCapable = true; break;
            default: break;
        }
    }
    m_pal = palCapable && !ntscCapable;

    // SRAM descriptor: "RA" magic at 0x1B0.
    if (m_rom[0x1B0] == 'R' && m_rom[0x1B1] == 'A') {
        u32 start = (static_cast<u32>(m_rom[0x1B4]) << 24) | (static_cast<u32>(m_rom[0x1B5]) << 16) |
                    (static_cast<u32>(m_rom[0x1B6]) << 8)  |  static_cast<u32>(m_rom[0x1B7]);
        u32 end   = (static_cast<u32>(m_rom[0x1B8]) << 24) | (static_cast<u32>(m_rom[0x1B9]) << 16) |
                    (static_cast<u32>(m_rom[0x1BA]) << 8)  |  static_cast<u32>(m_rom[0x1BB]);
        if (end >= start && start >= 0x200000 && end < 0x400000) {
            m_sramStart = start;
            m_sramEnd = end;
            m_hasSram = true;
        }
    }

    // Some games with battery saves omit the descriptor but still expect SRAM
    // in the standard window once the ROM is smaller than 2 MB.
    if (!m_hasSram && m_rom.size() <= 0x200000) {
        m_sramStart = 0x200000;
        m_sramEnd = 0x203FFF;
        m_hasSram = false;  // only enabled via the 0xA130F1 control register
    }
}

void Cartridge::reset() {
    for (u8 i = 0; i < 8; i++) m_banks[i] = i;
    m_sramEnabled = m_hasSram;
}

u32 Cartridge::mapRomOffset(u32 address) const {
    u32 slot = (address >> 19) & 7;
    u32 offset = (static_cast<u32>(m_banks[slot]) * BANK_SIZE) + (address & (BANK_SIZE - 1));
    return offset;
}

u8 Cartridge::read8(u32 address) const {
    address &= 0x3FFFFF;

    if (m_hasSram && m_sramEnabled && address >= m_sramStart && address <= m_sramEnd) {
        u32 index = address - m_sramStart;
        if (index < m_sram.size()) return m_sram[index];
        return 0xFF;
    }

    u32 offset = mapRomOffset(address);
    if (offset < m_rom.size()) return m_rom[offset];
    return 0xFF;
}

u16 Cartridge::read16(u32 address) const {
    address &= 0x3FFFFE;

    if (m_hasSram && m_sramEnabled && address >= m_sramStart && address <= m_sramEnd) {
        // SRAM sits on one half of the bus; mirror the byte into both halves so
        // word reads behave the way games expect.
        u8 value = read8(address + 1);
        return static_cast<u16>((value << 8) | value);
    }

    u32 offset = mapRomOffset(address);
    if (offset + 1 < m_rom.size()) {
        return static_cast<u16>((m_rom[offset] << 8) | m_rom[offset + 1]);
    }
    return 0xFFFF;
}

void Cartridge::write8(u32 address, u8 value) {
    address &= 0x3FFFFF;

    if (m_hasSram && m_sramEnabled && address >= m_sramStart && address <= m_sramEnd) {
        u32 index = address - m_sramStart;
        if (index < m_sram.size()) {
            m_sram[index] = value;
            m_sramDirty = true;
        }
    }
    // Writes to ROM are ignored.
}

void Cartridge::write16(u32 address, u16 value) {
    address &= 0x3FFFFE;

    if (m_hasSram && m_sramEnabled && address >= m_sramStart && address <= m_sramEnd) {
        write8(address + 1, static_cast<u8>(value & 0xFF));
    }
}

void Cartridge::writeControl(u32 address, u8 value) {
    u32 reg = address & 0xF;

    if (reg == 0x1) {
        // 0xA130F1: bit 0 selects SRAM over ROM in the 0x200000 window,
        // bit 1 is the write-protect flag (ignored here).
        m_sramEnabled = (value & 1) != 0;
        if (m_sramEnabled && m_sram.empty()) {
            m_hasSram = true;
            m_sram.assign(m_sramEnd - m_sramStart + 1, 0xFF);
            loadBattery();
        }
        return;
    }

    // 0xA130F3..0xA130FF: SSF2 mapper, one register per 512 KB slot 1-7.
    if (reg >= 0x3 && (reg & 1)) {
        u32 slot = reg >> 1;
        if (slot < 8) m_banks[slot] = value;
    }
}

void Cartridge::saveBattery() {
    if (!m_hasSram || m_sram.empty() || !m_sramDirty) return;

    FILE* file = fopen(m_savePath.string().c_str(), "wb");
    if (!file) {
        log_error("Failed to write save file: %s", m_savePath.string().c_str());
        return;
    }
    fwrite(m_sram.data(), 1, m_sram.size(), file);
    fclose(file);
    m_sramDirty = false;
}

void Cartridge::loadBattery() {
    if (m_sram.empty()) return;

    FILE* file = fopen(m_savePath.string().c_str(), "rb");
    if (!file) return;

    fread(m_sram.data(), 1, m_sram.size(), file);
    fclose(file);
}

void Cartridge::saveState(Buffer* buf) {
    buffer_write(buf, m_banks.data(), m_banks.size());
    buffer_write(buf, &m_sramEnabled, sizeof(m_sramEnabled));

    u32 sramSize = static_cast<u32>(m_sram.size());
    buffer_write(buf, &sramSize, sizeof(sramSize));
    if (sramSize) buffer_write(buf, m_sram.data(), sramSize);
}

void Cartridge::loadState(Buffer* buf) {
    buffer_read(buf, m_banks.data(), m_banks.size());
    buffer_read(buf, &m_sramEnabled, sizeof(m_sramEnabled));

    u32 sramSize = 0;
    buffer_read(buf, &sramSize, sizeof(sramSize));
    if (sramSize) {
        m_sram.resize(sramSize);
        buffer_read(buf, m_sram.data(), sramSize);
    }
}

} // namespace md

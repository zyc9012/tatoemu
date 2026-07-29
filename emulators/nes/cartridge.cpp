#include "cartridge.h"
#include "cpu.h"
#include "ppu.h"
#include "../../utilities/zip_reader.h"
#include <set>
#include "mappers/mapper000.h"
#include "mappers/mapper001.h"
#include "mappers/mapper002.h"
#include "mappers/mapper003.h"
#include "mappers/mapper004.h"
#include "mappers/mapper005.h"
#include "mappers/mapper009.h"
#include "mappers/mapper010.h"
#include "mappers/mapper019.h"
#include "mappers/mapper023.h"
#include "mappers/mapper024.h"
#include "mappers/mapper025.h"
#include "mappers/mapper026.h"
#include "mappers/mapper069.h"
#include "mappers/mapper073.h"
#include "mappers/mapper074.h"
#include "mappers/mapper162.h"
#include "mappers/mapper163.h"
#include "mappers/mapper164.h"
#include "mappers/mapper178.h"
#include <algorithm>
#include <cstring>

namespace nes {

Cartridge::Cartridge()
    : m_cpu(nullptr)
    , m_ppu(nullptr)
    , m_mapperNumber(0)
    , m_subMapper(0)
    , m_prgBanks(0)
    , m_chrBanks(0)
    , m_mirrorMode(MirrorMode::HORIZONTAL)
    , m_hasBattery(false)
    , m_hasTrainer(false)
    , m_isNES20(false)
    , m_loaded(false) {
}

Cartridge::~Cartridge() {
    if (m_hasBattery && m_loaded) {
        saveBattery();
    }
}

bool Cartridge::load(const fs::path& filename) {
    fs::path ext = filename.extension();
    std::vector<u8> romData;

    if (ext == ".zip") {
        // Handle ZIP files
        util::ZipReader zip;
        if (!zip.open(filename)) {
            log_error("Failed to open ZIP file: %s", filename.string().c_str());
            return false;
        }

        // Find and extract NES ROM file from ZIP
        std::string romFilename;
        std::set<std::string> extensions = {".nes"};

        if (!zip.findAndExtractFile(extensions, romData, romFilename, true)) {
            zip.close();
            return false;
        }

        zip.close();
        log_info("Extracted %s from ZIP", romFilename.c_str());
    } else {
        // Handle regular files
        FILE* file = fopen(filename.string().c_str(), "rb");
        if (!file) {
            log_error("Failed to open ROM file: %s", filename.string().c_str());
            return false;
        }

        // Read entire file
        fseek(file, 0, SEEK_END);
        size_t file_size = ftell(file);
        fseek(file, 0, SEEK_SET);
        romData.resize(file_size);
        fread(romData.data(), 1, file_size, file);
        fclose(file);
    }
    
    if (romData.size() < INES_HEADER_SIZE) {
        log_error("ROM file too small");
        return false;
    }
    
    // Parse iNES header
    if (!parseINES(romData)) {
        return false;
    }
    
    m_romFilename = filename;
    m_title = filename.stem().string();
    
    // Create mapper
    createMapper();
    
    if (!m_mapper) {
        log_error("Unsupported mapper: %d (%d)", m_mapperNumber, m_subMapper);
        return false;
    }
    
    // Load battery-backed RAM if present
    if (m_hasBattery) {
        loadBattery();
    }
    
    m_loaded = true;
    m_mapper->setBaseMirrorMode(m_mirrorMode);
    m_mapper->reset();
    
    log_info("Loaded ROM: %s", m_title.c_str());
    log_info("  Format: %s", m_isNES20 ? "NES 2.0" : "iNES");
    log_info("  Mapper: %d (%d)", m_mapperNumber, m_subMapper);
    log_info("  PRG ROM: %d x 16KB", static_cast<int>(m_prgBanks));
    log_info("  CHR ROM: %d x 8KB", static_cast<int>(m_chrBanks));
    
    // Print RAM sizes
    size_t prgRamSize = m_prgRam.size();
    if (prgRamSize > 0) {
        if (prgRamSize >= 1024) {
            log_info_nn("  PRG RAM: %dKB", static_cast<int>(prgRamSize / 1024));
        } else {
            log_info_nn("  PRG RAM: %d bytes", static_cast<int>(prgRamSize));
        }
        if (m_hasBattery) {
            log_info_nn(" (battery-backed)");
        }
        log_info("");
    }
    
    // CHR RAM is only allocated if there's no CHR ROM
    if (m_chrBanks == 0) {
        size_t chrRamSize = m_chrRom.size();
        if (chrRamSize > 0) {
            if (chrRamSize >= 1024) {
                log_info("  CHR RAM: %dKB", static_cast<int>(chrRamSize / 1024));
            } else {
                log_info("  CHR RAM: %d bytes", static_cast<int>(chrRamSize));
            }
        }
    }
    
    log_info_nn("  Mirroring: ");
    switch (m_mirrorMode) {
        case MirrorMode::HORIZONTAL:
            log_info_nn("Horizontal");
            break;
        case MirrorMode::VERTICAL:
            log_info_nn("Vertical");
            break;
        case MirrorMode::SINGLE_SCREEN_A:
            log_info_nn("Single Screen A");
            break;
        case MirrorMode::SINGLE_SCREEN_B:
            log_info_nn("Single Screen B");
            break;
        case MirrorMode::FOUR_SCREEN:
            log_info_nn("Four Screen");
            break;
        default:
            log_info_nn("Unknown");
            break;
    }
    log_info("");
    log_info("  Battery: %s", m_hasBattery ? "Yes" : "No");
    log_info("  Trainer: %s", m_hasTrainer ? "Yes" : "No");
    
    return true;
}

bool Cartridge::parseINES(const std::vector<u8>& data) {
    // Check for "NES\x1A" magic bytes
    if (data[0] != 'N' || data[1] != 'E' || data[2] != 'S' || data[3] != 0x1A) {
        log_error("Invalid iNES header");
        return false;
    }
    
    // Check if this is NES 2.0 format
    // NES 2.0 is detected by: (Byte7 & 0x0C) == 0x08
    u8 flags7 = data[7];
    m_isNES20 = ((flags7 & 0x0C) == 0x08);
    
    u8 prgCount = data[4];
    u8 chrCount = data[5];
    u8 flags6 = data[6];
    
    // Mapper number
    if (m_isNES20) {
        // NES 2.0: Mapper = ((Byte8 & 0x0F) << 8) | (Byte7 & 0xF0) | (Byte6 >> 4)
        u8 byte8 = data[8];
        m_mapperNumber = ((byte8 & 0x0F) << 8) | (flags7 & 0xF0) | (flags6 >> 4);
        m_subMapper = (byte8 & 0xF0) >> 4;
    } else {
        // iNES: Mapper = (Byte7 & 0xF0) | (Byte6 >> 4)
        m_mapperNumber = (flags7 & 0xF0) | (flags6 >> 4);
        m_subMapper = 0;
    }
    
    // Mirroring
    if (flags6 & 0x08) {
        m_mirrorMode = MirrorMode::FOUR_SCREEN;
    } else if (flags6 & 0x01) {
        m_mirrorMode = MirrorMode::VERTICAL;
    } else {
        m_mirrorMode = MirrorMode::HORIZONTAL;
    }
    
    // Battery-backed RAM
    m_hasBattery = (flags6 & 0x02) != 0;
    
    // Trainer
    m_hasTrainer = (flags6 & 0x04) != 0;
    
    // Calculate PRG ROM size
    size_t prgSize;
    if (m_isNES20) {
        u8 byte9 = data[9];
        if ((byte9 & 0x0F) == 0x0F) {
            // Exponential format: size = (2^exponent) * (multiplier * 2 + 1)
            u8 exponent = prgCount >> 2;
            u8 multiplier = prgCount & 0x03;
            if (exponent > 60) {
                log_error("Unsupported PRG ROM size (exponent too large)");
                return false;
            }
            u64 multiplierValue = multiplier * 2 + 1;
            u64 size = multiplierValue * (static_cast<u64>(1) << exponent);
            if (size > 0xFFFFFFFF) {
                log_error("Unsupported PRG ROM size (too large)");
                return false;
            }
            prgSize = static_cast<size_t>(size);
            size_t banks = (prgSize + PRG_ROM_BANK_SIZE - 1) / PRG_ROM_BANK_SIZE;
            m_prgBanks = (banks > 255) ? 255 : static_cast<u8>(banks);
        } else {
            // Linear format: size = (((Byte9 & 0x0F) << 8) | PrgCount) * 0x4000
            u16 banks = ((byte9 & 0x0F) << 8) | prgCount;
            prgSize = banks * PRG_ROM_BANK_SIZE;
            m_prgBanks = (banks > 255) ? 255 : static_cast<u8>(banks);
        }
    } else {
        // iNES format
        if (prgCount == 0) {
            prgSize = 256 * PRG_ROM_BANK_SIZE;  // 0 means 256 banks
            m_prgBanks = 255;  // Cap at 255 for display (actual size is correct)
        } else {
            prgSize = prgCount * PRG_ROM_BANK_SIZE;
            m_prgBanks = prgCount;
        }
    }
    
    // Calculate CHR ROM size
    size_t chrSize;
    if (m_isNES20) {
        u8 byte9 = data[9];
        if ((byte9 & 0xF0) == 0xF0) {
            // Exponential format: size = (2^exponent) * (multiplier * 2 + 1)
            u8 exponent = chrCount >> 2;
            u8 multiplier = chrCount & 0x03;
            if (exponent > 60) {
                log_error("Unsupported CHR ROM size (exponent too large)");
                return false;
            }
            u64 multiplierValue = multiplier * 2 + 1;
            u64 size = multiplierValue * (static_cast<u64>(1) << exponent);
            if (size > 0xFFFFFFFF) {
                log_error("Unsupported CHR ROM size (too large)");
                return false;
            }
            chrSize = static_cast<size_t>(size);
            size_t banks = (chrSize + CHR_ROM_BANK_SIZE - 1) / CHR_ROM_BANK_SIZE;
            m_chrBanks = (banks > 255) ? 255 : static_cast<u8>(banks);
        } else {
            // Linear format: size = (((Byte9 & 0xF0) << 4) | ChrCount) * 0x2000
            u16 banks = ((byte9 & 0xF0) << 4) | chrCount;
            chrSize = banks * CHR_ROM_BANK_SIZE;
            m_chrBanks = (banks > 255) ? 255 : static_cast<u8>(banks);
        }
    } else {
        // iNES format
        chrSize = chrCount * CHR_ROM_BANK_SIZE;
        m_chrBanks = chrCount;
    }
    
    // Calculate PRG RAM sizes (NES 2.0)
    size_t prgRamSize = 0x2000;  // Default 8KB
    if (m_isNES20) {
        u8 byte10 = data[10];
        // Work RAM (non-battery): lower nibble
        u8 workRamExp = byte10 & 0x0F;
        size_t workRamSize = 0;
        if (workRamExp > 0) {
            workRamSize = 128 * (static_cast<size_t>(1) << (workRamExp - 1));
        }
        // Save RAM (battery): upper nibble
        u8 saveRamExp = (byte10 & 0xF0) >> 4;
        size_t saveRamSize = 0;
        if (saveRamExp > 0) {
            saveRamSize = 128 * (static_cast<size_t>(1) << (saveRamExp - 1));
        }
        // Use the larger of the two, or default to 8KB if both are 0
        prgRamSize = (workRamSize > 0 || saveRamSize > 0) ? 
                     std::max(workRamSize, saveRamSize) : 0x2000;
    }
    
    // Calculate CHR RAM sizes (NES 2.0)
    size_t chrRamSize = 0;
    if (m_isNES20) {
        u8 byte11 = data[11];
        // CHR RAM: lower nibble
        u8 chrRamExp = byte11 & 0x0F;
        if (chrRamExp > 0) {
            chrRamSize = 128 * (static_cast<size_t>(1) << (chrRamExp - 1));
        }
        // Save CHR RAM: upper nibble (for battery-backed CHR RAM)
        u8 saveChrRamExp = (byte11 & 0xF0) >> 4;
        if (saveChrRamExp > 0) {
            size_t saveChrRamSize = 128 * (static_cast<size_t>(1) << (saveChrRamExp - 1));
            if (saveChrRamSize > chrRamSize) {
                chrRamSize = saveChrRamSize;
            }
        }
    }
    
    // Calculate data offsets
    size_t offset = INES_HEADER_SIZE;
    if (m_hasTrainer) {
        // Load trainer data (512 bytes, mapped at $7000-$71FF)
        if (offset + 512 > data.size()) {
            log_error("Trainer data too small");
            return false;
        }
        m_trainer.assign(data.begin() + offset, data.begin() + offset + 512);
        offset += 512;
    }
    
    // PRG ROM
    if (offset + prgSize > data.size()) {
        log_error("PRG ROM data too small");
        return false;
    }
    m_prgRom.assign(data.begin() + offset, data.begin() + offset + prgSize);
    offset += prgSize;
    
    // CHR ROM (or allocate CHR RAM)
    if (chrSize > 0) {
        if (offset + chrSize > data.size()) {
            log_error("CHR ROM data too small");
            return false;
        }
        m_chrRom.assign(data.begin() + offset, data.begin() + offset + chrSize);
    } else {
        // Allocate CHR RAM (use NES 2.0 size if specified, otherwise default 8KB)
        size_t allocatedChrRamSize = (chrRamSize > 0) ? chrRamSize : CHR_ROM_BANK_SIZE;
        m_chrRom.resize(allocatedChrRamSize, 0);
    }
    
    // Allocate PRG RAM
    m_prgRam.resize(prgRamSize, 0);
    
    // Load trainer into PRG RAM at offset 0x1000 ($7000-$71FF in $6000-$7FFF range)
    if (m_hasTrainer && m_prgRam.size() >= 0x2000 && m_trainer.size() > 0) {
        std::copy(m_trainer.begin(), m_trainer.end(), m_prgRam.begin() + 0x1000);
    }
    
    return true;
}

void Cartridge::createMapper() {
    switch (m_mapperNumber) {
        case 0:
            m_mapper = std::make_unique<Mapper000>(this);
            break;
        case 1:
            m_mapper = std::make_unique<Mapper001>(this);
            break;
        case 2:
            m_mapper = std::make_unique<Mapper002>(this);
            break;
        case 3:
            m_mapper = std::make_unique<Mapper003>(this);
            break;
        case 4:
            m_mapper = std::make_unique<Mapper004>(this);
            break;
        case 5:
            m_mapper = std::make_unique<Mapper005>(this);
            break;
        case 9:
            m_mapper = std::make_unique<Mapper009>(this);
            break;
        case 10:
            m_mapper = std::make_unique<Mapper010>(this);
            break;
        case 19:
            m_mapper = std::make_unique<Mapper019>(this);
            break;
        case 23:
            m_mapper = std::make_unique<Mapper023>(this);
            break;
        case 24:
            m_mapper = std::make_unique<Mapper024>(this);
            break;
        case 26:
            m_mapper = std::make_unique<Mapper026>(this);
            break;
        case 25:
            m_mapper = std::make_unique<Mapper025>(this);
            break;
        case 69:
            m_mapper = std::make_unique<Mapper069>(this);
            break;
        case 73:
            m_mapper = std::make_unique<Mapper073>(this);
            break;
        case 74:
            m_mapper = std::make_unique<Mapper074>(this);
            break;
        case 162:
            m_mapper = std::make_unique<Mapper162>(this);
            break;
        case 163:
            m_mapper = std::make_unique<Mapper163>(this);
            break;
        case 164:
            m_mapper = std::make_unique<Mapper164>(this);
            break;
        case 178:
            m_mapper = std::make_unique<Mapper178>(this);
            break;
        default:
            m_mapper = nullptr;
            break;
    }
}

void Cartridge::reset() {
    m_mapper->reset();
}

u32 Cartridge::getCpuCycles() const {
    return m_cpu->getCycles();
}

u8 Cartridge::cpuRead(u16 address) {
    return m_mapper->cpuRead(address);
}

void Cartridge::cpuWrite(u16 address, u8 value) {
    m_mapper->cpuWrite(address, value);
}

u8 Cartridge::readCHR(u16 address) {
    return m_mapper->readCHR(address);
}

void Cartridge::writeCHR(u16 address, u8 value) {
    m_mapper->writeCHR(address, value);
}

u8 Cartridge::readCIRAM(u16 address) const {
    return m_ppu->readCIRAM(address);
}

void Cartridge::writeCIRAM(u16 address, u8 value) {
    m_ppu->writeCIRAM(address, value);
}

bool Cartridge::readNametable(u16 address, u8& value) {
    return m_mapper->readNametable(address, value);
}

bool Cartridge::writeNametable(u16 address, u8 value) {
    return m_mapper->writeNametable(address, value);
}

MirrorMode Cartridge::getMirrorMode() const {
    return m_mapper->getMirrorMode();
}

void Cartridge::scanlineCounter() {
    m_mapper->scanlineCounter();
}

void Cartridge::clockAudio() {
    m_mapper->clockAudio();
}

float Cartridge::getExpansionAudio() const {
    return m_mapper->getAudioOutput();
}

bool Cartridge::hasExpansionAudio() const {
    return m_mapper->hasExpansionAudio();
}

void Cartridge::saveBattery() const {
    if (!m_hasBattery || m_prgRam.empty()) {
        return;
    }
    
    fs::path savePath = m_romFilename;
    savePath.replace_extension(".sav");
    
    FILE* file = fopen(savePath.string().c_str(), "wb");
    if (file) {
        fwrite(m_prgRam.data(), 1, m_prgRam.size(), file);
        fclose(file);
        log_info("Battery data saved to: %s", savePath.string().c_str());
    }
}

void Cartridge::loadBattery() {
    if (!m_hasBattery || m_prgRam.empty()) {
        return;
    }
    
    fs::path savePath = m_romFilename;
    savePath.replace_extension(".sav");
    
    FILE* file = fopen(savePath.string().c_str(), "rb");
    if (file) {
        fread(m_prgRam.data(), 1, m_prgRam.size(), file);
        fclose(file);
        log_info("Battery data loaded from: %s", savePath.string().c_str());
    }
}

template <typename Visit>
void Cartridge::visitState(Visit visit) {
    visit(m_mirrorMode);
    
    // PRG RAM, sized by the state rather than by the cartridge
    u32 prgRamSize = static_cast<u32>(m_prgRam.size());
    visit(prgRamSize);
    if constexpr (Visit::loading) {
        m_prgRam.resize(prgRamSize);
    }
    visit.bytes(m_prgRam.data(), m_prgRam.size());
    
    // Only CHR RAM belongs in a state; CHR ROM comes from the file
    if (m_chrBanks == 0) {
        u32 chrSize = static_cast<u32>(m_chrRom.size());
        visit(chrSize);
        if constexpr (Visit::loading) {
            m_chrRom.resize(chrSize);
        }
        visit.bytes(m_chrRom.data(), m_chrRom.size());
    }
}

void Cartridge::saveState(Buffer* buf) {
    visitState(StateWriter{buf});
    
    // The mapper is reached through a virtual call, so it walks itself.
    m_mapper->saveState(buf);
}

void Cartridge::loadState(Buffer* buf) {
    visitState(StateReader{buf});
    
    m_mapper->loadState(buf);
}

} // namespace nes

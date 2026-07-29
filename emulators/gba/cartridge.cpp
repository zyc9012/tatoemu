#include "cartridge.h"
#include "eeprom.h"
#include "../../utilities/zip_reader.h"
#include <algorithm>
#include <cstring>
#include <cstdio>

namespace gba {

Cartridge::Cartridge() {}
Cartridge::~Cartridge() {
    if (m_loaded && m_saveType != SaveType::NONE) {
        saveBattery();
    }
}

bool Cartridge::load(const fs::path& filename) {
    std::vector<u8> data;

    fs::path ext = filename.extension();
    if (ext == ".zip") {
        util::ZipReader zip;
        if (!zip.open(filename)) {
            log_error("Failed to open ZIP: %s", filename.string().c_str());
            return false;
        }
        auto files = zip.getFileList();
        for (const auto& f : files) {
            fs::path p = f;
            if (p.extension() == ".gba") {
                zip.extractFile(f, data);
                break;
            }
        }
        zip.close();
        if (data.empty()) {
            log_error("No GBA ROM found in ZIP");
            return false;
        }
    } else {
        FILE* file = fopen(filename.string().c_str(), "rb");
        if (!file) {
            log_error("Failed to open ROM: %s", filename.string().c_str());
            return false;
        }
        fseek(file, 0, SEEK_END);
        long size = ftell(file);
        fseek(file, 0, SEEK_SET);
        data.resize(static_cast<size_t>(size));
        fread(data.data(), 1, size, file);
        fclose(file);
    }

    if (data.size() < 0xC0) {
        log_error("ROM too small");
        return false;
    }

    m_rom = std::move(data);
    // Ensure ROM size is power of 2 (for mirroring)
    u32 roundedSize = 1;
    while (roundedSize < m_rom.size()) roundedSize <<= 1;
    m_rom.resize(roundedSize, 0xFF);

    parseHeader();
    detectSaveType();

    m_savePath = filename;
    m_savePath.replace_extension(".sav");

    // Initialize save memory
    switch (m_saveType) {
        case SaveType::SRAM:
            m_sram.resize(0x8000, 0xFF); // 32KB
            break;
        case SaveType::FLASH_64K:
            m_sram.resize(0x10000, 0xFF); // 64KB
            break;
        case SaveType::FLASH_128K:
            m_sram.resize(0x20000, 0xFF); // 128KB
            break;
        case SaveType::EEPROM_512:
            m_eeprom.init(false); // 512 bytes
            break;
        case SaveType::EEPROM_8K:
            m_eeprom.init(true); // 8KB
            break;
        default:
            break;
    }

    loadBattery();

    m_loaded = true;
    log_info("Loaded ROM: %s", m_title.c_str());
    log_info("  Game Code: %s", m_gameCode.c_str());
    log_info("  Save Type: %s", (m_saveType == SaveType::NONE) ? "None" :
                                (m_saveType == SaveType::SRAM) ? "SRAM" :
                                (m_saveType == SaveType::FLASH_64K) ? "Flash 64K" :
                                (m_saveType == SaveType::FLASH_128K) ? "Flash 128K" :
                                (m_saveType == SaveType::EEPROM_512) ? "EEPROM 512B" :
                                (m_saveType == SaveType::EEPROM_8K) ? "EEPROM 8KB" : "Unknown");
    return true;
}

void Cartridge::parseHeader() {
    // Title at offset 0xA0, 12 bytes
    char title[13] = {};
    std::memcpy(title, &m_rom[0xA0], 12);
    m_title = title;
    // Remove trailing spaces
    while (!m_title.empty() && m_title.back() == ' ') m_title.pop_back();
    if (m_title.empty()) m_title = "Unknown";

    // Game code at offset 0xAC, 4 bytes
    char code[5] = {};
    std::memcpy(code, &m_rom[0xAC], 4);
    m_gameCode = code;
}

void Cartridge::detectSaveType() {
    // Search ROM for save type strings
    const char* sramStr = "SRAM_V";
    const char* sramFStr = "SRAM_F_V";
    const char* flash512Str = "FLASH512_V";
    const char* flash1mStr = "FLASH1M_V";
    const char* flashStr = "FLASH_V";
    const char* eepromStr = "EEPROM_V";

    std::string romStr(reinterpret_cast<const char*>(m_rom.data()), m_rom.size());

    if (romStr.find(flash1mStr) != std::string::npos) {
        m_saveType = SaveType::FLASH_128K;
    } else if (romStr.find(flash512Str) != std::string::npos) {
        m_saveType = SaveType::FLASH_64K;
    } else if (romStr.find(flashStr) != std::string::npos) {
        m_saveType = SaveType::FLASH_64K;
    } else if (romStr.find(sramFStr) != std::string::npos || romStr.find(sramStr) != std::string::npos) {
        m_saveType = SaveType::SRAM;
    } else if (romStr.find(eepromStr) != std::string::npos) {
        // EEPROM size depends on the ROM size
        // ROMs >= 16MB use 8K EEPROM, smaller use 512B
        bool use8K = m_rom.size() >= 0x1000000;
        m_saveType = use8K ? SaveType::EEPROM_8K : SaveType::EEPROM_512;
    } else {
        m_saveType = SaveType::NONE;
    }
}

u8 Cartridge::readSave(u32 address) const {
    switch (m_saveType) {
        case SaveType::SRAM:
            return m_sram[address & 0x7FFF];
        case SaveType::FLASH_64K:
            if (m_flashIdMode) {
                if (address == 0) return 0x32; // Panasonic manufacturer ID
                if (address == 1) return 0x1B; // Device ID
                return 0;
            }
            return m_sram[address & 0xFFFF];
        case SaveType::FLASH_128K:
            if (m_flashIdMode) {
                if (address == 0) return 0x62; // Sanyo manufacturer ID
                if (address == 1) return 0x13; // Device ID
                return 0;
            }
            return m_sram[(m_flashBank * 0x10000) + (address & 0xFFFF)];
        default:
            return 0xFF;
    }
}

void Cartridge::writeSave(u32 address, u8 value) {
    switch (m_saveType) {
        case SaveType::SRAM:
            m_sram[address & 0x7FFF] = value;
            return;
        case SaveType::FLASH_64K:
        case SaveType::FLASH_128K: {
            // Flash state machine
            switch (m_flashState) {
                case FlashState::READY:
                    if (address == 0x5555 && value == 0xAA)
                        m_flashState = FlashState::CMD1;
                    break;
                case FlashState::CMD1:
                    if (address == 0x2AAA && value == 0x55)
                        m_flashState = FlashState::CMD2;
                    else
                        m_flashState = FlashState::READY;
                    break;
                case FlashState::CMD2:
                    if (m_flashEraseMode) {
                        // Second CMD1→CMD2 after erase setup (0x80)
                        if (address == 0x5555 && value == 0x10) {
                            // Erase entire chip
                            std::memset(m_sram.data(), 0xFF, m_sram.size());
                        } else if (value == 0x30) {
                            // Erase 4KB sector at the written address
                            u32 sector = address & 0xF000;
                            u32 offset = (m_saveType == SaveType::FLASH_128K) ? m_flashBank * 0x10000 : 0;
                            std::memset(&m_sram[offset + sector], 0xFF, 0x1000);
                        }
                        m_flashEraseMode = false;
                        m_flashState = FlashState::READY;
                    } else if (address == 0x5555) {
                        switch (value) {
                            case 0x90: m_flashIdMode = true; break;
                            case 0xF0: m_flashIdMode = false; break;
                            case 0x80:
                                m_flashEraseMode = true;
                                m_flashState = FlashState::READY;
                                return;
                            case 0xA0: m_flashState = FlashState::WRITE; return;
                            case 0xB0:
                                if (m_saveType == SaveType::FLASH_128K) {
                                    m_flashState = FlashState::BANK_SELECT;
                                    return;
                                }
                                break;
                        }
                        m_flashState = FlashState::READY;
                    } else {
                        m_flashState = FlashState::READY;
                    }
                    break;
                case FlashState::WRITE: {
                    u32 offset = (m_saveType == SaveType::FLASH_128K) ? m_flashBank * 0x10000 : 0;
                    m_sram[offset + (address & 0xFFFF)] = value;
                    m_flashState = FlashState::READY;
                    break;
                }
                case FlashState::BANK_SELECT:
                    if (address == 0) m_flashBank = value & 1;
                    m_flashState = FlashState::READY;
                    break;
                default:
                    m_flashState = FlashState::READY;
                    break;
            }
            break;
        }
        default:
            break;
    }
}

u16 Cartridge::readEEPROM() {
    if (hasEEPROM() && m_eeprom.isInitialized()) {
        return m_eeprom.read();
    }
    return 1; // Ready/idle state
}

void Cartridge::writeEEPROM(u16 value, u32 writeSize) {
    if (!hasEEPROM()) {
        // Auto-detect EEPROM if not already detected
        if (m_saveType == SaveType::NONE) {
            // Default to 512B, will auto-upgrade to 8K if needed
            m_saveType = SaveType::EEPROM_512;
            m_eeprom.init(false);
            log_info("Auto-detected EEPROM save type");
        } else {
            return;
        }
    }
    
    if (!m_eeprom.isInitialized()) {
        m_eeprom.init(m_saveType == SaveType::EEPROM_8K);
    }
    
    m_eeprom.write(value, writeSize);
}

void Cartridge::saveBattery() const {
    if (hasEEPROM() && m_eeprom.isInitialized()) {
        // Save EEPROM data
        FILE* file = fopen(m_savePath.string().c_str(), "wb");
        if (file) {
            fwrite(m_eeprom.getData(), 1, m_eeprom.getSize(), file);
            fclose(file);
            log_info("Battery saved (EEPROM): %s", m_savePath.string().c_str());
        }
    } else if (!m_sram.empty() && !m_savePath.empty()) {
        FILE* file = fopen(m_savePath.string().c_str(), "wb");
        if (file) {
            fwrite(m_sram.data(), 1, m_sram.size(), file);
            fclose(file);
            log_info("Battery saved: %s", m_savePath.string().c_str());
        }
    }
}

void Cartridge::loadBattery() {
    if (hasEEPROM() && m_eeprom.isInitialized()) {
        // Load EEPROM data
        FILE* file = fopen(m_savePath.string().c_str(), "rb");
        if (file) {
            fseek(file, 0, SEEK_END);
            long size = ftell(file);
            fseek(file, 0, SEEK_SET);
            
            // Auto-upgrade to 8K if file is larger than 512 bytes
            if (size > 0x200 && !m_eeprom.is8K()) {
                m_eeprom.upgradeTo8K();
                m_saveType = SaveType::EEPROM_8K;
            }
            
            fread(m_eeprom.getData(), 1, std::min(static_cast<long>(m_eeprom.getSize()), size), file);
            fclose(file);
            log_info("Battery loaded (EEPROM): %s", m_savePath.string().c_str());
        }
    } else if (!m_sram.empty() && !m_savePath.empty()) {
        FILE* file = fopen(m_savePath.string().c_str(), "rb");
        if (file) {
            fread(m_sram.data(), 1, m_sram.size(), file);
            fclose(file);
            log_info("Battery loaded: %s", m_savePath.string().c_str());
        }
    }
}

template <typename Visit>
void Cartridge::visitState(Visit visit) {
    // The ROM size is recorded but not acted on; the ROM comes from the file.
    u32 romSize = static_cast<u32>(m_rom.size());
    visit(romSize);
    visit(m_saveType);
    
    u32 sramSize = static_cast<u32>(m_sram.size());
    visit(sramSize);
    if (sramSize > 0) {
        if constexpr (Visit::loading) {
            m_sram.resize(sramSize);
        }
        visit.bytes(m_sram.data(), sramSize);
    }
    visit(m_flashState);
    visit(m_flashBank);
    visit(m_flashIdMode);
    
    // EEPROM state
    m_eeprom.visitState(visit);
}

void Cartridge::saveState(Buffer* buf) {
    visitState(StateWriter{buf});
}

void Cartridge::loadState(Buffer* buf) {
    visitState(StateReader{buf});
}

} // namespace gba

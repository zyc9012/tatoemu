#include "cartridge.h"
#include "cpu.h"
#include "ppu.h"
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
#include <iostream>
#include <fstream>
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
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open ROM file: " << filename << std::endl;
        return false;
    }
    
    // Read entire file
    std::vector<u8> data((std::istreambuf_iterator<char>(file)),
                          std::istreambuf_iterator<char>());
    file.close();
    
    if (data.size() < INES_HEADER_SIZE) {
        std::cerr << "ROM file too small" << std::endl;
        return false;
    }
    
    // Parse iNES header
    if (!parseINES(data)) {
        return false;
    }
    
    m_romFilename = filename;
    m_title = filename.stem().string();
    
    // Create mapper
    createMapper();
    
    if (!m_mapper) {
        std::cerr << "Unsupported mapper: " << m_mapperNumber;
        if (m_isNES20 && m_subMapper > 0) {
            std::cerr << "." << static_cast<int>(m_subMapper);
        }
        std::cerr << std::endl;
        return false;
    }
    
    // Load battery-backed RAM if present
    if (m_hasBattery) {
        loadBattery();
    }
    
    m_loaded = true;
    m_mapper->setBaseMirrorMode(m_mirrorMode);
    m_mapper->reset();
    
    std::cout << "Loaded ROM: " << m_title << std::endl;
    std::cout << "  Format: " << (m_isNES20 ? "NES 2.0" : "iNES") << std::endl;
    std::cout << "  Mapper: " << static_cast<int>(m_mapperNumber);
    if (m_isNES20 && m_subMapper > 0) {
        std::cout << "." << static_cast<int>(m_subMapper);
    }
    std::cout << std::endl;
    std::cout << "  PRG ROM: " << static_cast<int>(m_prgBanks) << " x 16KB" << std::endl;
    std::cout << "  CHR ROM: " << static_cast<int>(m_chrBanks) << " x 8KB" << std::endl;
    
    // Print RAM sizes
    size_t prgRamSize = m_prgRam.size();
    if (prgRamSize > 0) {
        if (prgRamSize >= 1024) {
            std::cout << "  PRG RAM: " << (prgRamSize / 1024) << "KB";
        } else {
            std::cout << "  PRG RAM: " << prgRamSize << " bytes";
        }
        if (m_hasBattery) {
            std::cout << " (battery-backed)";
        }
        std::cout << std::endl;
    }
    
    // CHR RAM is only allocated if there's no CHR ROM
    if (m_chrBanks == 0) {
        size_t chrRamSize = m_chrRom.size();
        if (chrRamSize > 0) {
            if (chrRamSize >= 1024) {
                std::cout << "  CHR RAM: " << (chrRamSize / 1024) << "KB";
            } else {
                std::cout << "  CHR RAM: " << chrRamSize << " bytes";
            }
            std::cout << std::endl;
        }
    }
    
    std::cout << "  Mirroring: ";
    switch (m_mirrorMode) {
        case MirrorMode::HORIZONTAL:
            std::cout << "Horizontal";
            break;
        case MirrorMode::VERTICAL:
            std::cout << "Vertical";
            break;
        case MirrorMode::SINGLE_SCREEN_A:
            std::cout << "Single Screen A";
            break;
        case MirrorMode::SINGLE_SCREEN_B:
            std::cout << "Single Screen B";
            break;
        case MirrorMode::FOUR_SCREEN:
            std::cout << "Four Screen";
            break;
        default:
            std::cout << "Unknown";
            break;
    }
    std::cout << std::endl;
    std::cout << "  Battery: " << (m_hasBattery ? "Yes" : "No") << std::endl;
    std::cout << "  Trainer: " << (m_hasTrainer ? "Yes" : "No") << std::endl;
    
    return true;
}

bool Cartridge::parseINES(const std::vector<u8>& data) {
    // Check for "NES\x1A" magic bytes
    if (data[0] != 'N' || data[1] != 'E' || data[2] != 'S' || data[3] != 0x1A) {
        std::cerr << "Invalid iNES header" << std::endl;
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
                std::cerr << "Unsupported PRG ROM size (exponent too large)" << std::endl;
                return false;
            }
            u64 multiplierValue = multiplier * 2 + 1;
            u64 size = multiplierValue * (static_cast<u64>(1) << exponent);
            if (size > 0xFFFFFFFF) {
                std::cerr << "Unsupported PRG ROM size (too large)" << std::endl;
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
                std::cerr << "Unsupported CHR ROM size (exponent too large)" << std::endl;
                return false;
            }
            u64 multiplierValue = multiplier * 2 + 1;
            u64 size = multiplierValue * (static_cast<u64>(1) << exponent);
            if (size > 0xFFFFFFFF) {
                std::cerr << "Unsupported CHR ROM size (too large)" << std::endl;
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
            std::cerr << "Trainer data too small" << std::endl;
            return false;
        }
        m_trainer.assign(data.begin() + offset, data.begin() + offset + 512);
        offset += 512;
    }
    
    // PRG ROM
    if (offset + prgSize > data.size()) {
        std::cerr << "PRG ROM data too small" << std::endl;
        return false;
    }
    m_prgRom.assign(data.begin() + offset, data.begin() + offset + prgSize);
    offset += prgSize;
    
    // CHR ROM (or allocate CHR RAM)
    if (chrSize > 0) {
        if (offset + chrSize > data.size()) {
            std::cerr << "CHR ROM data too small" << std::endl;
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
    if (m_mapper) {
        m_mapper->reset();
    }
}

u32 Cartridge::getCpuCycles() const {
    if (m_cpu) {
        return m_cpu->getCycles();
    }
    return 0;
}

u8 Cartridge::cpuRead(u16 address) {
    if (m_mapper) {
        return m_mapper->cpuRead(address);
    }
    return 0;
}

void Cartridge::cpuWrite(u16 address, u8 value) {
    if (m_mapper) {
        m_mapper->cpuWrite(address, value);
    }
}

u8 Cartridge::readCHR(u16 address) {
    if (m_mapper) {
        return m_mapper->readCHR(address);
    }
    return 0;
}

void Cartridge::writeCHR(u16 address, u8 value) {
    if (m_mapper) {
        m_mapper->writeCHR(address, value);
    }
}

u8 Cartridge::readCIRAM(u16 address) const {
    if (m_ppu) {
        return m_ppu->readCIRAM(address);
    }
    return 0;
}

void Cartridge::writeCIRAM(u16 address, u8 value) {
    if (m_ppu) {
        m_ppu->writeCIRAM(address, value);
    }
}

bool Cartridge::readNametable(u16 address, u8& value) {
    if (m_mapper) {
        return m_mapper->readNametable(address, value);
    }
    return false;
}

bool Cartridge::writeNametable(u16 address, u8 value) {
    if (m_mapper) {
        return m_mapper->writeNametable(address, value);
    }
    return false;
}

MirrorMode Cartridge::getMirrorMode() const {
    if (m_mapper) {
        return m_mapper->getMirrorMode();
    }
    return m_mirrorMode;
}

void Cartridge::scanlineCounter() {
    if (m_mapper) {
        m_mapper->scanlineCounter();
    }
}

bool Cartridge::irqState() const {
    if (m_mapper) {
        return m_mapper->irqState();
    }
    return false;
}

void Cartridge::irqClear() {
    if (m_mapper) {
        m_mapper->irqClear();
    }
}

void Cartridge::clockAudio() {
    if (m_mapper) {
        m_mapper->clockAudio();
    }
}

float Cartridge::getExpansionAudio() const {
    if (m_mapper) {
        return m_mapper->getAudioOutput();
    }
    return 0.0f;
}

bool Cartridge::hasExpansionAudio() const {
    if (m_mapper) {
        return m_mapper->hasExpansionAudio();
    }
    return false;
}

void Cartridge::saveBattery() const {
    if (!m_hasBattery || m_prgRam.empty()) {
        return;
    }
    
    fs::path savePath = m_romFilename;
    savePath.replace_extension(".sav");
    
    std::ofstream file(savePath, std::ios::binary);
    if (file.is_open()) {
        file.write(reinterpret_cast<const char*>(m_prgRam.data()), m_prgRam.size());
        file.close();
        std::cout << "Battery data saved to: " << savePath << std::endl;
    }
}

void Cartridge::loadBattery() {
    if (!m_hasBattery || m_prgRam.empty()) {
        return;
    }
    
    fs::path savePath = m_romFilename;
    savePath.replace_extension(".sav");
    
    std::ifstream file(savePath, std::ios::binary);
    if (file.is_open()) {
        file.read(reinterpret_cast<char*>(m_prgRam.data()), m_prgRam.size());
        file.close();
        std::cout << "Battery data loaded from: " << savePath << std::endl;
    }
}

void Cartridge::saveState(std::ofstream& file) const {
    file.write(reinterpret_cast<const char*>(&m_mirrorMode), sizeof(m_mirrorMode));
    
    u32 prgRamSize = static_cast<u32>(m_prgRam.size());
    file.write(reinterpret_cast<const char*>(&prgRamSize), sizeof(prgRamSize));
    file.write(reinterpret_cast<const char*>(m_prgRam.data()), m_prgRam.size());
    
    u32 chrSize = static_cast<u32>(m_chrRom.size());
    file.write(reinterpret_cast<const char*>(&chrSize), sizeof(chrSize));
    file.write(reinterpret_cast<const char*>(m_chrRom.data()), m_chrRom.size());
    
    if (m_mapper) {
        m_mapper->saveState(file);
    }
}

void Cartridge::loadState(std::ifstream& file) {
    file.read(reinterpret_cast<char*>(&m_mirrorMode), sizeof(m_mirrorMode));
    
    u32 prgRamSize;
    file.read(reinterpret_cast<char*>(&prgRamSize), sizeof(prgRamSize));
    m_prgRam.resize(prgRamSize);
    file.read(reinterpret_cast<char*>(m_prgRam.data()), m_prgRam.size());
    
    u32 chrSize;
    file.read(reinterpret_cast<char*>(&chrSize), sizeof(chrSize));
    if (m_chrBanks == 0) {  // Only restore CHR RAM, not ROM
        m_chrRom.resize(chrSize);
        file.read(reinterpret_cast<char*>(m_chrRom.data()), m_chrRom.size());
    } else {
        file.seekg(chrSize, std::ios::cur);  // Skip CHR ROM in save state
    }
    
    if (m_mapper) {
        m_mapper->loadState(file);
    }
}

} // namespace nes

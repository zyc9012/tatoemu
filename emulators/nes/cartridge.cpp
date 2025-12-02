#include "cartridge.h"
#include "cpu.h"
#include "ppu.h"
#include "mappers/mapper000.h"
#include "mappers/mapper001.h"
#include "mappers/mapper002.h"
#include "mappers/mapper003.h"
#include "mappers/mapper004.h"
#include "mappers/mapper005.h"
#include "mappers/mapper010.h"
#include "mappers/mapper023.h"
#include "mappers/mapper024.h"
#include "mappers/mapper025.h"
#include "mappers/mapper073.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cstring>

namespace nes {

Cartridge::Cartridge()
    : m_cpu(nullptr)
    , m_ppu(nullptr)
    , m_mapperNumber(0)
    , m_prgBanks(0)
    , m_chrBanks(0)
    , m_mirrorMode(MirrorMode::HORIZONTAL)
    , m_hasBattery(false)
    , m_hasTrainer(false)
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
        std::cerr << "Unsupported mapper: " << static_cast<int>(m_mapperNumber) << std::endl;
        return false;
    }
    
    // Load battery-backed RAM if present
    if (m_hasBattery) {
        loadBattery();
    }
    
    m_loaded = true;
    m_mapper->reset();
    
    std::cout << "Loaded ROM: " << m_title << std::endl;
    std::cout << "  Mapper: " << static_cast<int>(m_mapperNumber) << std::endl;
    std::cout << "  PRG ROM: " << static_cast<int>(m_prgBanks) << " x 16KB" << std::endl;
    std::cout << "  CHR ROM: " << static_cast<int>(m_chrBanks) << " x 8KB" << std::endl;
    std::cout << "  Mirroring: " << (m_mirrorMode == MirrorMode::VERTICAL ? "Vertical" : "Horizontal") << std::endl;
    std::cout << "  Battery: " << (m_hasBattery ? "Yes" : "No") << std::endl;
    
    return true;
}

bool Cartridge::parseINES(const std::vector<u8>& data) {
    // Check for "NES\x1A" magic bytes
    if (data[0] != 'N' || data[1] != 'E' || data[2] != 'S' || data[3] != 0x1A) {
        std::cerr << "Invalid iNES header" << std::endl;
        return false;
    }
    
    m_prgBanks = data[4];
    m_chrBanks = data[5];
    
    u8 flags6 = data[6];
    u8 flags7 = data[7];
    
    // Mapper number
    m_mapperNumber = ((flags6 >> 4) & 0x0F) | (flags7 & 0xF0);
    
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
    
    // Calculate data offsets
    size_t offset = INES_HEADER_SIZE;
    if (m_hasTrainer) {
        offset += 512;  // Skip trainer
    }
    
    // PRG ROM
    size_t prgSize = m_prgBanks * PRG_ROM_BANK_SIZE;
    if (offset + prgSize > data.size()) {
        std::cerr << "PRG ROM data too small" << std::endl;
        return false;
    }
    m_prgRom.assign(data.begin() + offset, data.begin() + offset + prgSize);
    offset += prgSize;
    
    // CHR ROM (or allocate CHR RAM)
    if (m_chrBanks > 0) {
        size_t chrSize = m_chrBanks * CHR_ROM_BANK_SIZE;
        if (offset + chrSize > data.size()) {
            std::cerr << "CHR ROM data too small" << std::endl;
            return false;
        }
        m_chrRom.assign(data.begin() + offset, data.begin() + offset + chrSize);
    } else {
        // Allocate 8KB CHR RAM
        m_chrRom.resize(CHR_ROM_BANK_SIZE, 0);
    }
    
    // Allocate PRG RAM (8KB default)
    m_prgRam.resize(0x2000, 0);
    
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
        case 10:
            m_mapper = std::make_unique<Mapper010>(this);
            break;
        case 23:
            m_mapper = std::make_unique<Mapper023>(this);
            break;
        case 24:
            m_mapper = std::make_unique<Mapper024>(this);
            break;
        case 25:
            m_mapper = std::make_unique<Mapper025>(this);
            break;
        case 73:
            m_mapper = std::make_unique<Mapper073>(this);
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

MirrorMode Cartridge::getMirrorMode() const {
    if (m_mapper) {
        return m_mapper->getMirrorMode();
    }
    return m_mirrorMode;
}

void Cartridge::setMirrorMode(MirrorMode mode) {
    m_mirrorMode = mode;
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
        std::cout << "Battery save written to: " << savePath << std::endl;
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
        std::cout << "Battery save loaded from: " << savePath << std::endl;
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

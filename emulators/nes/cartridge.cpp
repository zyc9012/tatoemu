#include "cartridge.h"
#include "cpu.h"
#include "ppu.h"
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
        case 10:
            m_mapper = std::make_unique<Mapper010>(this);
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

// ========== Mapper 0: NROM ==========

Mapper000::Mapper000(Cartridge* cartridge) : Mapper(cartridge) {
}

void Mapper000::reset() {
}

u8 Mapper000::cpuRead(u16 address) {
    if (address >= 0x6000 && address < 0x8000) {
        // PRG RAM
        return m_cartridge->getPRGRAM()[address & 0x1FFF];
    } else if (address >= 0x8000) {
        // PRG ROM - mirror if only 16KB
        const auto& prg = m_cartridge->getPRG();
        return prg[(address - 0x8000) % prg.size()];
    }
    return 0;
}

void Mapper000::cpuWrite(u16 address, u8 value) {
    if (address >= 0x6000 && address < 0x8000) {
        // PRG RAM
        m_cartridge->getPRGRAM()[address & 0x1FFF] = value;
    }
    // ROM writes are ignored
}

u8 Mapper000::readCHR(u16 address) {
    return m_cartridge->getCHR()[address & 0x1FFF];
}

void Mapper000::writeCHR(u16 address, u8 value) {
    // CHR RAM (if no CHR ROM)
    if (m_cartridge->getCHR().size() == CHR_ROM_BANK_SIZE) {
        m_cartridge->getCHR()[address & 0x1FFF] = value;
    }
}

void Mapper000::saveState(std::ofstream& file) const {
    (void)file;  // No mapper state
}

void Mapper000::loadState(std::ifstream& file) {
    (void)file;  // No mapper state
}

// ========== Mapper 1: MMC1 ==========

Mapper001::Mapper001(Cartridge* cartridge)
    : Mapper(cartridge)
    , m_shiftRegister(0x10)
    , m_shiftCount(0)
    , m_control(0x0C)
    , m_chrBank0(0)
    , m_chrBank1(0)
    , m_prgBank(0) {
}

void Mapper001::reset() {
    m_shiftRegister = 0x10;
    m_shiftCount = 0;
    m_control = 0x0C;  // PRG mode 3, CHR mode 0
    m_chrBank0 = 0;
    m_chrBank1 = 0;
    m_prgBank = 0;
    updateBanks();
}

void Mapper001::updateBanks() {
    const auto& prg = m_cartridge->getPRG();
    const auto& chr = m_cartridge->getCHR();
    
    u32 prgSize = prg.size();
    u32 chrSize = chr.size();
    
    // PRG bank switching
    u8 prgMode = (m_control >> 2) & 0x03;
    
    switch (prgMode) {
        case 0:
        case 1:
            // 32KB mode (ignore low bit of bank number)
            m_prgBankOffset[0] = ((m_prgBank & 0x0E) % (prgSize / 0x4000)) * 0x4000;
            m_prgBankOffset[1] = m_prgBankOffset[0] + 0x4000;
            break;
        case 2:
            // Fix first bank, switch second
            m_prgBankOffset[0] = 0;
            m_prgBankOffset[1] = ((m_prgBank & 0x0F) % (prgSize / 0x4000)) * 0x4000;
            break;
        case 3:
            // Switch first bank, fix last
            m_prgBankOffset[0] = ((m_prgBank & 0x0F) % (prgSize / 0x4000)) * 0x4000;
            m_prgBankOffset[1] = prgSize - 0x4000;
            break;
    }
    
    // CHR bank switching
    bool chrMode = (m_control & 0x10) != 0;
    
    if (chrMode) {
        // 4KB mode
        m_chrBankOffset[0] = (m_chrBank0 % (chrSize / 0x1000)) * 0x1000;
        m_chrBankOffset[1] = (m_chrBank1 % (chrSize / 0x1000)) * 0x1000;
    } else {
        // 8KB mode
        m_chrBankOffset[0] = ((m_chrBank0 & 0x1E) % (chrSize / 0x1000)) * 0x1000;
        m_chrBankOffset[1] = m_chrBankOffset[0] + 0x1000;
    }
}

u8 Mapper001::cpuRead(u16 address) {
    if (address >= 0x6000 && address < 0x8000) {
        return m_cartridge->getPRGRAM()[address & 0x1FFF];
    } else if (address >= 0x8000 && address < 0xC000) {
        return m_cartridge->getPRG()[m_prgBankOffset[0] + (address & 0x3FFF)];
    } else if (address >= 0xC000) {
        return m_cartridge->getPRG()[m_prgBankOffset[1] + (address & 0x3FFF)];
    }
    return 0;
}

void Mapper001::cpuWrite(u16 address, u8 value) {
    if (address >= 0x6000 && address < 0x8000) {
        m_cartridge->getPRGRAM()[address & 0x1FFF] = value;
    } else if (address >= 0x8000) {
        // Register writes
        if (value & 0x80) {
            // Reset shift register
            m_shiftRegister = 0x10;
            m_shiftCount = 0;
            m_control |= 0x0C;
            updateBanks();
        } else {
            // Shift in bit
            m_shiftRegister >>= 1;
            m_shiftRegister |= (value & 1) << 4;
            m_shiftCount++;
            
            if (m_shiftCount == 5) {
                // Write to internal register
                u8 reg = (address >> 13) & 0x03;
                
                switch (reg) {
                    case 0:  // $8000-$9FFF: Control
                        m_control = m_shiftRegister;
                        break;
                    case 1:  // $A000-$BFFF: CHR bank 0
                        m_chrBank0 = m_shiftRegister;
                        break;
                    case 2:  // $C000-$DFFF: CHR bank 1
                        m_chrBank1 = m_shiftRegister;
                        break;
                    case 3:  // $E000-$FFFF: PRG bank
                        m_prgBank = m_shiftRegister;
                        break;
                }
                
                m_shiftRegister = 0x10;
                m_shiftCount = 0;
                updateBanks();
            }
        }
    }
}

u8 Mapper001::readCHR(u16 address) {
    if (address < 0x1000) {
        return m_cartridge->getCHR()[m_chrBankOffset[0] + (address & 0x0FFF)];
    } else {
        return m_cartridge->getCHR()[m_chrBankOffset[1] + (address & 0x0FFF)];
    }
}

void Mapper001::writeCHR(u16 address, u8 value) {
    // CHR RAM only
    if (address < 0x1000) {
        m_cartridge->getCHR()[m_chrBankOffset[0] + (address & 0x0FFF)] = value;
    } else {
        m_cartridge->getCHR()[m_chrBankOffset[1] + (address & 0x0FFF)] = value;
    }
}

MirrorMode Mapper001::getMirrorMode() const {
    switch (m_control & 0x03) {
        case 0: return MirrorMode::SINGLE_SCREEN_A;
        case 1: return MirrorMode::SINGLE_SCREEN_B;
        case 2: return MirrorMode::VERTICAL;
        case 3: return MirrorMode::HORIZONTAL;
        default: return MirrorMode::HORIZONTAL;
    }
}

void Mapper001::saveState(std::ofstream& file) const {
    file.write(reinterpret_cast<const char*>(&m_shiftRegister), sizeof(m_shiftRegister));
    file.write(reinterpret_cast<const char*>(&m_shiftCount), sizeof(m_shiftCount));
    file.write(reinterpret_cast<const char*>(&m_control), sizeof(m_control));
    file.write(reinterpret_cast<const char*>(&m_chrBank0), sizeof(m_chrBank0));
    file.write(reinterpret_cast<const char*>(&m_chrBank1), sizeof(m_chrBank1));
    file.write(reinterpret_cast<const char*>(&m_prgBank), sizeof(m_prgBank));
}

void Mapper001::loadState(std::ifstream& file) {
    file.read(reinterpret_cast<char*>(&m_shiftRegister), sizeof(m_shiftRegister));
    file.read(reinterpret_cast<char*>(&m_shiftCount), sizeof(m_shiftCount));
    file.read(reinterpret_cast<char*>(&m_control), sizeof(m_control));
    file.read(reinterpret_cast<char*>(&m_chrBank0), sizeof(m_chrBank0));
    file.read(reinterpret_cast<char*>(&m_chrBank1), sizeof(m_chrBank1));
    file.read(reinterpret_cast<char*>(&m_prgBank), sizeof(m_prgBank));
    updateBanks();
}

// ========== Mapper 2: UxROM ==========

Mapper002::Mapper002(Cartridge* cartridge)
    : Mapper(cartridge)
    , m_prgBank(0) {
}

void Mapper002::reset() {
    m_prgBank = 0;
}

u8 Mapper002::cpuRead(u16 address) {
    if (address >= 0x6000 && address < 0x8000) {
        return m_cartridge->getPRGRAM()[address & 0x1FFF];
    } else if (address >= 0x8000 && address < 0xC000) {
        // Switchable bank
        const auto& prg = m_cartridge->getPRG();
        u32 offset = (m_prgBank % (prg.size() / 0x4000)) * 0x4000;
        return prg[offset + (address & 0x3FFF)];
    } else if (address >= 0xC000) {
        // Fixed to last bank
        const auto& prg = m_cartridge->getPRG();
        return prg[prg.size() - 0x4000 + (address & 0x3FFF)];
    }
    return 0;
}

void Mapper002::cpuWrite(u16 address, u8 value) {
    if (address >= 0x6000 && address < 0x8000) {
        m_cartridge->getPRGRAM()[address & 0x1FFF] = value;
    } else if (address >= 0x8000) {
        m_prgBank = value;
    }
}

u8 Mapper002::readCHR(u16 address) {
    return m_cartridge->getCHR()[address & 0x1FFF];
}

void Mapper002::writeCHR(u16 address, u8 value) {
    m_cartridge->getCHR()[address & 0x1FFF] = value;
}

void Mapper002::saveState(std::ofstream& file) const {
    file.write(reinterpret_cast<const char*>(&m_prgBank), sizeof(m_prgBank));
}

void Mapper002::loadState(std::ifstream& file) {
    file.read(reinterpret_cast<char*>(&m_prgBank), sizeof(m_prgBank));
}

// ========== Mapper 3: CNROM ==========

Mapper003::Mapper003(Cartridge* cartridge)
    : Mapper(cartridge)
    , m_chrBank(0) {
}

void Mapper003::reset() {
    m_chrBank = 0;
}

u8 Mapper003::cpuRead(u16 address) {
    if (address >= 0x6000 && address < 0x8000) {
        return m_cartridge->getPRGRAM()[address & 0x1FFF];
    } else if (address >= 0x8000) {
        const auto& prg = m_cartridge->getPRG();
        return prg[(address - 0x8000) % prg.size()];
    }
    return 0;
}

void Mapper003::cpuWrite(u16 address, u8 value) {
    if (address >= 0x6000 && address < 0x8000) {
        m_cartridge->getPRGRAM()[address & 0x1FFF] = value;
    } else if (address >= 0x8000) {
        m_chrBank = value & 0x03;  // Usually only 2 bits used
    }
}

u8 Mapper003::readCHR(u16 address) {
    const auto& chr = m_cartridge->getCHR();
    u32 offset = (m_chrBank % (chr.size() / 0x2000)) * 0x2000;
    return chr[offset + (address & 0x1FFF)];
}

void Mapper003::writeCHR(u16 address, u8 value) {
    (void)address;
    (void)value;
    // CHR ROM - ignore writes
}

void Mapper003::saveState(std::ofstream& file) const {
    file.write(reinterpret_cast<const char*>(&m_chrBank), sizeof(m_chrBank));
}

void Mapper003::loadState(std::ifstream& file) {
    file.read(reinterpret_cast<char*>(&m_chrBank), sizeof(m_chrBank));
}

// ========== Mapper 4: MMC3 ==========

Mapper004::Mapper004(Cartridge* cartridge)
    : Mapper(cartridge)
    , m_bankSelect(0)
    , m_irqLatch(0)
    , m_irqCounter(0)
    , m_irqEnable(false)
    , m_irqReload(false)
    , m_mirrorMode(MirrorMode::HORIZONTAL)
    , m_prgRamEnable(true) {
    std::memset(m_bankData, 0, sizeof(m_bankData));
}

void Mapper004::reset() {
    m_bankSelect = 0;
    std::memset(m_bankData, 0, sizeof(m_bankData));
    m_irqLatch = 0;
    m_irqCounter = 0;
    m_irqEnable = false;
    m_irqReload = false;
    m_irqActive = false;
    m_mirrorMode = MirrorMode::HORIZONTAL;
    m_prgRamEnable = true;
    updateBanks();
}

void Mapper004::updateBanks() {
    const auto& prg = m_cartridge->getPRG();
    const auto& chr = m_cartridge->getCHR();
    
    u32 prgBanks = prg.size() / 0x2000;  // 8KB banks
    u32 chrBanks = chr.size() / 0x0400;  // 1KB banks
    
    // PRG bank layout depends on bit 6 of bank select
    if (m_bankSelect & 0x40) {
        // $C000 swappable, $8000 fixed to second-to-last bank
        m_prgBankOffset[0] = (prgBanks - 2) * 0x2000;
        m_prgBankOffset[1] = (m_bankData[7] % prgBanks) * 0x2000;
        m_prgBankOffset[2] = (m_bankData[6] % prgBanks) * 0x2000;
        m_prgBankOffset[3] = (prgBanks - 1) * 0x2000;
    } else {
        // $8000 swappable, $C000 fixed to second-to-last bank
        m_prgBankOffset[0] = (m_bankData[6] % prgBanks) * 0x2000;
        m_prgBankOffset[1] = (m_bankData[7] % prgBanks) * 0x2000;
        m_prgBankOffset[2] = (prgBanks - 2) * 0x2000;
        m_prgBankOffset[3] = (prgBanks - 1) * 0x2000;
    }
    
    // CHR bank layout depends on bit 7 of bank select
    if (chrBanks > 0) {
        if (m_bankSelect & 0x80) {
            // R0/R1 at $1000, R2-R5 at $0000
            m_chrBankOffset[0] = (m_bankData[2] % chrBanks) * 0x0400;
            m_chrBankOffset[1] = (m_bankData[3] % chrBanks) * 0x0400;
            m_chrBankOffset[2] = (m_bankData[4] % chrBanks) * 0x0400;
            m_chrBankOffset[3] = (m_bankData[5] % chrBanks) * 0x0400;
            m_chrBankOffset[4] = ((m_bankData[0] & 0xFE) % chrBanks) * 0x0400;
            m_chrBankOffset[5] = ((m_bankData[0] | 0x01) % chrBanks) * 0x0400;
            m_chrBankOffset[6] = ((m_bankData[1] & 0xFE) % chrBanks) * 0x0400;
            m_chrBankOffset[7] = ((m_bankData[1] | 0x01) % chrBanks) * 0x0400;
        } else {
            // R0/R1 at $0000, R2-R5 at $1000
            m_chrBankOffset[0] = ((m_bankData[0] & 0xFE) % chrBanks) * 0x0400;
            m_chrBankOffset[1] = ((m_bankData[0] | 0x01) % chrBanks) * 0x0400;
            m_chrBankOffset[2] = ((m_bankData[1] & 0xFE) % chrBanks) * 0x0400;
            m_chrBankOffset[3] = ((m_bankData[1] | 0x01) % chrBanks) * 0x0400;
            m_chrBankOffset[4] = (m_bankData[2] % chrBanks) * 0x0400;
            m_chrBankOffset[5] = (m_bankData[3] % chrBanks) * 0x0400;
            m_chrBankOffset[6] = (m_bankData[4] % chrBanks) * 0x0400;
            m_chrBankOffset[7] = (m_bankData[5] % chrBanks) * 0x0400;
        }
    }
}

u8 Mapper004::cpuRead(u16 address) {
    if (address >= 0x6000 && address < 0x8000) {
        if (m_prgRamEnable) {
            return m_cartridge->getPRGRAM()[address & 0x1FFF];
        }
        return 0;
    } else if (address >= 0x8000) {
        u8 bank = (address - 0x8000) / 0x2000;
        u16 offset = address & 0x1FFF;
        return m_cartridge->getPRG()[m_prgBankOffset[bank] + offset];
    }
    return 0;
}

void Mapper004::cpuWrite(u16 address, u8 value) {
    if (address >= 0x6000 && address < 0x8000) {
        if (m_prgRamEnable) {
            m_cartridge->getPRGRAM()[address & 0x1FFF] = value;
        }
    } else if (address >= 0x8000) {
        switch (address & 0xE001) {
            case 0x8000:
                // Bank select
                m_bankSelect = value;
                updateBanks();
                break;
                
            case 0x8001:
                // Bank data
                m_bankData[m_bankSelect & 0x07] = value;
                updateBanks();
                break;
                
            case 0xA000:
                // Mirroring
                if (value & 0x01) {
                    m_mirrorMode = MirrorMode::HORIZONTAL;
                } else {
                    m_mirrorMode = MirrorMode::VERTICAL;
                }
                break;
                
            case 0xA001:
                // PRG RAM protect
                m_prgRamEnable = (value & 0x80) != 0;
                break;
                
            case 0xC000:
                // IRQ latch
                m_irqLatch = value;
                break;
                
            case 0xC001:
                // IRQ reload
                m_irqReload = true;
                m_irqCounter = 0;
                break;
                
            case 0xE000:
                // IRQ disable
                m_irqEnable = false;
                m_irqActive = false;
                break;
                
            case 0xE001:
                // IRQ enable
                m_irqEnable = true;
                break;
        }
    }
}

u8 Mapper004::readCHR(u16 address) {
    u8 bank = address / 0x0400;
    u16 offset = address & 0x03FF;
    return m_cartridge->getCHR()[m_chrBankOffset[bank] + offset];
}

void Mapper004::writeCHR(u16 address, u8 value) {
    // CHR RAM only
    u8 bank = address / 0x0400;
    u16 offset = address & 0x03FF;
    m_cartridge->getCHR()[m_chrBankOffset[bank] + offset] = value;
}

MirrorMode Mapper004::getMirrorMode() const {
    return m_mirrorMode;
}

void Mapper004::scanlineCounter() {
    if (m_irqCounter == 0 || m_irqReload) {
        m_irqCounter = m_irqLatch;
        m_irqReload = false;
    } else {
        m_irqCounter--;
    }
    
    if (m_irqCounter == 0 && m_irqEnable) {
        m_irqActive = true;
    }
}

void Mapper004::saveState(std::ofstream& file) const {
    file.write(reinterpret_cast<const char*>(&m_bankSelect), sizeof(m_bankSelect));
    file.write(reinterpret_cast<const char*>(m_bankData), sizeof(m_bankData));
    file.write(reinterpret_cast<const char*>(&m_irqLatch), sizeof(m_irqLatch));
    file.write(reinterpret_cast<const char*>(&m_irqCounter), sizeof(m_irqCounter));
    file.write(reinterpret_cast<const char*>(&m_irqEnable), sizeof(m_irqEnable));
    file.write(reinterpret_cast<const char*>(&m_irqReload), sizeof(m_irqReload));
    file.write(reinterpret_cast<const char*>(&m_irqActive), sizeof(m_irqActive));
    file.write(reinterpret_cast<const char*>(&m_mirrorMode), sizeof(m_mirrorMode));
    file.write(reinterpret_cast<const char*>(&m_prgRamEnable), sizeof(m_prgRamEnable));
}

void Mapper004::loadState(std::ifstream& file) {
    file.read(reinterpret_cast<char*>(&m_bankSelect), sizeof(m_bankSelect));
    file.read(reinterpret_cast<char*>(m_bankData), sizeof(m_bankData));
    file.read(reinterpret_cast<char*>(&m_irqLatch), sizeof(m_irqLatch));
    file.read(reinterpret_cast<char*>(&m_irqCounter), sizeof(m_irqCounter));
    file.read(reinterpret_cast<char*>(&m_irqEnable), sizeof(m_irqEnable));
    file.read(reinterpret_cast<char*>(&m_irqReload), sizeof(m_irqReload));
    file.read(reinterpret_cast<char*>(&m_irqActive), sizeof(m_irqActive));
    file.read(reinterpret_cast<char*>(&m_mirrorMode), sizeof(m_mirrorMode));
    file.read(reinterpret_cast<char*>(&m_prgRamEnable), sizeof(m_prgRamEnable));
    updateBanks();
}

// ========== Mapper 10: MMC4 ==========

Mapper010::Mapper010(Cartridge* cartridge)
    : Mapper(cartridge)
    , m_prgBank(0)
    , m_chrBank0FD(0)
    , m_chrBank0FE(0)
    , m_chrBank1FD(0)
    , m_chrBank1FE(0)
    , m_latch0(0xFE)
    , m_latch1(0xFE)
    , m_mirrorMode(MirrorMode::HORIZONTAL) {
}

void Mapper010::reset() {
    m_prgBank = 0;
    m_chrBank0FD = 0;
    m_chrBank0FE = 0;
    m_chrBank1FD = 0;
    m_chrBank1FE = 0;
    m_latch0 = 0xFE;
    m_latch1 = 0xFE;
    m_mirrorMode = MirrorMode::HORIZONTAL;
}

u8 Mapper010::cpuRead(u16 address) {
    if (address >= 0x6000 && address < 0x8000) {
        return m_cartridge->getPRGRAM()[address & 0x1FFF];
    } else if (address >= 0x8000 && address < 0xC000) {
        const auto& prg = m_cartridge->getPRG();
        u32 offset = (m_prgBank % (prg.size() / 0x4000)) * 0x4000;
        return prg[offset + (address & 0x3FFF)];
    } else if (address >= 0xC000) {
        const auto& prg = m_cartridge->getPRG();
        return prg[prg.size() - 0x4000 + (address & 0x3FFF)];
    }
    return 0;
}

void Mapper010::cpuWrite(u16 address, u8 value) {
    if (address >= 0x6000 && address < 0x8000) {
        m_cartridge->getPRGRAM()[address & 0x1FFF] = value;
    } else if (address >= 0xA000 && address < 0xB000) {
        // PRG bank select
        m_prgBank = value & 0x0F;
    } else if (address >= 0xB000 && address < 0xC000) {
        // CHR bank 0 select ($FD)
        m_chrBank0FD = value & 0x1F;
    } else if (address >= 0xC000 && address < 0xD000) {
        // CHR bank 0 select ($FE)
        m_chrBank0FE = value & 0x1F;
    } else if (address >= 0xD000 && address < 0xE000) {
        // CHR bank 1 select ($FD)
        m_chrBank1FD = value & 0x1F;
    } else if (address >= 0xE000 && address < 0xF000) {
        // CHR bank 1 select ($FE)
        m_chrBank1FE = value & 0x1F;
    } else if (address >= 0xF000) {
        // Mirroring
        m_mirrorMode = (value & 0x01) ? MirrorMode::HORIZONTAL : MirrorMode::VERTICAL;
    }
}

u8 Mapper010::readCHR(u16 address) {
    const auto& chr = m_cartridge->getCHR();
    u32 chrBanks = chr.size() / 0x1000;
    
    if (address < 0x1000) {
        u8 bank = (m_latch0 == 0xFD) ? m_chrBank0FD : m_chrBank0FE;
        u32 offset = (bank % chrBanks) * 0x1000;
        u8 value = chr[offset + (address & 0x0FFF)];
        
        // Update latch based on tile fetched
        if ((address & 0x0FF8) == 0x0FD8) {
            m_latch0 = 0xFD;
        } else if ((address & 0x0FF8) == 0x0FE8) {
            m_latch0 = 0xFE;
        }
        
        return value;
    } else {
        u8 bank = (m_latch1 == 0xFD) ? m_chrBank1FD : m_chrBank1FE;
        u32 offset = (bank % chrBanks) * 0x1000;
        u8 value = chr[offset + (address & 0x0FFF)];
        
        // Update latch based on tile fetched (addresses relative to bank)
        u16 bankAddr = address & 0x0FFF;
        if ((bankAddr & 0x0FF8) == 0x0FD8) {
            m_latch1 = 0xFD;
        } else if ((bankAddr & 0x0FF8) == 0x0FE8) {
            m_latch1 = 0xFE;
        }
        
        return value;
    }
}

void Mapper010::writeCHR(u16 address, u8 value) {
    (void)address;
    (void)value;
    // CHR ROM - ignore writes
}

MirrorMode Mapper010::getMirrorMode() const {
    return m_mirrorMode;
}

void Mapper010::saveState(std::ofstream& file) const {
    file.write(reinterpret_cast<const char*>(&m_prgBank), sizeof(m_prgBank));
    file.write(reinterpret_cast<const char*>(&m_chrBank0FD), sizeof(m_chrBank0FD));
    file.write(reinterpret_cast<const char*>(&m_chrBank0FE), sizeof(m_chrBank0FE));
    file.write(reinterpret_cast<const char*>(&m_chrBank1FD), sizeof(m_chrBank1FD));
    file.write(reinterpret_cast<const char*>(&m_chrBank1FE), sizeof(m_chrBank1FE));
    file.write(reinterpret_cast<const char*>(&m_latch0), sizeof(m_latch0));
    file.write(reinterpret_cast<const char*>(&m_latch1), sizeof(m_latch1));
    file.write(reinterpret_cast<const char*>(&m_mirrorMode), sizeof(m_mirrorMode));
}

void Mapper010::loadState(std::ifstream& file) {
    file.read(reinterpret_cast<char*>(&m_prgBank), sizeof(m_prgBank));
    file.read(reinterpret_cast<char*>(&m_chrBank0FD), sizeof(m_chrBank0FD));
    file.read(reinterpret_cast<char*>(&m_chrBank0FE), sizeof(m_chrBank0FE));
    file.read(reinterpret_cast<char*>(&m_chrBank1FD), sizeof(m_chrBank1FD));
    file.read(reinterpret_cast<char*>(&m_chrBank1FE), sizeof(m_chrBank1FE));
    file.read(reinterpret_cast<char*>(&m_latch0), sizeof(m_latch0));
    file.read(reinterpret_cast<char*>(&m_latch1), sizeof(m_latch1));
    file.read(reinterpret_cast<char*>(&m_mirrorMode), sizeof(m_mirrorMode));
}

} // namespace nes

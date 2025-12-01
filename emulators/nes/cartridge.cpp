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

// ========== Mapper 5: MMC5 (ExROM) ==========

Mapper005::Mapper005(Cartridge* cartridge)
    : Mapper(cartridge)
    , m_prgMode(3)
    , m_prgRamProtect1(false)
    , m_prgRamProtect2(false)
    , m_chrMode(0)
    , m_chrBankHigh(false)
    , m_nametableMapping(0)
    , m_fillModeTile(0)
    , m_fillModeAttr(0)
    , m_exRamMode(0)
    , m_irqScanline(0)
    , m_irqStatus(0)
    , m_irqEnable(false)
    , m_inFrame(false)
    , m_scanlineCounter(0)
    , m_multiplicand(0)
    , m_multiplier(0) {
    std::memset(m_prgBankRegs, 0xFF, sizeof(m_prgBankRegs));
    std::memset(m_chrBankRegs, 0, sizeof(m_chrBankRegs));
    std::memset(m_prgBankOffset, 0, sizeof(m_prgBankOffset));
    std::memset(m_chrBankOffset, 0, sizeof(m_chrBankOffset));
    m_exRam.fill(0);
    m_prgRamExt.fill(0);
}

void Mapper005::reset() {
    m_prgMode = 3;
    m_prgRamProtect1 = false;
    m_prgRamProtect2 = false;
    m_chrMode = 0;
    m_chrBankHigh = false;
    m_nametableMapping = 0;
    m_fillModeTile = 0;
    m_fillModeAttr = 0;
    m_exRamMode = 0;
    m_irqScanline = 0;
    m_irqStatus = 0;
    m_irqEnable = false;
    m_inFrame = false;
    m_scanlineCounter = 0;
    m_multiplicand = 0;
    m_multiplier = 0;
    m_irqActive = false;
    
    std::memset(m_prgBankRegs, 0xFF, sizeof(m_prgBankRegs));
    std::memset(m_chrBankRegs, 0, sizeof(m_chrBankRegs));
    m_exRam.fill(0);
    
    updatePRGBanks();
    updateCHRBanks();
}

void Mapper005::updatePRGBanks() {
    const auto& prg = m_cartridge->getPRG();
    u32 prgSize = prg.size();
    u32 prgBanks8k = prgSize / 0x2000;
    
    switch (m_prgMode) {
        case 0:  // 32KB mode
            {
                u8 bank = (m_prgBankRegs[4] & 0x7C) >> 2;
                u32 offset = (bank % (prgBanks8k / 4)) * 0x8000;
                m_prgBankOffset[0] = offset;
                m_prgBankOffset[1] = offset + 0x2000;
                m_prgBankOffset[2] = offset + 0x4000;
                m_prgBankOffset[3] = offset + 0x6000;
            }
            break;
            
        case 1:  // 16KB + 16KB mode
            {
                u8 bank0 = (m_prgBankRegs[2] & 0x7E) >> 1;
                u8 bank1 = (m_prgBankRegs[4] & 0x7E) >> 1;
                m_prgBankOffset[0] = ((bank0 % (prgBanks8k / 2)) * 0x4000);
                m_prgBankOffset[1] = m_prgBankOffset[0] + 0x2000;
                m_prgBankOffset[2] = ((bank1 % (prgBanks8k / 2)) * 0x4000);
                m_prgBankOffset[3] = m_prgBankOffset[2] + 0x2000;
            }
            break;
            
        case 2:  // 16KB + 8KB + 8KB mode
            {
                u8 bank0 = (m_prgBankRegs[2] & 0x7E) >> 1;
                u8 bank1 = m_prgBankRegs[3] & 0x7F;
                u8 bank2 = m_prgBankRegs[4] & 0x7F;
                m_prgBankOffset[0] = ((bank0 % (prgBanks8k / 2)) * 0x4000);
                m_prgBankOffset[1] = m_prgBankOffset[0] + 0x2000;
                m_prgBankOffset[2] = (bank1 % prgBanks8k) * 0x2000;
                m_prgBankOffset[3] = (bank2 % prgBanks8k) * 0x2000;
            }
            break;
            
        case 3:  // 8KB x 4 mode
            {
                u8 bank0 = m_prgBankRegs[1] & 0x7F;
                u8 bank1 = m_prgBankRegs[2] & 0x7F;
                u8 bank2 = m_prgBankRegs[3] & 0x7F;
                u8 bank3 = m_prgBankRegs[4] & 0x7F;
                m_prgBankOffset[0] = (bank0 % prgBanks8k) * 0x2000;
                m_prgBankOffset[1] = (bank1 % prgBanks8k) * 0x2000;
                m_prgBankOffset[2] = (bank2 % prgBanks8k) * 0x2000;
                m_prgBankOffset[3] = (bank3 % prgBanks8k) * 0x2000;
            }
            break;
    }
}

void Mapper005::updateCHRBanks() {
    const auto& chr = m_cartridge->getCHR();
    u32 chrSize = chr.size();
    if (chrSize == 0) return;
    
    u32 chrBanks1k = chrSize / 0x400;
    if (chrBanks1k == 0) chrBanks1k = 1;
    
    switch (m_chrMode) {
        case 0:  // 8KB mode
            {
                u16 bank = m_chrBankRegs[7] & 0xFF;
                u32 offset = (bank % (chrBanks1k / 8)) * 0x2000;
                for (int i = 0; i < 8; i++) {
                    m_chrBankOffset[i] = offset + i * 0x400;
                }
            }
            break;
            
        case 1:  // 4KB mode
            {
                u16 bank0 = m_chrBankRegs[3] & 0xFF;
                u16 bank1 = m_chrBankRegs[7] & 0xFF;
                u32 offset0 = (bank0 % (chrBanks1k / 4)) * 0x1000;
                u32 offset1 = (bank1 % (chrBanks1k / 4)) * 0x1000;
                for (int i = 0; i < 4; i++) {
                    m_chrBankOffset[i] = offset0 + i * 0x400;
                    m_chrBankOffset[i + 4] = offset1 + i * 0x400;
                }
            }
            break;
            
        case 2:  // 2KB mode
            {
                u16 bank0 = m_chrBankRegs[1] & 0xFF;
                u16 bank1 = m_chrBankRegs[3] & 0xFF;
                u16 bank2 = m_chrBankRegs[5] & 0xFF;
                u16 bank3 = m_chrBankRegs[7] & 0xFF;
                m_chrBankOffset[0] = ((bank0 % (chrBanks1k / 2)) * 0x800);
                m_chrBankOffset[1] = m_chrBankOffset[0] + 0x400;
                m_chrBankOffset[2] = ((bank1 % (chrBanks1k / 2)) * 0x800);
                m_chrBankOffset[3] = m_chrBankOffset[2] + 0x400;
                m_chrBankOffset[4] = ((bank2 % (chrBanks1k / 2)) * 0x800);
                m_chrBankOffset[5] = m_chrBankOffset[4] + 0x400;
                m_chrBankOffset[6] = ((bank3 % (chrBanks1k / 2)) * 0x800);
                m_chrBankOffset[7] = m_chrBankOffset[6] + 0x400;
            }
            break;
            
        case 3:  // 1KB mode
            for (int i = 0; i < 8; i++) {
                u16 bank = m_chrBankRegs[i] & 0xFF;
                m_chrBankOffset[i] = (bank % chrBanks1k) * 0x400;
            }
            break;
    }
}

u8 Mapper005::cpuRead(u16 address) {
    if (address >= 0x5000 && address < 0x5C00) {
        // MMC5 registers
        switch (address) {
            case 0x5204:  // IRQ Status
                {
                    u8 result = m_irqStatus;
                    m_irqStatus &= ~0x80;  // Clear pending flag on read
                    m_irqActive = false;
                    return result;
                }
            case 0x5205:  // Multiply result low
                return (m_multiplicand * m_multiplier) & 0xFF;
            case 0x5206:  // Multiply result high
                return ((m_multiplicand * m_multiplier) >> 8) & 0xFF;
            default:
                return 0;
        }
    } else if (address >= 0x5C00 && address < 0x6000) {
        // ExRAM
        return readExRAM(address - 0x5C00);
    } else if (address >= 0x6000 && address < 0x8000) {
        // PRG RAM
        return m_prgRamExt[address & 0x1FFF];
    } else if (address >= 0x8000) {
        // PRG ROM
        u8 bank = (address - 0x8000) / 0x2000;
        u16 offset = address & 0x1FFF;
        
        // Check if bank points to RAM or ROM
        u8 bankReg = m_prgBankRegs[(m_prgMode == 3) ? (bank + 1) : ((bank < 2) ? 2 : 4)];
        if (!(bankReg & 0x80)) {
            // RAM bank
            return m_prgRamExt[(bankReg & 0x07) * 0x2000 + offset];
        }
        return m_cartridge->getPRG()[m_prgBankOffset[bank] + offset];
    }
    return 0;
}

void Mapper005::cpuWrite(u16 address, u8 value) {
    if (address >= 0x5000 && address < 0x5C00) {
        // MMC5 registers
        switch (address) {
            case 0x5100:  // PRG mode
                m_prgMode = value & 0x03;
                updatePRGBanks();
                break;
            case 0x5101:  // CHR mode
                m_chrMode = value & 0x03;
                updateCHRBanks();
                break;
            case 0x5102:  // PRG RAM protect 1
                m_prgRamProtect1 = (value & 0x03) == 0x02;
                break;
            case 0x5103:  // PRG RAM protect 2
                m_prgRamProtect2 = (value & 0x03) == 0x01;
                break;
            case 0x5104:  // Extended RAM mode
                m_exRamMode = value & 0x03;
                break;
            case 0x5105:  // Nametable mapping
                m_nametableMapping = value;
                break;
            case 0x5106:  // Fill mode tile
                m_fillModeTile = value;
                break;
            case 0x5107:  // Fill mode attribute
                m_fillModeAttr = value & 0x03;
                break;
            case 0x5113:  // PRG bank 0 (RAM)
                m_prgBankRegs[0] = value;
                updatePRGBanks();
                break;
            case 0x5114:  // PRG bank 1
                m_prgBankRegs[1] = value;
                updatePRGBanks();
                break;
            case 0x5115:  // PRG bank 2
                m_prgBankRegs[2] = value;
                updatePRGBanks();
                break;
            case 0x5116:  // PRG bank 3
                m_prgBankRegs[3] = value;
                updatePRGBanks();
                break;
            case 0x5117:  // PRG bank 4
                m_prgBankRegs[4] = value | 0x80;  // Always ROM
                updatePRGBanks();
                break;
            case 0x5120: case 0x5121: case 0x5122: case 0x5123:
            case 0x5124: case 0x5125: case 0x5126: case 0x5127:
                // CHR banks (sprite)
                m_chrBankRegs[address - 0x5120] = value;
                m_chrBankHigh = false;
                updateCHRBanks();
                break;
            case 0x5128: case 0x5129: case 0x512A: case 0x512B:
                // CHR banks (background)
                m_chrBankRegs[8 + (address - 0x5128)] = value;
                m_chrBankHigh = true;
                updateCHRBanks();
                break;
            case 0x5203:  // IRQ scanline
                m_irqScanline = value;
                break;
            case 0x5204:  // IRQ enable
                m_irqEnable = (value & 0x80) != 0;
                break;
            case 0x5205:  // Multiplicand
                m_multiplicand = value;
                break;
            case 0x5206:  // Multiplier
                m_multiplier = value;
                break;
        }
    } else if (address >= 0x5C00 && address < 0x6000) {
        // ExRAM
        writeExRAM(address - 0x5C00, value);
    } else if (address >= 0x6000 && address < 0x8000) {
        // PRG RAM (if write-enabled)
        if (m_prgRamProtect1 && m_prgRamProtect2) {
            m_prgRamExt[address & 0x1FFF] = value;
        }
    }
    // ROM writes ignored
}

u8 Mapper005::readExRAM(u16 address) {
    if (m_exRamMode >= 2) {
        return m_exRam[address & 0x3FF];
    }
    return 0;  // Mode 0-1 returns open bus
}

void Mapper005::writeExRAM(u16 address, u8 value) {
    if (m_exRamMode != 3) {  // Mode 3 is read-only
        m_exRam[address & 0x3FF] = value;
    }
}

u8 Mapper005::readCHR(u16 address) {
    const auto& chr = m_cartridge->getCHR();
    if (chr.empty()) return 0;
    
    u8 bank = address / 0x400;
    u16 offset = address & 0x3FF;
    return chr[m_chrBankOffset[bank] + offset];
}

void Mapper005::writeCHR(u16 address, u8 value) {
    // CHR RAM support
    auto& chr = m_cartridge->getCHR();
    if (!chr.empty()) {
        u8 bank = address / 0x400;
        u16 offset = address & 0x3FF;
        chr[m_chrBankOffset[bank] + offset] = value;
    }
}

MirrorMode Mapper005::getMirrorMode() const {
    // MMC5 has complex nametable mapping, simplified to basic modes
    return m_cartridge->getBaseMirrorMode();
}

void Mapper005::scanlineCounter() {
    if (!m_inFrame) {
        m_inFrame = true;
        m_scanlineCounter = 0;
    }
    
    m_scanlineCounter++;
    
    if (m_scanlineCounter == m_irqScanline) {
        m_irqStatus |= 0x80;  // Set pending
        if (m_irqEnable) {
            m_irqActive = true;
        }
    }
    
    // Detect end of frame (scanline 240)
    if (m_scanlineCounter >= 240) {
        m_inFrame = false;
        m_irqStatus &= ~0x40;  // Clear in-frame
    } else {
        m_irqStatus |= 0x40;   // Set in-frame
    }
}

void Mapper005::saveState(std::ofstream& file) const {
    file.write(reinterpret_cast<const char*>(&m_prgMode), sizeof(m_prgMode));
    file.write(reinterpret_cast<const char*>(m_prgBankRegs), sizeof(m_prgBankRegs));
    file.write(reinterpret_cast<const char*>(&m_prgRamProtect1), sizeof(m_prgRamProtect1));
    file.write(reinterpret_cast<const char*>(&m_prgRamProtect2), sizeof(m_prgRamProtect2));
    file.write(reinterpret_cast<const char*>(&m_chrMode), sizeof(m_chrMode));
    file.write(reinterpret_cast<const char*>(m_chrBankRegs), sizeof(m_chrBankRegs));
    file.write(reinterpret_cast<const char*>(&m_chrBankHigh), sizeof(m_chrBankHigh));
    file.write(reinterpret_cast<const char*>(&m_nametableMapping), sizeof(m_nametableMapping));
    file.write(reinterpret_cast<const char*>(&m_fillModeTile), sizeof(m_fillModeTile));
    file.write(reinterpret_cast<const char*>(&m_fillModeAttr), sizeof(m_fillModeAttr));
    file.write(reinterpret_cast<const char*>(m_exRam.data()), m_exRam.size());
    file.write(reinterpret_cast<const char*>(&m_exRamMode), sizeof(m_exRamMode));
    file.write(reinterpret_cast<const char*>(&m_irqScanline), sizeof(m_irqScanline));
    file.write(reinterpret_cast<const char*>(&m_irqStatus), sizeof(m_irqStatus));
    file.write(reinterpret_cast<const char*>(&m_irqEnable), sizeof(m_irqEnable));
    file.write(reinterpret_cast<const char*>(&m_inFrame), sizeof(m_inFrame));
    file.write(reinterpret_cast<const char*>(&m_scanlineCounter), sizeof(m_scanlineCounter));
    file.write(reinterpret_cast<const char*>(&m_multiplicand), sizeof(m_multiplicand));
    file.write(reinterpret_cast<const char*>(&m_multiplier), sizeof(m_multiplier));
    file.write(reinterpret_cast<const char*>(m_prgRamExt.data()), m_prgRamExt.size());
}

void Mapper005::loadState(std::ifstream& file) {
    file.read(reinterpret_cast<char*>(&m_prgMode), sizeof(m_prgMode));
    file.read(reinterpret_cast<char*>(m_prgBankRegs), sizeof(m_prgBankRegs));
    file.read(reinterpret_cast<char*>(&m_prgRamProtect1), sizeof(m_prgRamProtect1));
    file.read(reinterpret_cast<char*>(&m_prgRamProtect2), sizeof(m_prgRamProtect2));
    file.read(reinterpret_cast<char*>(&m_chrMode), sizeof(m_chrMode));
    file.read(reinterpret_cast<char*>(m_chrBankRegs), sizeof(m_chrBankRegs));
    file.read(reinterpret_cast<char*>(&m_chrBankHigh), sizeof(m_chrBankHigh));
    file.read(reinterpret_cast<char*>(&m_nametableMapping), sizeof(m_nametableMapping));
    file.read(reinterpret_cast<char*>(&m_fillModeTile), sizeof(m_fillModeTile));
    file.read(reinterpret_cast<char*>(&m_fillModeAttr), sizeof(m_fillModeAttr));
    file.read(reinterpret_cast<char*>(m_exRam.data()), m_exRam.size());
    file.read(reinterpret_cast<char*>(&m_exRamMode), sizeof(m_exRamMode));
    file.read(reinterpret_cast<char*>(&m_irqScanline), sizeof(m_irqScanline));
    file.read(reinterpret_cast<char*>(&m_irqStatus), sizeof(m_irqStatus));
    file.read(reinterpret_cast<char*>(&m_irqEnable), sizeof(m_irqEnable));
    file.read(reinterpret_cast<char*>(&m_inFrame), sizeof(m_inFrame));
    file.read(reinterpret_cast<char*>(&m_scanlineCounter), sizeof(m_scanlineCounter));
    file.read(reinterpret_cast<char*>(&m_multiplicand), sizeof(m_multiplicand));
    file.read(reinterpret_cast<char*>(&m_multiplier), sizeof(m_multiplier));
    file.read(reinterpret_cast<char*>(m_prgRamExt.data()), m_prgRamExt.size());
    updatePRGBanks();
    updateCHRBanks();
}

// ========== Mapper 23: VRC2b/VRC4e/VRC4f ==========

Mapper023::Mapper023(Cartridge* cartridge)
    : Mapper(cartridge)
    , m_prgSwapMode(0)
    , m_mirrorMode(MirrorMode::VERTICAL)
    , m_irqLatch(0)
    , m_irqCounter(0)
    , m_irqPrescaler(0)
    , m_irqPrescalerCounter(0)
    , m_irqEnable(false)
    , m_irqEnableOnAck(false)
    , m_irqMode(false) {
    std::memset(m_prgBank, 0, sizeof(m_prgBank));
    std::memset(m_chrBank, 0, sizeof(m_chrBank));
    std::memset(m_chrBankHigh, 0, sizeof(m_chrBankHigh));
    std::memset(m_prgBankOffset, 0, sizeof(m_prgBankOffset));
    std::memset(m_chrBankOffset, 0, sizeof(m_chrBankOffset));
}

void Mapper023::reset() {
    std::memset(m_prgBank, 0, sizeof(m_prgBank));
    std::memset(m_chrBank, 0, sizeof(m_chrBank));
    std::memset(m_chrBankHigh, 0, sizeof(m_chrBankHigh));
    m_prgSwapMode = 0;
    m_mirrorMode = MirrorMode::VERTICAL;
    m_irqLatch = 0;
    m_irqCounter = 0;
    m_irqPrescaler = 0;
    m_irqPrescalerCounter = 0;
    m_irqEnable = false;
    m_irqEnableOnAck = false;
    m_irqMode = false;
    m_irqActive = false;
    updateBanks();
}

void Mapper023::updateBanks() {
    const auto& prg = m_cartridge->getPRG();
    const auto& chr = m_cartridge->getCHR();
    
    u32 prgBanks8k = prg.size() / 0x2000;
    u32 chrBanks1k = chr.size() / 0x400;
    if (chrBanks1k == 0) chrBanks1k = 8;  // CHR RAM
    
    // PRG banks
    if (m_prgSwapMode & 0x02) {
        // Swap mode: $C000 switchable, $8000 fixed to second-to-last
        m_prgBankOffset[0] = (prgBanks8k - 2) * 0x2000;
        m_prgBankOffset[1] = (m_prgBank[1] % prgBanks8k) * 0x2000;
        m_prgBankOffset[2] = (m_prgBank[0] % prgBanks8k) * 0x2000;
        m_prgBankOffset[3] = (prgBanks8k - 1) * 0x2000;
    } else {
        // Normal mode: $8000 switchable, $C000 fixed to second-to-last
        m_prgBankOffset[0] = (m_prgBank[0] % prgBanks8k) * 0x2000;
        m_prgBankOffset[1] = (m_prgBank[1] % prgBanks8k) * 0x2000;
        m_prgBankOffset[2] = (prgBanks8k - 2) * 0x2000;
        m_prgBankOffset[3] = (prgBanks8k - 1) * 0x2000;
    }
    
    // CHR banks (combine low and high nibbles)
    for (int i = 0; i < 8; i++) {
        u8 bank = m_chrBank[i] | (m_chrBankHigh[i] << 4);
        m_chrBankOffset[i] = (bank % chrBanks1k) * 0x400;
    }
}

u8 Mapper023::cpuRead(u16 address) {
    if (address >= 0x6000 && address < 0x8000) {
        return m_cartridge->getPRGRAM()[address & 0x1FFF];
    } else if (address >= 0x8000) {
        u8 bank = (address - 0x8000) / 0x2000;
        u16 offset = address & 0x1FFF;
        return m_cartridge->getPRG()[m_prgBankOffset[bank] + offset];
    }
    return 0;
}

void Mapper023::cpuWrite(u16 address, u8 value) {
    if (address >= 0x6000 && address < 0x8000) {
        m_cartridge->getPRGRAM()[address & 0x1FFF] = value;
        return;
    }
    
    // VRC2b/VRC4 address line mapping: A0 and A1 swapped
    // Mapper 23 uses A0 and A1 directly
    u16 reg = (address & 0xF000) | ((address & 0x03));
    
    switch (reg) {
        case 0x8000: case 0x8001: case 0x8002: case 0x8003:
            m_prgBank[0] = value & 0x1F;
            updateBanks();
            break;
            
        case 0x9000: case 0x9001:
            switch (value & 0x03) {
                case 0: m_mirrorMode = MirrorMode::VERTICAL; break;
                case 1: m_mirrorMode = MirrorMode::HORIZONTAL; break;
                case 2: m_mirrorMode = MirrorMode::SINGLE_SCREEN_A; break;
                case 3: m_mirrorMode = MirrorMode::SINGLE_SCREEN_B; break;
            }
            break;
            
        case 0x9002: case 0x9003:
            m_prgSwapMode = value;
            updateBanks();
            break;
            
        case 0xA000: case 0xA001: case 0xA002: case 0xA003:
            m_prgBank[1] = value & 0x1F;
            updateBanks();
            break;
            
        case 0xB000:
            m_chrBank[0] = value & 0x0F;
            updateBanks();
            break;
        case 0xB001:
            m_chrBankHigh[0] = value & 0x1F;
            updateBanks();
            break;
        case 0xB002:
            m_chrBank[1] = value & 0x0F;
            updateBanks();
            break;
        case 0xB003:
            m_chrBankHigh[1] = value & 0x1F;
            updateBanks();
            break;
            
        case 0xC000:
            m_chrBank[2] = value & 0x0F;
            updateBanks();
            break;
        case 0xC001:
            m_chrBankHigh[2] = value & 0x1F;
            updateBanks();
            break;
        case 0xC002:
            m_chrBank[3] = value & 0x0F;
            updateBanks();
            break;
        case 0xC003:
            m_chrBankHigh[3] = value & 0x1F;
            updateBanks();
            break;
            
        case 0xD000:
            m_chrBank[4] = value & 0x0F;
            updateBanks();
            break;
        case 0xD001:
            m_chrBankHigh[4] = value & 0x1F;
            updateBanks();
            break;
        case 0xD002:
            m_chrBank[5] = value & 0x0F;
            updateBanks();
            break;
        case 0xD003:
            m_chrBankHigh[5] = value & 0x1F;
            updateBanks();
            break;
            
        case 0xE000:
            m_chrBank[6] = value & 0x0F;
            updateBanks();
            break;
        case 0xE001:
            m_chrBankHigh[6] = value & 0x1F;
            updateBanks();
            break;
        case 0xE002:
            m_chrBank[7] = value & 0x0F;
            updateBanks();
            break;
        case 0xE003:
            m_chrBankHigh[7] = value & 0x1F;
            updateBanks();
            break;
            
        case 0xF000:
            m_irqLatch = (m_irqLatch & 0xF0) | (value & 0x0F);
            break;
        case 0xF001:
            m_irqLatch = (m_irqLatch & 0x0F) | ((value & 0x0F) << 4);
            break;
        case 0xF002:
            m_irqEnableOnAck = (value & 0x01) != 0;
            m_irqEnable = (value & 0x02) != 0;
            m_irqMode = (value & 0x04) != 0;
            if (m_irqEnable) {
                m_irqCounter = m_irqLatch;
                m_irqPrescalerCounter = 341;
            }
            m_irqActive = false;
            break;
        case 0xF003:
            m_irqEnable = m_irqEnableOnAck;
            m_irqActive = false;
            break;
    }
}

u8 Mapper023::readCHR(u16 address) {
    u8 bank = address / 0x400;
    u16 offset = address & 0x3FF;
    return m_cartridge->getCHR()[m_chrBankOffset[bank] + offset];
}

void Mapper023::writeCHR(u16 address, u8 value) {
    u8 bank = address / 0x400;
    u16 offset = address & 0x3FF;
    m_cartridge->getCHR()[m_chrBankOffset[bank] + offset] = value;
}

MirrorMode Mapper023::getMirrorMode() const {
    return m_mirrorMode;
}

void Mapper023::scanlineCounter() {
    if (!m_irqEnable) return;
    
    if (m_irqMode) {
        // Cycle mode
        m_irqPrescalerCounter--;
        if (m_irqPrescalerCounter <= 0) {
            m_irqPrescalerCounter = 341;
            if (m_irqCounter == 0xFF) {
                m_irqCounter = m_irqLatch;
                m_irqActive = true;
            } else {
                m_irqCounter++;
            }
        }
    } else {
        // Scanline mode
        if (m_irqCounter == 0xFF) {
            m_irqCounter = m_irqLatch;
            m_irqActive = true;
        } else {
            m_irqCounter++;
        }
    }
}

void Mapper023::saveState(std::ofstream& file) const {
    file.write(reinterpret_cast<const char*>(m_prgBank), sizeof(m_prgBank));
    file.write(reinterpret_cast<const char*>(m_chrBank), sizeof(m_chrBank));
    file.write(reinterpret_cast<const char*>(m_chrBankHigh), sizeof(m_chrBankHigh));
    file.write(reinterpret_cast<const char*>(&m_prgSwapMode), sizeof(m_prgSwapMode));
    file.write(reinterpret_cast<const char*>(&m_mirrorMode), sizeof(m_mirrorMode));
    file.write(reinterpret_cast<const char*>(&m_irqLatch), sizeof(m_irqLatch));
    file.write(reinterpret_cast<const char*>(&m_irqCounter), sizeof(m_irqCounter));
    file.write(reinterpret_cast<const char*>(&m_irqPrescaler), sizeof(m_irqPrescaler));
    file.write(reinterpret_cast<const char*>(&m_irqPrescalerCounter), sizeof(m_irqPrescalerCounter));
    file.write(reinterpret_cast<const char*>(&m_irqEnable), sizeof(m_irqEnable));
    file.write(reinterpret_cast<const char*>(&m_irqEnableOnAck), sizeof(m_irqEnableOnAck));
    file.write(reinterpret_cast<const char*>(&m_irqMode), sizeof(m_irqMode));
}

void Mapper023::loadState(std::ifstream& file) {
    file.read(reinterpret_cast<char*>(m_prgBank), sizeof(m_prgBank));
    file.read(reinterpret_cast<char*>(m_chrBank), sizeof(m_chrBank));
    file.read(reinterpret_cast<char*>(m_chrBankHigh), sizeof(m_chrBankHigh));
    file.read(reinterpret_cast<char*>(&m_prgSwapMode), sizeof(m_prgSwapMode));
    file.read(reinterpret_cast<char*>(&m_mirrorMode), sizeof(m_mirrorMode));
    file.read(reinterpret_cast<char*>(&m_irqLatch), sizeof(m_irqLatch));
    file.read(reinterpret_cast<char*>(&m_irqCounter), sizeof(m_irqCounter));
    file.read(reinterpret_cast<char*>(&m_irqPrescaler), sizeof(m_irqPrescaler));
    file.read(reinterpret_cast<char*>(&m_irqPrescalerCounter), sizeof(m_irqPrescalerCounter));
    file.read(reinterpret_cast<char*>(&m_irqEnable), sizeof(m_irqEnable));
    file.read(reinterpret_cast<char*>(&m_irqEnableOnAck), sizeof(m_irqEnableOnAck));
    file.read(reinterpret_cast<char*>(&m_irqMode), sizeof(m_irqMode));
    updateBanks();
}

// ========== VRC6 Audio Channels ==========

void VRC6Pulse::reset() {
    volume = 0;
    duty = 0;
    period = 0;
    timer = 0;
    step = 0;
    enabled = false;
    mode = false;
}

void VRC6Pulse::clockTimer() {
    if (!enabled) return;
    
    if (timer == 0) {
        timer = period;
        // Advance step (16 steps)
        step = (step + 1) & 0x0F;
    } else {
        timer--;
    }
}

u8 VRC6Pulse::output() const {
    if (!enabled) return 0;
    if (period < 1) return 0;  // Prevent ultrasonic frequencies
    
    // VRC6 pulse has 16 steps with variable duty cycle
    // duty value 0-7 means output high for (duty+1) steps out of 16
    // When mode bit is set, output is always the volume (no duty cycle)
    if (mode) {
        return volume;
    }
    
    // step goes 0-15, output high if step <= duty
    if (step <= duty) {
        return volume;
    }
    return 0;
}

void VRC6Sawtooth::reset() {
    accumRate = 0;
    period = 0;
    timer = 0;
    accumulator = 0;
    step = 0;
    enabled = false;
}

void VRC6Sawtooth::clockTimer() {
    if (!enabled) return;
    
    if (timer == 0) {
        timer = period;
        
        // Accumulator is clocked every 2 steps
        step++;
        if ((step & 1) == 0) {
            // Add rate to accumulator (on even steps)
            accumulator += accumRate;
        }
        
        // Reset on step 14
        if (step >= 14) {
            step = 0;
            accumulator = 0;
        }
    } else {
        timer--;
    }
}

u8 VRC6Sawtooth::output() const {
    if (!enabled) return 0;
    if (period < 1) return 0;
    
    // Output is the top 5 bits of the accumulator
    return (accumulator >> 3) & 0x1F;
}

// ========== Mapper 24: VRC6a ==========

Mapper024::Mapper024(Cartridge* cartridge)
    : Mapper(cartridge)
    , m_prgBank16k(0)
    , m_prgBank8k(0)
    , m_mirrorMode(MirrorMode::VERTICAL)
    , m_irqLatch(0)
    , m_irqCounter(0)
    , m_irqPrescaler(0)
    , m_irqPrescalerCounter(0)
    , m_irqEnable(false)
    , m_irqEnableOnAck(false)
    , m_irqMode(false)
    , m_audioHalt(false) {
    std::memset(m_chrBank, 0, sizeof(m_chrBank));
    std::memset(m_prgBankOffset, 0, sizeof(m_prgBankOffset));
    std::memset(m_chrBankOffset, 0, sizeof(m_chrBankOffset));
    m_vrcPulse1.reset();
    m_vrcPulse2.reset();
    m_vrcSaw.reset();
}

void Mapper024::reset() {
    m_prgBank16k = 0;
    m_prgBank8k = 0;
    std::memset(m_chrBank, 0, sizeof(m_chrBank));
    m_mirrorMode = MirrorMode::VERTICAL;
    m_irqLatch = 0;
    m_irqCounter = 0;
    m_irqPrescaler = 0;
    m_irqPrescalerCounter = 0;
    m_irqEnable = false;
    m_irqEnableOnAck = false;
    m_irqMode = false;
    m_irqActive = false;
    
    // Audio reset
    m_vrcPulse1.reset();
    m_vrcPulse2.reset();
    m_vrcSaw.reset();
    m_audioHalt = false;
    
    updateBanks();
}

void Mapper024::updateBanks() {
    const auto& prg = m_cartridge->getPRG();
    const auto& chr = m_cartridge->getCHR();
    
    u32 prgBanks16k = prg.size() / 0x4000;
    u32 prgBanks8k = prg.size() / 0x2000;
    u32 chrBanks1k = chr.size() / 0x400;
    if (chrBanks1k == 0) chrBanks1k = 8;
    
    // PRG: 16KB at $8000, 8KB at $C000, fixed 8KB at $E000
    m_prgBankOffset[0] = (m_prgBank16k % prgBanks16k) * 0x4000;
    m_prgBankOffset[1] = m_prgBankOffset[0] + 0x2000;
    m_prgBankOffset[2] = (m_prgBank8k % prgBanks8k) * 0x2000;
    m_prgBankOffset[3] = (prgBanks8k - 1) * 0x2000;
    
    // CHR: 1KB banks
    for (int i = 0; i < 8; i++) {
        m_chrBankOffset[i] = (m_chrBank[i] % chrBanks1k) * 0x400;
    }
}

u8 Mapper024::cpuRead(u16 address) {
    if (address >= 0x6000 && address < 0x8000) {
        return m_cartridge->getPRGRAM()[address & 0x1FFF];
    } else if (address >= 0x8000) {
        u8 bank = (address - 0x8000) / 0x2000;
        u16 offset = address & 0x1FFF;
        return m_cartridge->getPRG()[m_prgBankOffset[bank] + offset];
    }
    return 0;
}

void Mapper024::cpuWrite(u16 address, u8 value) {
    if (address >= 0x6000 && address < 0x8000) {
        m_cartridge->getPRGRAM()[address & 0x1FFF] = value;
        return;
    }
    
    switch (address) {
        case 0x8000: case 0x8001: case 0x8002: case 0x8003:
            m_prgBank16k = value & 0x0F;
            updateBanks();
            break;
            
        // VRC6 Audio Pulse 1
        case 0x9000:
            m_vrcPulse1.volume = value & 0x0F;
            m_vrcPulse1.duty = (value >> 4) & 0x07;
            m_vrcPulse1.mode = (value & 0x80) != 0;
            break;
        case 0x9001:
            m_vrcPulse1.period = (m_vrcPulse1.period & 0xF00) | value;
            break;
        case 0x9002:
            m_vrcPulse1.period = (m_vrcPulse1.period & 0x0FF) | ((value & 0x0F) << 8);
            m_vrcPulse1.enabled = (value & 0x80) != 0;
            break;
            
        // VRC6 Audio Pulse 2
        case 0xA000:
            m_vrcPulse2.volume = value & 0x0F;
            m_vrcPulse2.duty = (value >> 4) & 0x07;
            m_vrcPulse2.mode = (value & 0x80) != 0;
            break;
        case 0xA001:
            m_vrcPulse2.period = (m_vrcPulse2.period & 0xF00) | value;
            break;
        case 0xA002:
            m_vrcPulse2.period = (m_vrcPulse2.period & 0x0FF) | ((value & 0x0F) << 8);
            m_vrcPulse2.enabled = (value & 0x80) != 0;
            break;
            
        // VRC6 Audio Sawtooth
        case 0xB000:
            m_vrcSaw.accumRate = value & 0x3F;
            break;
        case 0xB001:
            m_vrcSaw.period = (m_vrcSaw.period & 0xF00) | value;
            break;
        case 0xB002:
            m_vrcSaw.period = (m_vrcSaw.period & 0x0FF) | ((value & 0x0F) << 8);
            m_vrcSaw.enabled = (value & 0x80) != 0;
            break;
            
        case 0xB003:
            // Bits 0-1: PPU banking style (ignored for now)
            // Bits 2-3: Mirroring
            switch ((value >> 2) & 0x03) {
                case 0: m_mirrorMode = MirrorMode::VERTICAL; break;
                case 1: m_mirrorMode = MirrorMode::HORIZONTAL; break;
                case 2: m_mirrorMode = MirrorMode::SINGLE_SCREEN_A; break;
                case 3: m_mirrorMode = MirrorMode::SINGLE_SCREEN_B; break;
            }
            // Bit 4: Audio halt
            m_audioHalt = (value & 0x10) != 0;
            break;
            
        case 0xC000: case 0xC001: case 0xC002: case 0xC003:
            m_prgBank8k = value & 0x1F;
            updateBanks();
            break;
            
        case 0xD000:
            m_chrBank[0] = value;
            updateBanks();
            break;
        case 0xD001:
            m_chrBank[1] = value;
            updateBanks();
            break;
        case 0xD002:
            m_chrBank[2] = value;
            updateBanks();
            break;
        case 0xD003:
            m_chrBank[3] = value;
            updateBanks();
            break;
        case 0xE000:
            m_chrBank[4] = value;
            updateBanks();
            break;
        case 0xE001:
            m_chrBank[5] = value;
            updateBanks();
            break;
        case 0xE002:
            m_chrBank[6] = value;
            updateBanks();
            break;
        case 0xE003:
            m_chrBank[7] = value;
            updateBanks();
            break;
            
        case 0xF000:
            m_irqLatch = value;
            break;
        case 0xF001:
            m_irqEnableOnAck = (value & 0x01) != 0;
            m_irqEnable = (value & 0x02) != 0;
            m_irqMode = (value & 0x04) != 0;
            if (m_irqEnable) {
                m_irqCounter = m_irqLatch;
                m_irqPrescalerCounter = 341;
            }
            m_irqActive = false;
            break;
        case 0xF002:
            m_irqEnable = m_irqEnableOnAck;
            m_irqActive = false;
            break;
    }
}

void Mapper024::clockAudio() {
    if (m_audioHalt) return;
    
    m_vrcPulse1.clockTimer();
    m_vrcPulse2.clockTimer();
    m_vrcSaw.clockTimer();
}

float Mapper024::getAudioOutput() const {
    if (m_audioHalt) return 0.0f;
    
    // Get raw outputs (0-15 for pulse, 0-31 for saw)
    u8 pulse1 = m_vrcPulse1.output();
    u8 pulse2 = m_vrcPulse2.output();
    u8 saw = m_vrcSaw.output();
    
    // Mix channels - VRC6 has louder output than internal APU
    // Scale to roughly 0.0-0.5 range (VRC6 is about 50% of total output when combined)
    // Pulse channels are 4-bit (0-15), saw is 5-bit (0-31)
    float pulseOut = (pulse1 + pulse2) / 30.0f;  // Max 30, normalize
    float sawOut = saw / 31.0f;
    
    // VRC6 mixing is roughly equal weighted between pulses and saw
    float output = (pulseOut * 0.5f + sawOut * 0.5f) * 0.5f;
    
    return output;
}

u8 Mapper024::readCHR(u16 address) {
    u8 bank = address / 0x400;
    u16 offset = address & 0x3FF;
    return m_cartridge->getCHR()[m_chrBankOffset[bank] + offset];
}

void Mapper024::writeCHR(u16 address, u8 value) {
    u8 bank = address / 0x400;
    u16 offset = address & 0x3FF;
    m_cartridge->getCHR()[m_chrBankOffset[bank] + offset] = value;
}

MirrorMode Mapper024::getMirrorMode() const {
    return m_mirrorMode;
}

void Mapper024::scanlineCounter() {
    if (!m_irqEnable) return;
    
    if (m_irqMode) {
        // Cycle mode
        m_irqPrescalerCounter--;
        if (m_irqPrescalerCounter <= 0) {
            m_irqPrescalerCounter = 341;
            if (m_irqCounter == 0xFF) {
                m_irqCounter = m_irqLatch;
                m_irqActive = true;
            } else {
                m_irqCounter++;
            }
        }
    } else {
        // Scanline mode
        if (m_irqCounter == 0xFF) {
            m_irqCounter = m_irqLatch;
            m_irqActive = true;
        } else {
            m_irqCounter++;
        }
    }
}

void Mapper024::saveState(std::ofstream& file) const {
    file.write(reinterpret_cast<const char*>(&m_prgBank16k), sizeof(m_prgBank16k));
    file.write(reinterpret_cast<const char*>(&m_prgBank8k), sizeof(m_prgBank8k));
    file.write(reinterpret_cast<const char*>(m_chrBank), sizeof(m_chrBank));
    file.write(reinterpret_cast<const char*>(&m_mirrorMode), sizeof(m_mirrorMode));
    file.write(reinterpret_cast<const char*>(&m_irqLatch), sizeof(m_irqLatch));
    file.write(reinterpret_cast<const char*>(&m_irqCounter), sizeof(m_irqCounter));
    file.write(reinterpret_cast<const char*>(&m_irqPrescaler), sizeof(m_irqPrescaler));
    file.write(reinterpret_cast<const char*>(&m_irqPrescalerCounter), sizeof(m_irqPrescalerCounter));
    file.write(reinterpret_cast<const char*>(&m_irqEnable), sizeof(m_irqEnable));
    file.write(reinterpret_cast<const char*>(&m_irqEnableOnAck), sizeof(m_irqEnableOnAck));
    file.write(reinterpret_cast<const char*>(&m_irqMode), sizeof(m_irqMode));
    // VRC6 Audio state
    file.write(reinterpret_cast<const char*>(&m_vrcPulse1), sizeof(m_vrcPulse1));
    file.write(reinterpret_cast<const char*>(&m_vrcPulse2), sizeof(m_vrcPulse2));
    file.write(reinterpret_cast<const char*>(&m_vrcSaw), sizeof(m_vrcSaw));
    file.write(reinterpret_cast<const char*>(&m_audioHalt), sizeof(m_audioHalt));
}

void Mapper024::loadState(std::ifstream& file) {
    file.read(reinterpret_cast<char*>(&m_prgBank16k), sizeof(m_prgBank16k));
    file.read(reinterpret_cast<char*>(&m_prgBank8k), sizeof(m_prgBank8k));
    file.read(reinterpret_cast<char*>(m_chrBank), sizeof(m_chrBank));
    file.read(reinterpret_cast<char*>(&m_mirrorMode), sizeof(m_mirrorMode));
    file.read(reinterpret_cast<char*>(&m_irqLatch), sizeof(m_irqLatch));
    file.read(reinterpret_cast<char*>(&m_irqCounter), sizeof(m_irqCounter));
    file.read(reinterpret_cast<char*>(&m_irqPrescaler), sizeof(m_irqPrescaler));
    file.read(reinterpret_cast<char*>(&m_irqPrescalerCounter), sizeof(m_irqPrescalerCounter));
    file.read(reinterpret_cast<char*>(&m_irqEnable), sizeof(m_irqEnable));
    file.read(reinterpret_cast<char*>(&m_irqEnableOnAck), sizeof(m_irqEnableOnAck));
    file.read(reinterpret_cast<char*>(&m_irqMode), sizeof(m_irqMode));
    // VRC6 Audio state
    file.read(reinterpret_cast<char*>(&m_vrcPulse1), sizeof(m_vrcPulse1));
    file.read(reinterpret_cast<char*>(&m_vrcPulse2), sizeof(m_vrcPulse2));
    file.read(reinterpret_cast<char*>(&m_vrcSaw), sizeof(m_vrcSaw));
    file.read(reinterpret_cast<char*>(&m_audioHalt), sizeof(m_audioHalt));
    updateBanks();
}

// ========== Mapper 25: VRC4b/VRC4d ==========

Mapper025::Mapper025(Cartridge* cartridge)
    : Mapper(cartridge)
    , m_prgSwapMode(0)
    , m_mirrorMode(MirrorMode::VERTICAL)
    , m_irqLatch(0)
    , m_irqCounter(0)
    , m_irqPrescaler(0)
    , m_irqPrescalerCounter(0)
    , m_irqEnable(false)
    , m_irqEnableOnAck(false)
    , m_irqMode(false) {
    std::memset(m_prgBank, 0, sizeof(m_prgBank));
    std::memset(m_chrBank, 0, sizeof(m_chrBank));
    std::memset(m_chrBankHigh, 0, sizeof(m_chrBankHigh));
    std::memset(m_prgBankOffset, 0, sizeof(m_prgBankOffset));
    std::memset(m_chrBankOffset, 0, sizeof(m_chrBankOffset));
}

void Mapper025::reset() {
    std::memset(m_prgBank, 0, sizeof(m_prgBank));
    std::memset(m_chrBank, 0, sizeof(m_chrBank));
    std::memset(m_chrBankHigh, 0, sizeof(m_chrBankHigh));
    m_prgSwapMode = 0;
    m_mirrorMode = MirrorMode::VERTICAL;
    m_irqLatch = 0;
    m_irqCounter = 0;
    m_irqPrescaler = 0;
    m_irqPrescalerCounter = 0;
    m_irqEnable = false;
    m_irqEnableOnAck = false;
    m_irqMode = false;
    m_irqActive = false;
    updateBanks();
}

void Mapper025::updateBanks() {
    const auto& prg = m_cartridge->getPRG();
    const auto& chr = m_cartridge->getCHR();
    
    u32 prgBanks8k = prg.size() / 0x2000;
    u32 chrBanks1k = chr.size() / 0x400;
    if (chrBanks1k == 0) chrBanks1k = 8;
    
    // PRG banks
    if (m_prgSwapMode & 0x02) {
        m_prgBankOffset[0] = (prgBanks8k - 2) * 0x2000;
        m_prgBankOffset[1] = (m_prgBank[1] % prgBanks8k) * 0x2000;
        m_prgBankOffset[2] = (m_prgBank[0] % prgBanks8k) * 0x2000;
        m_prgBankOffset[3] = (prgBanks8k - 1) * 0x2000;
    } else {
        m_prgBankOffset[0] = (m_prgBank[0] % prgBanks8k) * 0x2000;
        m_prgBankOffset[1] = (m_prgBank[1] % prgBanks8k) * 0x2000;
        m_prgBankOffset[2] = (prgBanks8k - 2) * 0x2000;
        m_prgBankOffset[3] = (prgBanks8k - 1) * 0x2000;
    }
    
    // CHR banks
    for (int i = 0; i < 8; i++) {
        u8 bank = m_chrBank[i] | (m_chrBankHigh[i] << 4);
        m_chrBankOffset[i] = (bank % chrBanks1k) * 0x400;
    }
}

u8 Mapper025::cpuRead(u16 address) {
    if (address >= 0x6000 && address < 0x8000) {
        return m_cartridge->getPRGRAM()[address & 0x1FFF];
    } else if (address >= 0x8000) {
        u8 bank = (address - 0x8000) / 0x2000;
        u16 offset = address & 0x1FFF;
        return m_cartridge->getPRG()[m_prgBankOffset[bank] + offset];
    }
    return 0;
}

void Mapper025::cpuWrite(u16 address, u8 value) {
    if (address >= 0x6000 && address < 0x8000) {
        m_cartridge->getPRGRAM()[address & 0x1FFF] = value;
        return;
    }
    
    // Mapper 25 (VRC4b/VRC4d) swaps A0 and A1
    // VRC4b: A1, A0 -> A0, A1 (swap)
    // VRC4d: A3, A2 -> A0, A1
    u16 a0 = (address >> 1) & 0x01;  // A1 -> bit 0
    u16 a1 = (address >> 0) & 0x01;  // A0 -> bit 1
    u16 reg = (address & 0xF000) | (a1 << 1) | a0;
    
    switch (reg) {
        case 0x8000: case 0x8001: case 0x8002: case 0x8003:
            m_prgBank[0] = value & 0x1F;
            updateBanks();
            break;
            
        case 0x9000: case 0x9001:
            switch (value & 0x03) {
                case 0: m_mirrorMode = MirrorMode::VERTICAL; break;
                case 1: m_mirrorMode = MirrorMode::HORIZONTAL; break;
                case 2: m_mirrorMode = MirrorMode::SINGLE_SCREEN_A; break;
                case 3: m_mirrorMode = MirrorMode::SINGLE_SCREEN_B; break;
            }
            break;
            
        case 0x9002: case 0x9003:
            m_prgSwapMode = value;
            updateBanks();
            break;
            
        case 0xA000: case 0xA001: case 0xA002: case 0xA003:
            m_prgBank[1] = value & 0x1F;
            updateBanks();
            break;
            
        case 0xB000:
            m_chrBank[0] = value & 0x0F;
            updateBanks();
            break;
        case 0xB001:
            m_chrBankHigh[0] = value & 0x1F;
            updateBanks();
            break;
        case 0xB002:
            m_chrBank[1] = value & 0x0F;
            updateBanks();
            break;
        case 0xB003:
            m_chrBankHigh[1] = value & 0x1F;
            updateBanks();
            break;
            
        case 0xC000:
            m_chrBank[2] = value & 0x0F;
            updateBanks();
            break;
        case 0xC001:
            m_chrBankHigh[2] = value & 0x1F;
            updateBanks();
            break;
        case 0xC002:
            m_chrBank[3] = value & 0x0F;
            updateBanks();
            break;
        case 0xC003:
            m_chrBankHigh[3] = value & 0x1F;
            updateBanks();
            break;
            
        case 0xD000:
            m_chrBank[4] = value & 0x0F;
            updateBanks();
            break;
        case 0xD001:
            m_chrBankHigh[4] = value & 0x1F;
            updateBanks();
            break;
        case 0xD002:
            m_chrBank[5] = value & 0x0F;
            updateBanks();
            break;
        case 0xD003:
            m_chrBankHigh[5] = value & 0x1F;
            updateBanks();
            break;
            
        case 0xE000:
            m_chrBank[6] = value & 0x0F;
            updateBanks();
            break;
        case 0xE001:
            m_chrBankHigh[6] = value & 0x1F;
            updateBanks();
            break;
        case 0xE002:
            m_chrBank[7] = value & 0x0F;
            updateBanks();
            break;
        case 0xE003:
            m_chrBankHigh[7] = value & 0x1F;
            updateBanks();
            break;
            
        case 0xF000:
            m_irqLatch = (m_irqLatch & 0xF0) | (value & 0x0F);
            break;
        case 0xF001:
            m_irqLatch = (m_irqLatch & 0x0F) | ((value & 0x0F) << 4);
            break;
        case 0xF002:
            m_irqEnableOnAck = (value & 0x01) != 0;
            m_irqEnable = (value & 0x02) != 0;
            m_irqMode = (value & 0x04) != 0;
            if (m_irqEnable) {
                m_irqCounter = m_irqLatch;
                m_irqPrescalerCounter = 341;
            }
            m_irqActive = false;
            break;
        case 0xF003:
            m_irqEnable = m_irqEnableOnAck;
            m_irqActive = false;
            break;
    }
}

u8 Mapper025::readCHR(u16 address) {
    u8 bank = address / 0x400;
    u16 offset = address & 0x3FF;
    return m_cartridge->getCHR()[m_chrBankOffset[bank] + offset];
}

void Mapper025::writeCHR(u16 address, u8 value) {
    u8 bank = address / 0x400;
    u16 offset = address & 0x3FF;
    m_cartridge->getCHR()[m_chrBankOffset[bank] + offset] = value;
}

MirrorMode Mapper025::getMirrorMode() const {
    return m_mirrorMode;
}

void Mapper025::scanlineCounter() {
    if (!m_irqEnable) return;
    
    if (m_irqMode) {
        m_irqPrescalerCounter--;
        if (m_irqPrescalerCounter <= 0) {
            m_irqPrescalerCounter = 341;
            if (m_irqCounter == 0xFF) {
                m_irqCounter = m_irqLatch;
                m_irqActive = true;
            } else {
                m_irqCounter++;
            }
        }
    } else {
        if (m_irqCounter == 0xFF) {
            m_irqCounter = m_irqLatch;
            m_irqActive = true;
        } else {
            m_irqCounter++;
        }
    }
}

void Mapper025::saveState(std::ofstream& file) const {
    file.write(reinterpret_cast<const char*>(m_prgBank), sizeof(m_prgBank));
    file.write(reinterpret_cast<const char*>(m_chrBank), sizeof(m_chrBank));
    file.write(reinterpret_cast<const char*>(m_chrBankHigh), sizeof(m_chrBankHigh));
    file.write(reinterpret_cast<const char*>(&m_prgSwapMode), sizeof(m_prgSwapMode));
    file.write(reinterpret_cast<const char*>(&m_mirrorMode), sizeof(m_mirrorMode));
    file.write(reinterpret_cast<const char*>(&m_irqLatch), sizeof(m_irqLatch));
    file.write(reinterpret_cast<const char*>(&m_irqCounter), sizeof(m_irqCounter));
    file.write(reinterpret_cast<const char*>(&m_irqPrescaler), sizeof(m_irqPrescaler));
    file.write(reinterpret_cast<const char*>(&m_irqPrescalerCounter), sizeof(m_irqPrescalerCounter));
    file.write(reinterpret_cast<const char*>(&m_irqEnable), sizeof(m_irqEnable));
    file.write(reinterpret_cast<const char*>(&m_irqEnableOnAck), sizeof(m_irqEnableOnAck));
    file.write(reinterpret_cast<const char*>(&m_irqMode), sizeof(m_irqMode));
}

void Mapper025::loadState(std::ifstream& file) {
    file.read(reinterpret_cast<char*>(m_prgBank), sizeof(m_prgBank));
    file.read(reinterpret_cast<char*>(m_chrBank), sizeof(m_chrBank));
    file.read(reinterpret_cast<char*>(m_chrBankHigh), sizeof(m_chrBankHigh));
    file.read(reinterpret_cast<char*>(&m_prgSwapMode), sizeof(m_prgSwapMode));
    file.read(reinterpret_cast<char*>(&m_mirrorMode), sizeof(m_mirrorMode));
    file.read(reinterpret_cast<char*>(&m_irqLatch), sizeof(m_irqLatch));
    file.read(reinterpret_cast<char*>(&m_irqCounter), sizeof(m_irqCounter));
    file.read(reinterpret_cast<char*>(&m_irqPrescaler), sizeof(m_irqPrescaler));
    file.read(reinterpret_cast<char*>(&m_irqPrescalerCounter), sizeof(m_irqPrescalerCounter));
    file.read(reinterpret_cast<char*>(&m_irqEnable), sizeof(m_irqEnable));
    file.read(reinterpret_cast<char*>(&m_irqEnableOnAck), sizeof(m_irqEnableOnAck));
    file.read(reinterpret_cast<char*>(&m_irqMode), sizeof(m_irqMode));
    updateBanks();
}

// ========== Mapper 73: VRC3 ==========

Mapper073::Mapper073(Cartridge* cartridge)
    : Mapper(cartridge)
    , m_prgBank(0)
    , m_irqLatch(0)
    , m_irqCounter(0)
    , m_irqEnable(false)
    , m_irqEnableOnAck(false)
    , m_irqMode(false) {
}

void Mapper073::reset() {
    m_prgBank = 0;
    m_irqLatch = 0;
    m_irqCounter = 0;
    m_irqEnable = false;
    m_irqEnableOnAck = false;
    m_irqMode = false;
    m_irqActive = false;
}

u8 Mapper073::cpuRead(u16 address) {
    const auto& prg = m_cartridge->getPRG();
    
    if (address >= 0x6000 && address < 0x8000) {
        return m_cartridge->getPRGRAM()[address & 0x1FFF];
    } else if (address >= 0x8000 && address < 0xC000) {
        // Switchable 16KB bank
        u32 prgBanks16k = prg.size() / 0x4000;
        u32 offset = (m_prgBank % prgBanks16k) * 0x4000;
        return prg[offset + (address & 0x3FFF)];
    } else if (address >= 0xC000) {
        // Fixed to last 16KB bank
        return prg[prg.size() - 0x4000 + (address & 0x3FFF)];
    }
    return 0;
}

void Mapper073::cpuWrite(u16 address, u8 value) {
    if (address >= 0x6000 && address < 0x8000) {
        m_cartridge->getPRGRAM()[address & 0x1FFF] = value;
        return;
    }
    
    switch (address & 0xF000) {
        case 0x8000:
            // IRQ latch bits 0-3
            m_irqLatch = (m_irqLatch & 0xFFF0) | (value & 0x0F);
            break;
        case 0x9000:
            // IRQ latch bits 4-7
            m_irqLatch = (m_irqLatch & 0xFF0F) | ((value & 0x0F) << 4);
            break;
        case 0xA000:
            // IRQ latch bits 8-11
            m_irqLatch = (m_irqLatch & 0xF0FF) | ((value & 0x0F) << 8);
            break;
        case 0xB000:
            // IRQ latch bits 12-15
            m_irqLatch = (m_irqLatch & 0x0FFF) | ((value & 0x0F) << 12);
            break;
        case 0xC000:
            // IRQ control
            m_irqEnableOnAck = (value & 0x01) != 0;
            m_irqEnable = (value & 0x02) != 0;
            m_irqMode = (value & 0x04) != 0;  // 0 = 16-bit, 1 = 8-bit mode
            if (m_irqEnable) {
                m_irqCounter = m_irqLatch;
            }
            m_irqActive = false;
            break;
        case 0xD000:
            // IRQ acknowledge
            m_irqEnable = m_irqEnableOnAck;
            m_irqActive = false;
            break;
        case 0xF000:
            // PRG bank select
            m_prgBank = value & 0x0F;
            break;
    }
}

u8 Mapper073::readCHR(u16 address) {
    return m_cartridge->getCHR()[address & 0x1FFF];
}

void Mapper073::writeCHR(u16 address, u8 value) {
    m_cartridge->getCHR()[address & 0x1FFF] = value;
}

void Mapper073::scanlineCounter() {
    if (!m_irqEnable) return;
    
    // VRC3 IRQ counts CPU cycles, called ~341 times per scanline
    // Simplified: increment per scanline call
    if (m_irqMode) {
        // 8-bit mode (high byte only)
        u8 high = (m_irqCounter >> 8) & 0xFF;
        high++;
        if (high == 0) {
            m_irqCounter = m_irqLatch;
            m_irqActive = true;
        } else {
            m_irqCounter = (m_irqCounter & 0x00FF) | (high << 8);
        }
    } else {
        // 16-bit mode
        m_irqCounter++;
        if (m_irqCounter == 0) {
            m_irqCounter = m_irqLatch;
            m_irqActive = true;
        }
    }
}

void Mapper073::saveState(std::ofstream& file) const {
    file.write(reinterpret_cast<const char*>(&m_prgBank), sizeof(m_prgBank));
    file.write(reinterpret_cast<const char*>(&m_irqLatch), sizeof(m_irqLatch));
    file.write(reinterpret_cast<const char*>(&m_irqCounter), sizeof(m_irqCounter));
    file.write(reinterpret_cast<const char*>(&m_irqEnable), sizeof(m_irqEnable));
    file.write(reinterpret_cast<const char*>(&m_irqEnableOnAck), sizeof(m_irqEnableOnAck));
    file.write(reinterpret_cast<const char*>(&m_irqMode), sizeof(m_irqMode));
}

void Mapper073::loadState(std::ifstream& file) {
    file.read(reinterpret_cast<char*>(&m_prgBank), sizeof(m_prgBank));
    file.read(reinterpret_cast<char*>(&m_irqLatch), sizeof(m_irqLatch));
    file.read(reinterpret_cast<char*>(&m_irqCounter), sizeof(m_irqCounter));
    file.read(reinterpret_cast<char*>(&m_irqEnable), sizeof(m_irqEnable));
    file.read(reinterpret_cast<char*>(&m_irqEnableOnAck), sizeof(m_irqEnableOnAck));
    file.read(reinterpret_cast<char*>(&m_irqMode), sizeof(m_irqMode));
}

} // namespace nes

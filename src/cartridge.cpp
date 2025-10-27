#include "cartridge.h"
#include <fstream>
#include <iostream>

Cartridge::Cartridge()
    : m_cartridgeType(0)
    , m_romSize(0)
    , m_ramSize(0)
    , m_loaded(false)
    , m_currentRomBank(1)
    , m_currentRamBank(0)
    , m_ramEnabled(false)
    , m_bankingMode(0) {
}

Cartridge::~Cartridge() {
}

void Cartridge::saveState(std::ofstream& file) const {
    // Save RAM (if present)
    size_t ramSize = m_ram.size();
    file.write(reinterpret_cast<const char*>(&ramSize), sizeof(ramSize));
    if (ramSize > 0) {
        file.write(reinterpret_cast<const char*>(m_ram.data()), ramSize);
    }
    // Save MBC state
    file.write(reinterpret_cast<const char*>(&m_currentRomBank), sizeof(m_currentRomBank));
    file.write(reinterpret_cast<const char*>(&m_currentRamBank), sizeof(m_currentRamBank));
    file.write(reinterpret_cast<const char*>(&m_ramEnabled), sizeof(m_ramEnabled));
    file.write(reinterpret_cast<const char*>(&m_bankingMode), sizeof(m_bankingMode));
}

void Cartridge::loadState(std::ifstream& file) {
    // Load RAM
    size_t ramSize;
    file.read(reinterpret_cast<char*>(&ramSize), sizeof(ramSize));
    if (ramSize > 0 && ramSize == m_ram.size()) {
        file.read(reinterpret_cast<char*>(m_ram.data()), ramSize);
    }
    // Load MBC state
    file.read(reinterpret_cast<char*>(&m_currentRomBank), sizeof(m_currentRomBank));
    file.read(reinterpret_cast<char*>(&m_currentRamBank), sizeof(m_currentRamBank));
    file.read(reinterpret_cast<char*>(&m_ramEnabled), sizeof(m_ramEnabled));
    file.read(reinterpret_cast<char*>(&m_bankingMode), sizeof(m_bankingMode));
}

bool Cartridge::load(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "Failed to open ROM file: " << filename << std::endl;
        return false;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    m_rom.resize(size);
    if (!file.read(reinterpret_cast<char*>(m_rom.data()), size)) {
        std::cerr << "Failed to read ROM file" << std::endl;
        return false;
    }

    parseHeader();
    m_loaded = true;

    std::cout << "Loaded ROM: " << m_title << std::endl;
    std::cout << "Cartridge Type: 0x" << std::hex << (int)m_cartridgeType << std::dec << std::endl;
    
    return true;
}

void Cartridge::parseHeader() {
    // Title is at 0x0134-0x0143
    m_title.clear();
    for (int i = 0x0134; i <= 0x0143 && m_rom[i] != 0; i++) {
        m_title += static_cast<char>(m_rom[i]);
    }

    m_cartridgeType = m_rom[0x0147];
    m_romSize = m_rom[0x0148];
    m_ramSize = m_rom[0x0149];

    // Allocate RAM if needed
    if (m_ramSize > 0) {
        u32 ramBytes = 0;
        switch (m_ramSize) {
            case 0x01: ramBytes = 2048; break;      // 2KB
            case 0x02: ramBytes = 8192; break;      // 8KB
            case 0x03: ramBytes = 32768; break;     // 32KB (4 banks)
            case 0x04: ramBytes = 131072; break;    // 128KB (16 banks)
            case 0x05: ramBytes = 65536; break;     // 64KB (8 banks)
        }
        if (ramBytes > 0) {
            m_ram.resize(ramBytes, 0);
        }
    }
}

u8 Cartridge::read(u16 address) const {
    if (address < 0x4000) {
        // ROM Bank 0
        return m_rom[address];
    } else if (address < 0x8000) {
        // Switchable ROM Bank
        u32 romAddress = (m_currentRomBank * 0x4000) + (address - 0x4000);
        if (romAddress < m_rom.size()) {
            return m_rom[romAddress];
        }
        return 0xFF;
    } else if (address >= 0xA000 && address < 0xC000) {
        // External RAM
        if (m_ramEnabled && !m_ram.empty()) {
            u32 ramAddress = (m_currentRamBank * 0x2000) + (address - 0xA000);
            if (ramAddress < m_ram.size()) {
                return m_ram[ramAddress];
            }
        }
        return 0xFF;
    }
    
    return 0xFF;
}

void Cartridge::write(u16 address, u8 value) {
    // MBC1 banking control
    if (address < 0x2000) {
        // RAM Enable
        m_ramEnabled = (value & 0x0F) == 0x0A;
    } else if (address < 0x4000) {
        // ROM Bank Number (lower 5 bits)
        u8 bank = value & 0x1F;
        if (bank == 0) bank = 1; // Bank 0 maps to bank 1
        m_currentRomBank = (m_currentRomBank & 0xE0) | bank;
    } else if (address < 0x6000) {
        // RAM Bank Number or Upper bits of ROM Bank Number
        if (m_bankingMode == 0) {
            // ROM Banking Mode
            m_currentRomBank = (m_currentRomBank & 0x1F) | ((value & 0x03) << 5);
        } else {
            // RAM Banking Mode
            m_currentRamBank = value & 0x03;
        }
    } else if (address < 0x8000) {
        // Banking Mode Select
        m_bankingMode = value & 0x01;
    } else if (address >= 0xA000 && address < 0xC000) {
        // External RAM
        if (m_ramEnabled && !m_ram.empty()) {
            u32 ramAddress = (m_currentRamBank * 0x2000) + (address - 0xA000);
            if (ramAddress < m_ram.size()) {
                m_ram[ramAddress] = value;
            }
        }
    }
}


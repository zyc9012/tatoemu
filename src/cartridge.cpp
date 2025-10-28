#include "cartridge.h"
#include <fstream>
#include <iostream>
#include <cmath>

Cartridge::Cartridge()
    : m_cartridgeType(0)
    , m_romSize(0)
    , m_ramSize(0)
    , m_loaded(false)
    , m_mbcType(MBCType::ROM_ONLY)
    , m_isGBC(false)
    , m_isGBCOnly(false)
    , m_currentRomBank(1)
    , m_currentRamBank(0)
    , m_ramEnabled(false)
    , m_bankingMode(0)
    , m_romBankHigh(0)
    , m_rtcRegister(0)
    , m_rtcLatched(false)
    , m_romBankHighBits(0)
    , m_accelX(0)
    , m_accelY(0)
    , m_accelZ(0)
    , m_accelRegister(0)
    , m_accelEnabled(false) {
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
    
    // Save MBC-specific state
    file.write(reinterpret_cast<const char*>(&m_romBankHigh), sizeof(m_romBankHigh));
    file.write(reinterpret_cast<const char*>(&m_rtcRegister), sizeof(m_rtcRegister));
    file.write(reinterpret_cast<const char*>(&m_rtcLatched), sizeof(m_rtcLatched));
    file.write(reinterpret_cast<const char*>(&m_romBankHighBits), sizeof(m_romBankHighBits));
    file.write(reinterpret_cast<const char*>(&m_accelX), sizeof(m_accelX));
    file.write(reinterpret_cast<const char*>(&m_accelY), sizeof(m_accelY));
    file.write(reinterpret_cast<const char*>(&m_accelZ), sizeof(m_accelZ));
    file.write(reinterpret_cast<const char*>(&m_accelRegister), sizeof(m_accelRegister));
    file.write(reinterpret_cast<const char*>(&m_accelEnabled), sizeof(m_accelEnabled));
    
    // Save RTC state
    file.write(reinterpret_cast<const char*>(&m_rtc), sizeof(m_rtc));
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
    
    // Load MBC-specific state
    file.read(reinterpret_cast<char*>(&m_romBankHigh), sizeof(m_romBankHigh));
    file.read(reinterpret_cast<char*>(&m_rtcRegister), sizeof(m_rtcRegister));
    file.read(reinterpret_cast<char*>(&m_rtcLatched), sizeof(m_rtcLatched));
    file.read(reinterpret_cast<char*>(&m_romBankHighBits), sizeof(m_romBankHighBits));
    file.read(reinterpret_cast<char*>(&m_accelX), sizeof(m_accelX));
    file.read(reinterpret_cast<char*>(&m_accelY), sizeof(m_accelY));
    file.read(reinterpret_cast<char*>(&m_accelZ), sizeof(m_accelZ));
    file.read(reinterpret_cast<char*>(&m_accelRegister), sizeof(m_accelRegister));
    file.read(reinterpret_cast<char*>(&m_accelEnabled), sizeof(m_accelEnabled));
    
    // Load RTC state
    file.read(reinterpret_cast<char*>(&m_rtc), sizeof(m_rtc));
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
    std::cout << "ROM Size: " << m_rom.size() / 1024.0 << " KB (0x" << std::hex << (int)m_romSize << std::dec << ")" << std::endl;
    std::cout << "RAM Size: " << m_ram.size() / 1024.0 << " KB (0x" << std::hex << (int)m_ramSize << std::dec << ")" << std::endl;
    std::cout << "Mode: " << (m_isGBCOnly ? "GBC Only" : (m_isGBC ? "GBC Compatible" : "DMG")) << std::endl;
    
    return true;
}

void Cartridge::parseHeader() {
    // Title is at 0x0134-0x0143 (or 0x0134-0x013E for GBC games)
    m_title.clear();
    for (int i = 0x0134; i <= 0x0143 && m_rom[i] != 0; i++) {
        m_title += static_cast<char>(m_rom[i]);
    }

    // Check GBC flag at 0x0143
    u8 gbcFlag = m_rom[0x0143];
    m_isGBC = (gbcFlag == 0x80 || gbcFlag == 0xC0);
    m_isGBCOnly = (gbcFlag == 0xC0);

    m_cartridgeType = m_rom[0x0147];
    m_romSize = m_rom[0x0148];
    m_ramSize = m_rom[0x0149];

    // Determine MBC type
    m_mbcType = static_cast<MBCType>(m_cartridgeType);

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
    
    // Special case for MBC2 - 512x4 bits RAM
    if (m_mbcType == MBCType::MBC2 || m_mbcType == MBCType::MBC2_BATTERY) {
        m_ram.resize(512, 0);
    }
    
    // Special case for MBC7 - 256KB RAM
    if (m_mbcType == MBCType::MBC7_SENSOR_RUMBLE_RAM_BATTERY) {
        m_ram.resize(262144, 0); // 256KB
    }
}

u8 Cartridge::read(u16 address) const {
    switch (m_mbcType) {
        case MBCType::ROM_ONLY:
        case MBCType::ROM_RAM:
        case MBCType::ROM_RAM_BATTERY:
            if (address < 0x8000) {
                return m_rom[address];
            } else if (address >= 0xA000 && address < 0xC000 && !m_ram.empty()) {
                return m_ram[address - 0xA000];
            }
            return 0xFF;
            
        case MBCType::MBC1:
        case MBCType::MBC1_RAM:
        case MBCType::MBC1_RAM_BATTERY:
            return readMBC1(address);
            
        case MBCType::MBC2:
        case MBCType::MBC2_BATTERY:
            return readMBC2(address);
            
        case MBCType::MBC3:
        case MBCType::MBC3_RAM:
        case MBCType::MBC3_RAM_BATTERY:
        case MBCType::MBC3_TIMER_BATTERY:
        case MBCType::MBC3_TIMER_RAM_BATTERY:
            return readMBC3(address);
            
        case MBCType::MBC5:
        case MBCType::MBC5_RAM:
        case MBCType::MBC5_RAM_BATTERY:
        case MBCType::MBC5_RUMBLE:
        case MBCType::MBC5_RUMBLE_RAM:
        case MBCType::MBC5_RUMBLE_RAM_BATTERY:
            return readMBC5(address);
            
        case MBCType::MBC7_SENSOR_RUMBLE_RAM_BATTERY:
            return readMBC7(address);
            
        default:
            // Fallback to MBC1 for unknown types
            return readMBC1(address);
    }
}

void Cartridge::write(u16 address, u8 value) {
    switch (m_mbcType) {
        case MBCType::ROM_ONLY:
        case MBCType::ROM_RAM:
        case MBCType::ROM_RAM_BATTERY:
            if (address >= 0xA000 && address < 0xC000 && !m_ram.empty()) {
                m_ram[address - 0xA000] = value;
            }
            break;
            
        case MBCType::MBC1:
        case MBCType::MBC1_RAM:
        case MBCType::MBC1_RAM_BATTERY:
            writeMBC1(address, value);
            break;
            
        case MBCType::MBC2:
        case MBCType::MBC2_BATTERY:
            writeMBC2(address, value);
            break;
            
        case MBCType::MBC3:
        case MBCType::MBC3_RAM:
        case MBCType::MBC3_RAM_BATTERY:
        case MBCType::MBC3_TIMER_BATTERY:
        case MBCType::MBC3_TIMER_RAM_BATTERY:
            writeMBC3(address, value);
            break;
            
        case MBCType::MBC5:
        case MBCType::MBC5_RAM:
        case MBCType::MBC5_RAM_BATTERY:
        case MBCType::MBC5_RUMBLE:
        case MBCType::MBC5_RUMBLE_RAM:
        case MBCType::MBC5_RUMBLE_RAM_BATTERY:
            writeMBC5(address, value);
            break;
            
        case MBCType::MBC7_SENSOR_RUMBLE_RAM_BATTERY:
            writeMBC7(address, value);
            break;
            
        default:
            // Fallback to MBC1 for unknown types
            writeMBC1(address, value);
            break;
    }
}

// MBC1 Implementation
u8 Cartridge::readMBC1(u16 address) const {
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

void Cartridge::writeMBC1(u16 address, u8 value) {
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

// MBC1M Implementation (MBC1 with multi-ROM support)
u8 Cartridge::readMBC1M(u16 address) const {
    if (address < 0x4000) {
        // ROM Bank 0 - can be switched in MBC1M
        u8 bank = m_bankingMode ? m_romBankHigh : 0;
        u32 romAddress = (bank * 0x4000) + address;
        if (romAddress < m_rom.size()) {
            return m_rom[romAddress];
        }
        return 0xFF;
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

void Cartridge::writeMBC1M(u16 address, u8 value) {
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
            m_romBankHigh = value & 0x03; // Also affects ROM bank 0
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

// MBC2 Implementation (512x4 bits RAM)
u8 Cartridge::readMBC2(u16 address) const {
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
        // MBC2 RAM (512x4 bits)
        if (m_ramEnabled && !m_ram.empty()) {
            u16 ramAddress = (address - 0xA000) & 0x1FF; // 512 bytes
            u8 value = m_ram[ramAddress] & 0x0F; // Only lower 4 bits
            return value | 0xF0; // Upper 4 bits are 1
        }
        return 0xFF;
    }
    
    return 0xFF;
}

void Cartridge::writeMBC2(u16 address, u8 value) {
    if (address < 0x2000) {
        // RAM Enable (only if address bit 8 is clear)
        if ((address & 0x0100) == 0) {
            m_ramEnabled = (value & 0x0F) == 0x0A;
        }
    } else if (address < 0x4000) {
        // ROM Bank Number (only if address bit 8 is set)
        if (address & 0x0100) {
            u8 bank = value & 0x0F;
            if (bank == 0) bank = 1; // Bank 0 maps to bank 1
            m_currentRomBank = bank;
        }
    } else if (address >= 0xA000 && address < 0xC000) {
        // MBC2 RAM (512x4 bits)
        if (m_ramEnabled && !m_ram.empty()) {
            u16 ramAddress = (address - 0xA000) & 0x1FF; // 512 bytes
            m_ram[ramAddress] = value & 0x0F; // Only lower 4 bits
        }
    }
}

// MBC3 Implementation (with RTC support)
u8 Cartridge::readMBC3(u16 address) const {
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
        // External RAM or RTC
        if (m_ramEnabled) {
            if (m_currentRamBank <= 0x03 && !m_ram.empty()) {
                // RAM banks 0-3
                u32 ramAddress = (m_currentRamBank * 0x2000) + (address - 0xA000);
                if (ramAddress < m_ram.size()) {
                    return m_ram[ramAddress];
                }
            } else if (m_currentRamBank >= 0x08 && m_currentRamBank <= 0x0C) {
                // RTC registers
                if (m_rtcLatched) {
                    switch (m_currentRamBank) {
                        case 0x08: return m_rtc.seconds;
                        case 0x09: return m_rtc.minutes;
                        case 0x0A: return m_rtc.hours;
                        case 0x0B: return m_rtc.days_low;
                        case 0x0C: return m_rtc.days_high;
                    }
                } else {
                    updateRTC();
                    switch (m_currentRamBank) {
                        case 0x08: return m_rtc.seconds;
                        case 0x09: return m_rtc.minutes;
                        case 0x0A: return m_rtc.hours;
                        case 0x0B: return m_rtc.days_low;
                        case 0x0C: return m_rtc.days_high;
                    }
                }
            }
        }
        return 0xFF;
    }
    
    return 0xFF;
}

void Cartridge::writeMBC3(u16 address, u8 value) {
    if (address < 0x2000) {
        // RAM Enable
        m_ramEnabled = (value & 0x0F) == 0x0A;
    } else if (address < 0x4000) {
        // ROM Bank Number
        u8 bank = value & 0x7F;
        if (bank == 0) bank = 1; // Bank 0 maps to bank 1
        m_currentRomBank = bank;
    } else if (address < 0x6000) {
        // RAM Bank Number or RTC Register Select
        m_currentRamBank = value;
    } else if (address < 0x8000) {
        // RTC Latch
        if (value == 0x00) {
            m_rtcLatched = false;
        } else if (value == 0x01) {
            latchRTC();
        }
    } else if (address >= 0xA000 && address < 0xC000) {
        // External RAM or RTC
        if (m_ramEnabled) {
            if (m_currentRamBank <= 0x03 && !m_ram.empty()) {
                // RAM banks 0-3
                u32 ramAddress = (m_currentRamBank * 0x2000) + (address - 0xA000);
                if (ramAddress < m_ram.size()) {
                    m_ram[ramAddress] = value;
                }
            } else if (m_currentRamBank >= 0x08 && m_currentRamBank <= 0x0C) {
                // RTC registers
                updateRTC();
                switch (m_currentRamBank) {
                    case 0x08: m_rtc.seconds = value; break;
                    case 0x09: m_rtc.minutes = value; break;
                    case 0x0A: m_rtc.hours = value; break;
                    case 0x0B: m_rtc.days_low = value; break;
                    case 0x0C: m_rtc.days_high = value; break;
                }
            }
        }
    }
}

// MBC30 Implementation (MBC3 variant with larger ROM/RAM)
u8 Cartridge::readMBC30(u16 address) const {
    return readMBC3(address); // Same as MBC3
}

void Cartridge::writeMBC30(u16 address, u8 value) {
    writeMBC3(address, value); // Same as MBC3
}

// MBC5 Implementation (up to 8MB ROM and 128KB RAM)
u8 Cartridge::readMBC5(u16 address) const {
    if (address < 0x4000) {
        // ROM Bank 0
        return m_rom[address];
    } else if (address < 0x8000) {
        // Switchable ROM Bank
        u16 bank = m_currentRomBank | (m_romBankHighBits << 8);
        u32 romAddress = (bank * 0x4000) + (address - 0x4000);
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

void Cartridge::writeMBC5(u16 address, u8 value) {
    if (address < 0x2000) {
        // RAM Enable
        m_ramEnabled = (value & 0x0F) == 0x0A;
    } else if (address < 0x3000) {
        // ROM Bank Number (lower 8 bits)
        m_currentRomBank = value;
    } else if (address < 0x4000) {
        // ROM Bank Number (upper bit)
        m_romBankHighBits = value & 0x01;
    } else if (address < 0x6000) {
        // RAM Bank Number
        m_currentRamBank = value & 0x0F;
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

// MBC7 Implementation (with accelerometer and 256KB RAM)
u8 Cartridge::readMBC7(u16 address) const {
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
        // External RAM or Accelerometer
        if (m_ramEnabled) {
            if (m_currentRamBank <= 0x0F && !m_ram.empty()) {
                // RAM banks 0-15
                u32 ramAddress = (m_currentRamBank * 0x2000) + (address - 0xA000);
                if (ramAddress < m_ram.size()) {
                    return m_ram[ramAddress];
                }
            } else if (m_currentRamBank == 0x10) {
                // Accelerometer register
                switch (m_accelRegister) {
                    case 0x00: return (m_accelX >> 8) & 0xFF;
                    case 0x01: return m_accelX & 0xFF;
                    case 0x02: return (m_accelY >> 8) & 0xFF;
                    case 0x03: return m_accelY & 0xFF;
                    case 0x04: return (m_accelZ >> 8) & 0xFF;
                    case 0x05: return m_accelZ & 0xFF;
                    default: return 0xFF;
                }
            }
        }
        return 0xFF;
    }
    
    return 0xFF;
}

void Cartridge::writeMBC7(u16 address, u8 value) {
    if (address < 0x2000) {
        // RAM Enable
        m_ramEnabled = (value & 0x0F) == 0x0A;
    } else if (address < 0x4000) {
        // ROM Bank Number
        u8 bank = value & 0x7F;
        if (bank == 0) bank = 1; // Bank 0 maps to bank 1
        m_currentRomBank = bank;
    } else if (address < 0x6000) {
        // RAM Bank Number or Accelerometer Register Select
        m_currentRamBank = value;
        if (value <= 0x05) {
            m_accelRegister = value;
        }
    } else if (address >= 0xA000 && address < 0xC000) {
        // External RAM or Accelerometer
        if (m_ramEnabled) {
            if (m_currentRamBank <= 0x0F && !m_ram.empty()) {
                // RAM banks 0-15
                u32 ramAddress = (m_currentRamBank * 0x2000) + (address - 0xA000);
                if (ramAddress < m_ram.size()) {
                    m_ram[ramAddress] = value;
                }
            } else if (m_currentRamBank == 0x10) {
                // Accelerometer control
                if (address == 0xA000) {
                    m_accelEnabled = (value & 0x01) != 0;
                }
            }
        }
    }
}

// RTC Helper Functions
void Cartridge::updateRTC() const {
    if (m_rtc.halt) return; // RTC is halted
    
    std::time_t now = std::time(nullptr);
    std::time_t elapsed = now - m_rtc.lastUpdate;
    
    if (elapsed > 0) {
        m_rtc.seconds += elapsed;
        m_rtc.lastUpdate = now;
        
        // Handle overflow
        if (m_rtc.seconds >= 60) {
            m_rtc.minutes += m_rtc.seconds / 60;
            m_rtc.seconds %= 60;
        }
        
        if (m_rtc.minutes >= 60) {
            m_rtc.hours += m_rtc.minutes / 60;
            m_rtc.minutes %= 60;
        }
        
        if (m_rtc.hours >= 24) {
            u32 days = m_rtc.hours / 24;
            m_rtc.hours %= 24;
            
            u32 totalDays = (m_rtc.days_high << 8) | m_rtc.days_low;
            totalDays += days;
            
            if (totalDays >= 512) {
                m_rtc.carry = 1;
                totalDays %= 512;
            }
            
            m_rtc.days_low = totalDays & 0xFF;
            m_rtc.days_high = (totalDays >> 8) & 0x01;
        }
    }
}

void Cartridge::latchRTC() const {
    updateRTC();
    m_rtcLatched = true;
}
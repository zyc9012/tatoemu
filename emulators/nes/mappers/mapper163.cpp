#include "mapper163.h"
#include "../consts.h"
#include "../ppu.h"
#include <cstring>

namespace nes {

Mapper163::Mapper163(Cartridge* cartridge)
    : Mapper(cartridge) {
}

void Mapper163::reset() {
    std::memset(m_registers, 0, sizeof(m_registers));
    m_autoSwitchCHR = false;
    
    // "Initial value of this register is 1, initial value of 'trigger' is 0."
    m_toggle = true;
    m_registers[4] = 0;
    
    m_prgBank = 0;
    m_chrBank0 = 0;
    m_chrBank1 = 0;
}

void Mapper163::updateState() {
    // PRG page: (_registers[0] & 0x0F) | ((_registers[2] & 0x0F) << 4)
    m_prgBank = (m_registers[0] & 0x0F) | ((m_registers[2] & 0x0F) << 4);
    
    // Auto-switch CHR: (_registers[0] & 0x80) == 0x80
    m_autoSwitchCHR = (m_registers[0] & 0x80) == 0x80;
}

u8 Mapper163::cpuRead(u16 address) {
    if (address >= 0x6000 && address < 0x8000) {
        // PRG RAM
        const auto& prgRam = m_cartridge->getPRGRAM();
        if (address < 0x6000 + prgRam.size()) {
            return prgRam[address - 0x6000];
        }
        return 0;
    }
    
    // Register reads ($5000-$5FFF)
    if (address >= 0x5000 && address <= 0x5FFF) {
        switch (address & 0x7700) {
            case 0x5100:
                // Copy protection stuff - based on FCEUX's implementation
                return m_registers[3] | m_registers[1] | m_registers[0] | (m_registers[2] ^ 0xFF);
                
            case 0x5500:
                if (m_toggle) {
                    return m_registers[3] | m_registers[0];
                }
                return 0;
        }
        return 4;  // Default return value
    }
    
    // PRG ROM ($8000-$FFFF)
    if (address >= 0x8000) {
        const auto& prg = m_cartridge->getPRG();
        // PRG page size is 0x8000 (32KB)
        u32 prgSize = prg.size();
        u32 banks = prgSize / 0x8000;
        if (banks == 0) banks = 1;
        
        u32 offset = (m_prgBank % banks) * 0x8000;
        u32 addr = (address - 0x8000) + offset;
        if (addr < prgSize) {
            return prg[addr];
        }
    }
    
    return 0;
}

void Mapper163::cpuWrite(u16 address, u8 value) {
    if (address >= 0x6000 && address < 0x8000) {
        // PRG RAM
        auto& prgRam = m_cartridge->getPRGRAM();
        if (address < 0x6000 + prgRam.size()) {
            prgRam[address - 0x6000] = value;
        }
        return;
    }
    
    // Register writes ($5000-$5FFF)
    if (address >= 0x5000 && address <= 0x5FFF) {
        // "(Address is masked with 0x7300, except for 5101)"
        if (address == 0x5101) {
            if (m_registers[4] != 0 && value == 0) {
                // "If the value of this register is changed from nonzero to zero,
                // 'trigger' is toggled (XORed with 1)"
                m_toggle = !m_toggle;
            }
            m_registers[4] = value;
        } else if (address == 0x5100 && value == 6) {
            // Special case: write 6 to $5100 selects PRG bank 3
            m_prgBank = 3;
        } else {
            switch (address & 0x7300) {
                case 0x5000: {
                    m_registers[0] = value;
                    // If bit 7 is clear and scanline < 128, switch CHR banks
                    PPU* ppu = m_cartridge->getPPU();
                    if (!(m_registers[0] & 0x80) && ppu && ppu->getScanline() < 128) {
                        m_chrBank0 = 0;
                        m_chrBank1 = 1;
                    }
                    updateState();
                    break;
                }
                    
                case 0x5100:
                    m_registers[1] = value;
                    if (value == 6) {
                        m_prgBank = 3;
                    }
                    break;
                    
                case 0x5200:
                    m_registers[2] = value;
                    updateState();
                    break;
                    
                case 0x5300:
                    m_registers[3] = value;
                    break;
            }
        }
    }
}

u8 Mapper163::readCHR(u16 address) {
    // Handle auto-switch CHR based on scanline (similar to NotifyVramAddressChange)
    if (m_autoSwitchCHR) {
        PPU* ppu = m_cartridge->getPPU();
        if (ppu && ppu->getCycle() > 256) {
            u16 scanline = ppu->getScanline();
            if (scanline == 239) {
                m_chrBank0 = 0;
                m_chrBank1 = 0;
            } else if (scanline == 127) {
                m_chrBank0 = 1;
                m_chrBank1 = 1;
            }
        }
    }
    
    const auto& chr = m_cartridge->getCHR();
    if (chr.empty()) {
        return 0;
    }
    
    // CHR page size is 0x1000 (4KB)
    u32 chrSize = chr.size();
    u32 banks = chrSize / 0x1000;
    if (banks == 0) banks = 1;
    
    u8 bank;
    if (address < 0x1000) {
        // First 4KB
        bank = m_chrBank0 % banks;
    } else {
        // Second 4KB
        bank = m_chrBank1 % banks;
    }
    
    u32 offset = bank * 0x1000;
    u32 addr = (address & 0x0FFF) + offset;
    
    if (addr < chrSize) {
        return chr[addr];
    }
    
    return 0;
}

void Mapper163::writeCHR(u16 address, u8 value) {
    // CHR RAM (if no CHR ROM or if CHR RAM is used)
    auto& chr = m_cartridge->getCHR();
    if (!chr.empty()) {
        // CHR page size is 0x1000 (4KB)
        u32 chrSize = chr.size();
        u32 banks = chrSize / 0x1000;
        if (banks == 0) banks = 1;
        
        u8 bank;
        if (address < 0x1000) {
            bank = m_chrBank0 % banks;
        } else {
            bank = m_chrBank1 % banks;
        }
        
        u32 offset = bank * 0x1000;
        u32 addr = (address & 0x0FFF) + offset;
        
        if (addr < chrSize) {
            chr[addr] = value;
        }
    }
}

void Mapper163::saveState(std::ofstream& file) const {
    file.write(reinterpret_cast<const char*>(m_registers), sizeof(m_registers));
    file.write(reinterpret_cast<const char*>(&m_toggle), sizeof(m_toggle));
    file.write(reinterpret_cast<const char*>(&m_autoSwitchCHR), sizeof(m_autoSwitchCHR));
    file.write(reinterpret_cast<const char*>(&m_prgBank), sizeof(m_prgBank));
    file.write(reinterpret_cast<const char*>(&m_chrBank0), sizeof(m_chrBank0));
    file.write(reinterpret_cast<const char*>(&m_chrBank1), sizeof(m_chrBank1));
}

void Mapper163::loadState(std::ifstream& file) {
    file.read(reinterpret_cast<char*>(m_registers), sizeof(m_registers));
    file.read(reinterpret_cast<char*>(&m_toggle), sizeof(m_toggle));
    file.read(reinterpret_cast<char*>(&m_autoSwitchCHR), sizeof(m_autoSwitchCHR));
    file.read(reinterpret_cast<char*>(&m_prgBank), sizeof(m_prgBank));
    file.read(reinterpret_cast<char*>(&m_chrBank0), sizeof(m_chrBank0));
    file.read(reinterpret_cast<char*>(&m_chrBank1), sizeof(m_chrBank1));
}

} // namespace nes

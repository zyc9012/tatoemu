#include "memory.h"
#include "cartridge.h"
#include "ppu.h"
#include "../cpu.h"
#include "../sound_cpu.h"
#include "../ppu_base.h"
#include "../cartridge_base.h"
#include "../controller.h"
#include <iostream>
#include <cstring>

/*
 * CPS2 Memory Map (68000)
 * =======================
 * 
 * 0x000000-0x3FFFFF: Program ROM (max 4MB, encrypted - decrypted by cartridge)
 *                    Contains the game's 68000 program code
 *                    Read-only, mapped from cartridge (decrypted)
 * 
 * 0x400000-0x40000F: CPS2 Registers (Frg registers)
 *                    Control registers for CPS2-specific features
 * 
 * 0x660000-0x663FFF: Extra RAM (16KB)
 *                    Additional working memory
 * 
 * 0x664001:          Frame toggle register (bit 1 toggles each frame)
 * 
 * 0x708000-0x717FFF: Object RAM (64KB)
 *                    Sprite/object data
 * 
 * 0x800000-0x8001FF: I/O Ports (mirrored through 0x807FFF)
 *   0x800000-0x80001F: Input ports (controllers, coins, start buttons)
 *     0x800000: Player 1 inputs (low byte)
 *     0x800001: Player 1 inputs (high byte)
 *     0x800010: Player 2 inputs (low byte)
 *     0x800011: Player 2 inputs (high byte)
 *     0x800012: System inputs (coins, start buttons)
 *     0x800018: DIP switches 1
 *     0x800019: DIP switches 2
 * 
 * 0x900000-0x92FFFF: Video RAM (VRAM) - 192KB
 *                    Contains tile data, palette data, scroll registers
 * 
 * 0xFF0000-0xFFFFFF: Work RAM - 64KB
 *                    General purpose RAM for the game program
 * 
 * 
 * CPS2 Memory Map (Z80 Sound CPU)
 * ================================
 * 
 * 0x0000-0x7FFF: Sound ROM
 *                Z80 program code for sound driver (QSound)
 * 
 * 0x8000-0x9FFF: Sound RAM (2KB, mirrored)
 *                Working memory for Z80
 * 
 * 0xC000-0xCFFF: QSound registers (placeholder for future)
 * 0xD000-0xFFFF: Additional QSound memory (placeholder for future)
 */

namespace cps2 {

Memory::Memory()
    : m_cpu(nullptr)
    , m_soundCpu(nullptr)
    , m_ppu(nullptr)
    , m_cartridge(nullptr)
    , m_controller1(nullptr)
    , m_controller2(nullptr)
    , m_z80Bank(0)
    , m_soundCommand(0)
    , m_soundFade(0)
    , m_n664001(0) {
}

void Memory::reset() {
    // Clear RAM
    m_workRam.fill(0);
    m_extraRam.fill(0);
    m_objRam.fill(0);
    m_soundRam.fill(0);
    m_frgRegs.fill(0);
    
    // Reset Z80 bank
    m_z80Bank = 0;
    
    // Reset sound communication
    m_soundCommand = 0;
    m_soundFade = 0;
    
    // Reset frame toggle
    m_n664001 = 0;
}

// ============================================================================
// 68000 Memory Access
// ============================================================================

u8 Memory::read8(u32 address) {
    // ROM (0x000000-0x3FFFFF) - decrypted by cartridge
    if (address < 0x400000) {
        if (m_cartridge) {
            return m_cartridge->readROM8(address);
        }
        return 0xFF;
    }
    
    // CPS2 Registers (0x400000-0x40000F)
    if (address >= 0x400000 && address <= 0x40000F) {
        return m_frgRegs[address & 0x0F];
    }
    
    // Extra RAM (0x660000-0x663FFF)
    if (address >= 0x660000 && address <= 0x663FFF) {
        return m_extraRam[address - 0x660000];
    }
    
    // 0x664001 register
    if (address == 0x664001) {
        return m_n664001;
    }
    
    // Object RAM (0x708000-0x717FFF)
    if (address >= 0x708000 && address <= 0x717FFF) {
        return m_objRam[address - 0x708000];
    }
    
    // I/O Ports (0x800000-0x807FFF, mirrored)
    if ((address & 0xFF8000) == 0x800000) {
        return readPort(address & 0x1FF);
    }
    
    // VRAM (0x900000-0x92FFFF)
    if (address >= 0x900000 && address <= 0x92FFFF) {
        return readVRAM8(address - 0x900000);
    }
    
    // Work RAM (0xFF0000-0xFFFFFF)
    if (address >= 0xFF0000 && address <= 0xFFFFFF) {
        return m_workRam[address & 0xFFFF];
    }
    
    // Unmapped region
    return 0x00;
}

u16 Memory::read16(u32 address) {
    // ROM (0x000000-0x3FFFFF)
    if (address < 0x400000) {
        if (m_cartridge) {
            return m_cartridge->readROM16(address);
        }
        return 0xFFFF;
    }
    
    // CPS2 Registers (0x400000-0x40000F)
    if (address >= 0x400000 && address <= 0x40000F) {
        u8 reg = address & 0x0F;
        if (reg < 15) {
            return (static_cast<u16>(m_frgRegs[reg + 1]) << 8) | m_frgRegs[reg];
        }
        return m_frgRegs[reg];
    }
    
    // Extra RAM (0x660000-0x663FFF)
    if (address >= 0x660000 && address <= 0x663FFF) {
        u32 offset = address - 0x660000;
        if (offset < (EXTRA_RAM_SIZE - 1)) {
            return (static_cast<u16>(m_extraRam[offset + 1]) << 8) | m_extraRam[offset];
        }
        return m_extraRam[offset];
    }
    
    // 0x664001 register
    if (address == 0x664001) {
        return m_n664001;
    }
    
    // Object RAM (0x708000-0x717FFF)
    if (address >= 0x708000 && address <= 0x717FFF) {
        u32 offset = address - 0x708000;
        if (offset < (OBJ_RAM_SIZE - 1)) {
            return (static_cast<u16>(m_objRam[offset + 1]) << 8) | m_objRam[offset];
        }
        return m_objRam[offset];
    }
    
    // I/O Ports (0x800000-0x807FFF, mirrored)
    if ((address & 0xFF8000) == 0x800000) {
        u8 high = readPort(address & 0x1FF);
        u8 low = readPort((address & 0x1FF) + 1);
        return (static_cast<u16>(high) << 8) | low;
    }
    
    // VRAM (0x900000-0x92FFFF)
    if (address >= 0x900000 && address <= 0x92FFFF) {
        return readVRAM16(address - 0x900000);
    }
    
    // Work RAM (0xFF0000-0xFFFFFF)
    if (address >= 0xFF0000 && address <= 0xFFFFFF) {
        u32 offset = address & 0xFFFF;
        if (offset < 0xFFFF) {
            return (static_cast<u16>(m_workRam[offset + 1]) << 8) | m_workRam[offset];
        }
        return m_workRam[offset];
    }
    
    return 0x0000;
}

u32 Memory::read32(u32 address) {
    u32 high = read16(address);
    u32 low = read16(address + 2);
    return (high << 16) | low;
}

void Memory::write8(u32 address, u8 value) {
    // ROM is read-only
    if (address < 0x400000) {
        return;
    }
    
    // CPS2 Registers (0x400000-0x40000F)
    if (address >= 0x400000 && address <= 0x40000F) {
        m_frgRegs[address & 0x0F] = value;
        return;
    }
    
    // Extra RAM (0x660000-0x663FFF)
    if (address >= 0x660000 && address <= 0x663FFF) {
        m_extraRam[address - 0x660000] = value;
        return;
    }
    
    // 0x664001 register
    if (address == 0x664001) {
        m_n664001 = value;
        return;
    }
    
    // Object RAM (0x708000-0x717FFF)
    if (address >= 0x708000 && address <= 0x717FFF) {
        m_objRam[address - 0x708000] = value;
        return;
    }
    
    // I/O Ports (0x800000-0x807FFF, mirrored)
    if ((address & 0xFF8000) == 0x800000) {
        writePort(address & 0x1FF, value);
        return;
    }
    
    // VRAM (0x900000-0x92FFFF)
    if (address >= 0x900000 && address <= 0x92FFFF) {
        writeVRAM8(address - 0x900000, value);
        return;
    }
    
    // Work RAM (0xFF0000-0xFFFFFF)
    if (address >= 0xFF0000 && address <= 0xFFFFFF) {
        m_workRam[address & 0xFFFF] = value;
        return;
    }
}

void Memory::write16(u32 address, u16 value) {
    write8(address, (value >> 8) & 0xFF);
    write8(address + 1, value & 0xFF);
}

void Memory::write32(u32 address, u32 value) {
    write16(address, (value >> 16) & 0xFFFF);
    write16(address + 2, value & 0xFFFF);
}

// ============================================================================
// VRAM Access (forwarded to PPU)
// ============================================================================

u8 Memory::readVRAM8(u32 address) {
    if (m_ppu) {
        PPU* ppu = static_cast<PPU*>(m_ppu);
        return ppu->readVRAM8(address);
    }
    return 0x00;
}

u16 Memory::readVRAM16(u32 address) {
    if (m_ppu) {
        PPU* ppu = static_cast<PPU*>(m_ppu);
        return ppu->readVRAM16(address);
    }
    return 0x0000;
}

u32 Memory::readVRAM32(u32 address) {
    if (m_ppu) {
        PPU* ppu = static_cast<PPU*>(m_ppu);
        return ppu->readVRAM32(address);
    }
    return 0x00000000;
}

void Memory::writeVRAM8(u32 address, u8 value) {
    if (m_ppu) {
        PPU* ppu = static_cast<PPU*>(m_ppu);
        ppu->writeVRAM8(address, value);
    }
}

void Memory::writeVRAM16(u32 address, u16 value) {
    if (m_ppu) {
        PPU* ppu = static_cast<PPU*>(m_ppu);
        ppu->writeVRAM16(address, value);
    }
}

void Memory::writeVRAM32(u32 address, u32 value) {
    if (m_ppu) {
        PPU* ppu = static_cast<PPU*>(m_ppu);
        ppu->writeVRAM32(address, value);
    }
}

// ============================================================================
// I/O Port Access
// ============================================================================

u8 Memory::readPort(u16 port) {
    u8 value = 0xFF;
    
    // Input ports (0x000-0x01F) - same as CPS1
    switch (port) {
        case 0x000:
            // Player 2 inputs (active low)
            if (m_controller2) {
                value = ~m_controller2->read();
            }
            return value;
            
        case 0x001:
            // Player 1 inputs (active low)
            if (m_controller1) {
                value = ~m_controller1->read();
            }
            return value;
            
        case 0x010:
            // Player 2 inputs (active low) - alternate port
            if (m_controller2) {
                value = ~m_controller2->read();
            }
            return value;
            
        case 0x011:
            // Player 1 inputs (active low) - alternate port
            if (m_controller1) {
                value = ~m_controller1->read();
            }
            return value;
            
        case 0x012:
            // Kick buttons
            value = 0xFF;
            if (m_controller1) {
                value &= ~(m_controller1->readKicks() & 0x07);
            }
            if (m_controller2) {
                value &= ~((m_controller2->readKicks() & 0x07) << 4);
            }
            return value;
            
        case 0x018:
            // Coin/Start inputs (active low)
            value = 0xFF;
            if (m_controller1) {
                u8 p1 = m_controller1->readCoinStart();
                if (p1 & 0x01) value &= ~0x01;  // P1 coin
                if (p1 & 0x10) value &= ~0x10;  // P1 start
            }
            if (m_controller2) {
                u8 p2 = m_controller2->readCoinStart();
                if (p2 & 0x01) value &= ~0x02;  // P2 coin
                if (p2 & 0x10) value &= ~0x20;  // P2 start
            }
            return value;
            
        case 0x01A:
            // DIP Switch A: Coinage settings
            return ~0x00; // 1 Coin 1 Credit
            
        case 0x01C:
            // DIP Switch B: Difficulty settings
            return ~0x03; // Difficulty 4/Normal
                        
        case 0x01E:
            // DIP Switch C: System settings
            return ~0x60; // Demo Sound ON, Continue ON
    }
    
    // CPS Registers (0x100-0x1FF) - forward to PPU
    if (port >= 0x100 && port < 0x200) {
        PPU* ppu = static_cast<PPU*>(m_ppu);
        return ppu->readRegister8(port - 0x100);
    }
    
    // Unmapped port
    return 0xFF;
}

void Memory::writePort(u16 port, u8 value) {
    // Sound command (0x181) - placeholder for future QSound
    if (port == 0x181) {
        m_soundCommand = value;
        return;
    }
    
    // Sound fade (0x189) - placeholder for future QSound
    if (port == 0x189) {
        m_soundFade = value;
        return;
    }
    
    // CPS Registers (0x100-0x1FF)
    if (port >= 0x100 && port < 0x200) {
        u8 regNum = port - 0x100;
        PPU* ppu = static_cast<PPU*>(m_ppu);
        ppu->writeRegister8(regNum, value);
        return;
    }
}

// ============================================================================
// Z80 Memory Access (placeholder - QSound not implemented yet)
// ============================================================================

u8 Memory::readZ80(u16 address) {
    // Sound ROM (0x0000-0x7FFF)
    if (address < 0x8000) {
        if (m_cartridge) {
            auto* cart = static_cast<Cartridge*>(m_cartridge);
            return cart->readSoundROM8(address);
        }
        return 0xFF;
    }
    
    // Bank-switchable ROM (0x8000-0xBFFF)
    if (address >= 0x8000 && address < 0xC000) {
        if (m_cartridge) {
            auto* cart = static_cast<Cartridge*>(m_cartridge);
            u32 bankOffset = (static_cast<u32>(m_z80Bank) << 14) + 0x8000;
            u32 romAddress = bankOffset + (address - 0x8000);
            return cart->readSoundROM8(romAddress);
        }
        return 0xFF;
    }
    
    // Z80 RAM (0xC000-0xCFFF) - placeholder
    if (address >= 0xC000 && address < 0xD000) {
        return m_soundRam[address - 0xC000];
    }
    
    // QSound registers (0xF000-0xFFFF) - placeholder
    if (address >= 0xF000) {
        switch (address) {
            case 0xF008:
                return m_soundCommand;
            case 0xF00A:
                return m_soundFade;
            default:
                return 0xFF;
        }
    }
    
    return 0xFF;
}

void Memory::writeZ80(u16 address, u8 value) {
    // ROM is read-only
    if (address < 0x8000) {
        return;
    }
    
    // Bank-switchable ROM area is read-only
    if (address >= 0x8000 && address < 0xC000) {
        return;
    }
    
    // Z80 RAM (0xC000-0xCFFF)
    if (address >= 0xC000 && address < 0xD000) {
        m_soundRam[address - 0xC000] = value;
        return;
    }
    
    // QSound registers (0xF000-0xFFFF) - placeholder
    if (address >= 0xF000) {
        switch (address) {
            case 0xF004: {
                // ROM bank switching (0-15)
                u8 newBank = value & 0x0F;
                if (m_z80Bank != newBank) {
                    m_z80Bank = newBank;
                }
                return;
            }
            default:
                return;
        }
    }
}

// ============================================================================
// Save/Load State
// ============================================================================

void Memory::saveState(std::ofstream& file) {
    // Save RAM
    file.write(reinterpret_cast<const char*>(m_workRam.data()), m_workRam.size());
    file.write(reinterpret_cast<const char*>(m_extraRam.data()), m_extraRam.size());
    file.write(reinterpret_cast<const char*>(m_objRam.data()), m_objRam.size());
    file.write(reinterpret_cast<const char*>(m_soundRam.data()), m_soundRam.size());
    file.write(reinterpret_cast<const char*>(m_frgRegs.data()), m_frgRegs.size());
    
    // Save Z80 state
    file.write(reinterpret_cast<const char*>(&m_z80Bank), sizeof(m_z80Bank));
    file.write(reinterpret_cast<const char*>(&m_soundCommand), sizeof(m_soundCommand));
    file.write(reinterpret_cast<const char*>(&m_soundFade), sizeof(m_soundFade));
    file.write(reinterpret_cast<const char*>(&m_n664001), sizeof(m_n664001));
}

void Memory::loadState(std::ifstream& file) {
    // Load RAM
    file.read(reinterpret_cast<char*>(m_workRam.data()), m_workRam.size());
    file.read(reinterpret_cast<char*>(m_extraRam.data()), m_extraRam.size());
    file.read(reinterpret_cast<char*>(m_objRam.data()), m_objRam.size());
    file.read(reinterpret_cast<char*>(m_soundRam.data()), m_soundRam.size());
    file.read(reinterpret_cast<char*>(m_frgRegs.data()), m_frgRegs.size());
    
    // Load Z80 state
    file.read(reinterpret_cast<char*>(&m_z80Bank), sizeof(m_z80Bank));
    file.read(reinterpret_cast<char*>(&m_soundCommand), sizeof(m_soundCommand));
    file.read(reinterpret_cast<char*>(&m_soundFade), sizeof(m_soundFade));
    file.read(reinterpret_cast<char*>(&m_n664001), sizeof(m_n664001));
}

} // namespace cps2

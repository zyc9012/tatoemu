#include "memory.h"
#include "cartridge.h"
#include "cpu.h"
#include "sound_cpu.h"
#include "ppu.h"
#include "apu.h"
#include "controller.h"
#include "db.h"
#include <iostream>
#include <cstring>

/*
 * Unified Memory Map (CPS1 and CPS2)
 * ===================================
 * 
 * Common:
 * 0x000000-0x3FFFFF: Program ROM (max 4MB, encrypted for CPS2)
 * 0x800000-0x8001FF: I/O Ports and CPS Registers
 * 0x900000-0x92FFFF: Video RAM (VRAM) - 192KB
 * 0xFF0000-0xFFFFFF: Work RAM - 64KB
 * 
 * CPS2-only:
 * 0x400000-0x40000F: CPS2 Registers (Frg registers)
 * 0x660000-0x663FFF: Extra RAM (16KB)
 * 0x664001: Frame toggle register
 * 0x708000-0x717FFF: Object RAM (64KB)
 */

 static const EEPROMInterface cps2EEPROMInterface =
 {
    6,      /* address bits */
    16,     /* data bits */
    "0110", /* read command */
    "0101", /* write command */
    "0111", /* erase command */
    0,
    0,
    0,
    0
};

namespace cps {

Memory::Memory()
    : m_cpu(nullptr)
    , m_soundCpu(nullptr)
    , m_ppu(nullptr)
    , m_apu(nullptr)
    , m_cartridge(nullptr)
    , m_controller(nullptr)
    , m_z80Bank(0)
    , m_protCalc{0, 0}
    , m_memProt{0x00, 0x00, 0x00, 0x00}
    , m_boardId{0x00, 0x00, 0x00}
    , m_soundCommand(0)
    , m_soundFade(0)
    , m_qscCmd{0, 0}
    , m_n664001(0)
    , m_rasterIRQ50(0)
    , m_rasterIRQ52(0) {
}

u8 Memory::getCPSVersion() const {
    if (m_cartridge) {
        return m_cartridge->getCPSVersion();
    }
    return 1;  // Default to CPS1
}

void Memory::reset() {
    // Clear RAM
    m_workRam.fill(0);
    m_soundRam.fill(0);
    
    // Reset Z80 bank
    m_z80Bank = 0;
    
    u8 cpsVer = getCPSVersion();
    
    // Initialize EEPROM
    if (cpsVer == 2) {
        m_eeprom.init(&cps2EEPROMInterface);
        m_eeprom.reset();
    }
    
    // Reset protection calc
    m_protCalc[0] = 0;
    m_protCalc[1] = 0;
    
    // Set board ID and memProt from game database
    if (m_cartridge) {
        BoardConfig config = m_cartridge->getBoardConfig();
        
        m_boardId[0] = config.boardIdOffset;
        m_boardId[1] = config.boardIdValue1;
        m_boardId[2] = config.boardIdValue2;
        
        // Set memory protection offsets
        m_memProt[0] = config.memProt[0];
        m_memProt[1] = config.memProt[1];
        m_memProt[2] = config.memProt[2];
        m_memProt[3] = config.memProt[3];
    }

    // CPS2-specific reset
    m_extraRam.fill(0);
    m_objRam.fill(0);
    m_frgRegs.fill(0);
    m_n664001 = 0;
    m_rasterIRQ50 = 0;
    m_rasterIRQ52 = 0;
    m_qscCmd[0] = 0;
    m_qscCmd[1] = 0;
    
    // Reset sound communication
    m_soundCommand = 0;
    m_soundFade = 0;
}

// ============================================================================
// 68000 Memory Access
// ============================================================================

u8 Memory::read8(u32 address) {
    u8 cpsVer = getCPSVersion();
    
    // ROM (0x000000-0x3FFFFF) - use decrypted ROM for instruction fetches
    if (address < 0x400000) {
        if (m_cartridge) {
            return m_cartridge->readROM8(address);
        }
        return 0xFF;
    }
    
    // CPS2-specific: CPS2 Registers (0x400000-0x40000F)
    if (cpsVer == 2 && address >= 0x400000 && address <= 0x40000F) {
        return m_frgRegs[address & 0x0F];
    }
    
    // CPS2-specific: Extra RAM (0x660000-0x663FFF)
    if (cpsVer == 2 && address >= 0x660000 && address <= 0x663FFF) {
        return m_extraRam[address - 0x660000];
    }
    
    // CPS2-specific: 0x664001 register
    if (cpsVer == 2 && address == 0x664001) {
        return m_n664001;
    }
    
    // CPS2-specific: QSound shared RAM (0x618000-0x619FFF)
    // Only odd addresses are valid (even addresses return 0xFF)
    if (cpsVer == 2 && address >= 0x618000 && address <= 0x619FFF) {
        // Even addresses return 0xFF
        if ((address & 1) == 0) {
            return 0xFF;
        }
        
        // Mask address to 0x1FFF and divide by 2 to get index
        u32 index = (address & 0x1FFF) >> 1;
        return m_soundRam[index];
    }
    
    // CPS2-specific: Object RAM (0x708000-0x717FFF)
    if (cpsVer == 2 && address >= 0x708000 && address <= 0x717FFF) {
        return m_objRam[address - 0x708000];
    }
    
    // I/O Ports and CPS Registers (0x800000-0x807FFF, mirrored)
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
    // I/O Ports and CPS Registers (0x800000-0x807FFF, mirrored)
    if ((address & 0xFF8000) == 0x800000) {
        if ((address & 0xFF8FFF) == (0x800100 + m_memProt[3])) {
            // Return multiplication result (high word)
            u32 result = static_cast<u32>(m_protCalc[0]) * static_cast<u32>(m_protCalc[1]);
            return static_cast<u16>((result >> 16) & 0xFFFF);
        }
        if ((address & 0xFF8FFF) == (0x800100 + m_memProt[2])) {
            // Return multiplication result (low word)
            u32 result = static_cast<u32>(m_protCalc[0]) * static_cast<u32>(m_protCalc[1]);
            return static_cast<u16>(result & 0xFFFF);
        }
        
        u8 high = readPort(address & 0x1FF);
        u8 low = readPort((address & 0x1FF) + 1);
        return (static_cast<u16>(high) << 8) | low;
    }

    u32 high = read8(address);
    u32 low = read8(address + 1);
    return (high << 8) | low;
}

u32 Memory::read32(u32 address) {
    u32 high = read16(address);
    u32 low = read16(address + 2);
    return (high << 16) | low;
}

// Data reads - use encrypted ROM for CPS2 (for exception vectors)
u8 Memory::read8Data(u32 address) {
    // ROM (0x000000-0x3FFFFF) - use encrypted ROM for data reads in CPS2
    if (address < 0x400000) {
        if (m_cartridge) {
            return m_cartridge->readEncryptedROM8(address);
        }
        return 0xFF;
    }
    
    // For non-ROM addresses, use regular read
    return read8(address);
}

u16 Memory::read16Data(u32 address) {
    // ROM (0x000000-0x3FFFFF) - use encrypted ROM for data reads in CPS2
    if (address < 0x400000) {
        if (m_cartridge) {
            return m_cartridge->readEncryptedROM16(address);
        }
        return 0xFFFF;
    }
    
    // For non-ROM addresses, use regular read
    return read16(address);
}

u32 Memory::read32Data(u32 address) {
    u32 high = read16Data(address);
    u32 low = read16Data(address + 2);
    return (high << 16) | low;
}

void Memory::write8(u32 address, u8 value) {
    u8 cpsVer = getCPSVersion();
    
    // ROM is read-only
    if (address < 0x400000) {
        return;
    }
    
    // CPS2-specific: CPS2 Registers (0x400000-0x40000F)
    if (cpsVer == 2 && address >= 0x400000 && address <= 0x40000F) {
        m_frgRegs[address & 0x0F] = value;
        return;
    }
    
    // CPS2-specific: Extra RAM (0x660000-0x663FFF)
    if (cpsVer == 2 && address >= 0x660000 && address <= 0x663FFF) {
        m_extraRam[address - 0x660000] = value;
        return;
    }
    
    // CPS2-specific: 0x664001 register (frame toggle/counter)
    if (cpsVer == 2 && address == 0x664001) {
        m_n664001 = value;
        return;
    }
    
    // CPS2-specific: QSound shared RAM (0x618000-0x619FFF)
    // Only odd addresses are valid (even addresses are ignored)
    if (cpsVer == 2 && address >= 0x618000 && address <= 0x619FFF) {
        // Even addresses are ignored
        if ((address & 1) == 0) {
            return;
        }
        
        // Mask address to 0x1FFF and divide by 2 to get index
        u32 index = (address & 0x1FFF) >> 1;
        m_soundRam[index] = value;
        return;
    }
    
    // CPS2-specific: Object RAM (0x708000-0x717FFF)
    if (cpsVer == 2 && address >= 0x708000 && address <= 0x717FFF) {
        m_objRam[address - 0x708000] = value;
        return;
    }
    
    // I/O Ports and CPS Registers (0x800000-0x807FFF, mirrored)
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
    // I/O Ports and CPS Registers (0x800000-0x807FFF, mirrored)
    if ((address & 0xFF8000) == 0x800000) {
        // Protection multiplication input
        if ((address & 0xFF8FFF) == (0x800100 + m_memProt[0])) {
            m_protCalc[0] = value;
        }
        if ((address & 0xFF8FFF) == (0x800100 + m_memProt[1])) {
            m_protCalc[1] = value;
        }
        
        writePort(address & 0x1FF, (value >> 8) & 0xFF);
        writePort((address & 0x1FF) + 1, value & 0xFF);
        return;
    }

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
        return m_ppu->readVRAM8(address);
    }
    return 0x00;
}

u16 Memory::readVRAM16(u32 address) {
    if (m_ppu) {
        return m_ppu->readVRAM16(address);
    }
    return 0x0000;
}

u32 Memory::readVRAM32(u32 address) {
    if (m_ppu) {
        return m_ppu->readVRAM32(address);
    }
    return 0x00000000;
}

void Memory::writeVRAM8(u32 address, u8 value) {
    if (m_ppu) {
        m_ppu->writeVRAM8(address, value);
    }
}

void Memory::writeVRAM16(u32 address, u16 value) {
    if (m_ppu) {
        m_ppu->writeVRAM16(address, value);
    }
}

void Memory::writeVRAM32(u32 address, u32 value) {
    if (m_ppu) {
        m_ppu->writeVRAM32(address, value);
    }
}

// ============================================================================
// I/O Port Access
// ============================================================================

u8 Memory::readPort(u16 port) {
    u8 cpsVer = getCPSVersion();
    u8 value = 0xFF;
    
    // CPS2-specific ports
    if (cpsVer == 2) {
        if (port == 0x020) {
            if (m_controller) {
                return m_controller->readPort(port);
            }
            return 0xFF;
        }
        
        // Port 0x021: EEPROM read (bit 0), Diagnostic (bit 1), Service (bit 2)
        if (port == 0x021) {
            value = 0xFE;  // Bit 0 cleared (will be set by EEPROM)
            value |= m_eeprom.read();
            return value;
        }
        
        // Port 0x030: Volume control high byte (CPS2 only)
        if (port == 0x030) {
            // TODO: Implement volume control
            // For now, return default value
            return 0xE0;
        }
        
        // Port 0x031: Volume control low byte (CPS2 only)
        if (port == 0x031) {
            // TODO: Implement volume control
            // For now, return default value
            return 0x21;
        }
        
        // Ports 0x050-0x051: Raster line counter for IRQ line 50 (CPS2 only)
        if ((port & 0x0FE) == 0x050) {
            if ((port & 1) == 0) {
                return (m_rasterIRQ50 >> 8) & 0xFF;  // High byte
            } else {
                return m_rasterIRQ50 & 0xFF;          // Low byte
            }
        }
        
        // Ports 0x052-0x053: Raster line counter for IRQ line 52 (CPS2 only)
        if ((port & 0x0FE) == 0x052) {
            if ((port & 1) == 0) {
                return (m_rasterIRQ52 >> 8) & 0xFF;  // High byte
            } else {
                return m_rasterIRQ52 & 0xFF;          // Low byte
            }
        }
    }
    
    // Input ports
    // The controller handles all button-to-port mappings based on CPS version
    switch (port) {
        case 0x000:
        case 0x001:
        case 0x010:
        case 0x011:
        case 0x012:
        case 0x018:
        case 0x177:
            if (m_controller) {
                return m_controller->readPort(port);
            }
            return 0xFF;

        // DIP Switch ports (not controller inputs)
        case 0x01A:
            // DIP Switch A: Coinage settings
            // Bit 0-2 (mask 0x07): Coin A
            //   000 = 1 Coin 1 Credit
            //   001 = 1 Coin 2 Credits
            //   010 = 1 Coin 3 Credits
            //   011 = 1 Coin 4 Credits
            //   100 = 1 Coin 6 Credits
            //   101 = 2 Coins 1 Credit
            //   110 = 3 Coins 1 Credit
            //   111 = 4 Coins 1 Credit
            // Bit 3-5 (mask 0x38): Coin B (same encoding as Coin A, shifted by 3 bits)
            // Bit 6 (mask 0x40): 2C to Start, 1 to Cont (0 = Off, 1 = On)
            // Bit 7 (mask 0x80): Unused or game-specific
            return ~0x00; // (all bits on = 1 Coin 1 Credit for both)

        case 0x01C:
            // DIP Switch B: Difficulty settings
            // Bit 0-2 (mask 0x07): Difficulty
            //   000 = 1 (Easiest)
            //   001 = 2
            //   010 = 3
            //   011 = 4 (Normal)
            //   100 = 5
            //   101 = 6
            //   110 = 7
            //   111 = 8 (Hardest)
            // Bit 3-7: Unused or game-specific settings
            return ~0x03; // (bits 0-2 = 011 = Difficulty 4/Normal)
                        
        case 0x01E:
            // DIP Switch C: System settings
            // Bit 0-1: Unused
            // Bit 2 (mask 0x04): Free Play (0 = Off, 1 = On)
            // Bit 3 (mask 0x08): Freeze (0 = Off, 1 = On)
            // Bit 4 (mask 0x10): Flip Screen (0 = Off, 1 = On)
            // Bit 5 (mask 0x20): Demo Sound (0 = Off, 1 = On)
            // Bit 6 (mask 0x40): Allow Continue (0 = Off, 1 = On)
            // Bit 7 (mask 0x80): Game Mode (0 = Game, 1 = Test)
            return ~0x60; // (bits 5-6 set = Demo Sound ON, Continue ON)

    }
    
    // CPS Registers (0x100-0x1FF)
    if (port >= 0x100 && port < 0x200) {
        // CPS1-specific: Board ID
        if (cpsVer == 1) {
            // Check if this is the board ID location
            if (port == (0x100 + m_boardId[0])) {
                return m_boardId[1];
            }
            if (port == (0x100 + m_boardId[0] + 1)) {
                return m_boardId[2];
            }
        }
        
        // CPS Registers - forward to PPU
        if (m_ppu) {
            return m_ppu->readRegister8(port & 0xFF);
        }
    }
    
    // Unmapped port - return 0xFF (bus pull-up)
    return 0xFF;
}

void Memory::writePort(u16 port, u8 value) {
    u8 cpsVer = getCPSVersion();
    
    // CPS2-specific ports
    if (cpsVer == 2) {
        // Port 0x040: EEPROM write control
        if (port == 0x040) {
            m_eeprom.write(value & 0x20, value & 0x40, value & 0x10);
            return;
        }
        
        // Port 0x0E1: Object bank select (CPS2 only)
        // Bit 0: Object bank (0 or 1)
        // This selects which sprite buffer (0x708000 or 0x710000) to use for sprite rendering
        // The object bank is also stored in FRG register 0x0E for PPU access
        if ((port & 0x1FF) == 0x0E1) {
            m_frgRegs[0x0E] = value & 1;
            // Sync PPU's object bank
            if (m_ppu) {
                m_ppu->setObjectBank(value & 1);
            }
            return;
        }
        
        // Ports 0x050-0x051: Raster line register for IRQ line 50 (CPS2 only)
        if ((port & 0x0FE) == 0x050) {
            if ((port & 1) == 0) {
                // High byte
                m_rasterIRQ50 = (m_rasterIRQ50 & 0x00FF) | (static_cast<u16>(value) << 8);
            } else {
                // Low byte
                m_rasterIRQ50 = (m_rasterIRQ50 & 0xFF00) | value;
            }
            return;
        }
        
        // Ports 0x052-0x053: Raster line register for IRQ line 52 (CPS2 only)
        if ((port & 0x0FE) == 0x052) {
            if ((port & 1) == 0) {
                // High byte
                m_rasterIRQ52 = (m_rasterIRQ52 & 0x00FF) | (static_cast<u16>(value) << 8);
            } else {
                // Low byte
                m_rasterIRQ52 = (m_rasterIRQ52 & 0xFF00) | value;
            }
            return;
        }
    }
    
    // Sound command (0x181)
    // This is how the 68000 sends commands to the Z80 sound CPU
    // The Z80 reads this from 0xF008
    if (port == 0x181) {
        // Store sound command (Z80 reads from 0xF008)
        m_soundCommand = value;
        return;
    }
    
    // Sound fade (0x189)
    // Used by some games to fade music in/out
    // The Z80 reads this from 0xF00A
    if (port == 0x189) {
        // Store fade value (Z80 reads from 0xF00A)
        m_soundFade = value;
        return;
    }
    
    // CPS Registers (0x100-0x1FF)
    // These control video layer scrolling, priorities, palette selection, etc.
    if (port >= 0x100 && port < 0x200) {
        u8 regNum = port & 0xFF;
        
        // Forward to PPU for layer control and scroll registers
        if (m_ppu) {
            m_ppu->writeRegister8(regNum, value);
        }
        
        return;
    }
}

// ============================================================================
// Z80 Memory Access
// ============================================================================

u8 Memory::readZ80(u16 address) {
    u8 cpsVer = getCPSVersion();
    
    // Sound ROM (0x0000-0x7FFF) - common
    if (address < 0x8000) {
        if (m_cartridge) {
            return m_cartridge->readSoundROM8(address);
        }
        return 0xFF;
    }
    
    // Bank-switchable ROM (0x8000-0xBFFF) - common
    if (address >= 0x8000 && address < 0xC000) {
        if (m_cartridge) {
            u32 bankOffset = (static_cast<u32>(m_z80Bank) << 14) + 0x8000;
            u32 romAddress = bankOffset + (address - 0x8000);
            return m_cartridge->readSoundROM8(romAddress);
        }
        return 0xFF;
    }
    
    if (cpsVer == 1) {
        // CPS1-specific: ROM fallback for fetches (0xC000-0xCFFF)
        if (address >= 0xC000 && address < 0xD000) {
            if (m_cartridge) {
                return m_cartridge->readSoundROM8(address - 0xC000);
            }
            return 0xFF;
        }
        
        // CPS1: Z80 RAM (0xD000-0xD7FF) - 2KB
        if (address >= 0xD000 && address < 0xD800) {
            return m_soundRam[address - 0xD000];
        }
        
        // CPS1: ROM fallback for fetches (0xD800-0xEFFF)
        if (address >= 0xD800 && address < 0xF000) {
            if (m_cartridge) {
                return m_cartridge->readSoundROM8((address - 0xD800) & 0x7FFF);
            }
            return 0xFF;
        }
        
        // CPS1: I/O Area (0xF000-0xFFFF) - YM2151, MSM6295
        if (address >= 0xF000) {
            switch (address) {
                case 0xF001:
                    // YM2151 status register
                    if (m_apu) {
                        return m_apu->readPort(0x01);
                    }
                    return 0xFF;
                    
                case 0xF002:
                    // MSM6295 status
                    if (m_apu) {
                        return m_apu->readPort(0x02);
                    }
                    return 0xFF;
                    
                case 0xF008:
                    // Sound command latch (from 68000)
                    return m_soundCommand;
                    
                case 0xF00A:
                    // Sound fade value (from 68000)
                    return m_soundFade;
                    
                default:
                    return 0xFF;
            }
        }
    } else {
        // CPS2-specific: Z80 RAM (0xC000-0xCFFF)
        if (address >= 0xC000 && address < 0xD000) {
            return m_soundRam[address - 0xC000];
        }
        
        // CPS2: QSound registers (0xD000-0xEFFF)
        // Note: For opcode fetches, this area maps to ROM (handled separately)
        if (address >= 0xD000 && address < 0xF000) {
            // QSound status register (0xD007)
            if (address == 0xD007) {
                printf("readZ80: (QSound status) address=%04X\n", address);
                // TODO: Call QscRead() when QSound chip is implemented
                // For now, return 0 (not ready) or implement basic status
                return 0;
            }
            // Other addresses in this range: for data reads, return 0xFF
            // For opcode fetches, this should map to ROM (handled by fetch logic)
            return 0xFF;
        }
        
        // CPS2: Z80 RAM (0xF000-0xFFFF)
        if (address >= 0xF000 && address <= 0xFFFF) {
            if (cpsVer == 1) {
                // Special registers in RAM area
                if (address == 0xF008) {
                    return m_soundCommand;
                }
                if (address == 0xF00A) {
                    return m_soundFade;
                }
            } else {
                // Regular RAM access (offset 0x1000-0x1FFF in m_soundRam)
                return m_soundRam[0x1000 + (address - 0xF000)];
            }
        }
    }
    
    return 0xFF;
}

void Memory::writeZ80(u16 address, u8 value) {
    u8 cpsVer = getCPSVersion();
    
    // Sound ROM is read-only
    if (address < 0x8000) {
        return;
    }
    
    // Bank-switchable ROM area is read-only
    if (address >= 0x8000 && address < 0xC000) {
        return;
    }
    
    if (cpsVer == 1) {
        // CPS1-specific: ROM fallback area is read-only
        if (address >= 0xC000 && address < 0xD000) {
            return;
        }
        
        // CPS1: Z80 RAM (0xD000-0xD7FF) - 2KB
        if (address >= 0xD000 && address < 0xD800) {
            m_soundRam[address - 0xD000] = value;
            return;
        }
        
        // CPS1: ROM fallback area is read-only
        if (address >= 0xD800 && address < 0xF000) {
            return;
        }
        
        // CPS1: I/O Area (0xF000-0xFFFF) - YM2151, MSM6295
        if (address >= 0xF000) {
            switch (address) {
                case 0xF000:
                    // YM2151 register select
                    if (m_apu) {
                        m_apu->writePort(0x00, value);
                    }
                    return;
                    
                case 0xF001:
                    // YM2151 data write
                    if (m_apu) {
                        m_apu->writePort(0x01, value);
                    }
                    return;
                    
                case 0xF002:
                    // MSM6295 command
                    if (m_apu) {
                        m_apu->writePort(0x02, value);
                    }
                    return;
                    
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
    } else {
        // CPS2-specific: Z80 RAM (0xC000-0xCFFF)
        if (address >= 0xC000 && address < 0xD000) {
            m_soundRam[address - 0xC000] = value;
            return;
        }
        
        // CPS2: QSound registers (0xD000-0xEFFF)
        if (address >= 0xD000 && address < 0xF000) {
            if (address == 0xD000) {
                // QSound command byte 0
                m_qscCmd[0] = value;
                return;
            }
            if (address == 0xD001) {
                // QSound command byte 1
                m_qscCmd[1] = value;
                return;
            }
            if (address == 0xD002) {
                // QSound command write
                // Command is: (m_qscCmd[0] << 8) | m_qscCmd[1], with value as the third byte
                // TODO: Call QscWrite(value, (m_qscCmd[0] << 8) | m_qscCmd[1]) when QSound chip is implemented
                // For now, just store the command
                return;
            }
            if (address == 0xD003) {
                // ROM bank switching (0-15)
                u8 newBank = value & 0x0F;
                if (m_z80Bank != newBank) {
                    m_z80Bank = newBank;
                }
                return;
            }
            // Other addresses in this range are write-only registers or unused
            return;
        }
        
        // CPS2: Z80 RAM (0xF000-0xFFFF)
        if (address >= 0xF000 && address <= 0xFFFF) {
            // Regular RAM access (offset 0x1000-0x1FFF in m_soundRam)
            m_soundRam[0x1000 + (address - 0xF000)] = value;
            return;
        }
    }
}

// ============================================================================
// Save/Load State
// ============================================================================

void Memory::saveState(std::ofstream& file) {
    u8 cpsVer = getCPSVersion();
    
    // Save RAM (common)
    file.write(reinterpret_cast<const char*>(m_workRam.data()), m_workRam.size());
    file.write(reinterpret_cast<const char*>(m_soundRam.data()), m_soundRam.size());
    
    // Save CPS1-specific state
    if (cpsVer == 1) {
        file.write(reinterpret_cast<const char*>(&m_protCalc), sizeof(m_protCalc));
        file.write(reinterpret_cast<const char*>(&m_memProt), sizeof(m_memProt));
        file.write(reinterpret_cast<const char*>(&m_boardId), sizeof(m_boardId));
    }
    
    // Save CPS2-specific state
    if (cpsVer == 2) {
        file.write(reinterpret_cast<const char*>(m_extraRam.data()), m_extraRam.size());
        file.write(reinterpret_cast<const char*>(m_objRam.data()), m_objRam.size());
        file.write(reinterpret_cast<const char*>(m_frgRegs.data()), m_frgRegs.size());
        file.write(reinterpret_cast<const char*>(&m_n664001), sizeof(m_n664001));
        file.write(reinterpret_cast<const char*>(m_qscCmd), sizeof(m_qscCmd));
    }
    
    // Save Z80 state (common)
    file.write(reinterpret_cast<const char*>(&m_z80Bank), sizeof(m_z80Bank));
    file.write(reinterpret_cast<const char*>(&m_soundCommand), sizeof(m_soundCommand));
    file.write(reinterpret_cast<const char*>(&m_soundFade), sizeof(m_soundFade));

    // Save EEPROM state
    m_eeprom.saveState(file);
}

void Memory::loadState(std::ifstream& file) {
    u8 cpsVer = getCPSVersion();
    
    // Load RAM (common)
    file.read(reinterpret_cast<char*>(m_workRam.data()), m_workRam.size());
    file.read(reinterpret_cast<char*>(m_soundRam.data()), m_soundRam.size());
    
    // Load CPS1-specific state
    if (cpsVer == 1) {
        file.read(reinterpret_cast<char*>(&m_protCalc), sizeof(m_protCalc));
        file.read(reinterpret_cast<char*>(&m_memProt), sizeof(m_memProt));
        file.read(reinterpret_cast<char*>(&m_boardId), sizeof(m_boardId));
    }
    
    // Load CPS2-specific state
    if (cpsVer == 2) {
        file.read(reinterpret_cast<char*>(m_extraRam.data()), m_extraRam.size());
        file.read(reinterpret_cast<char*>(m_objRam.data()), m_objRam.size());
        file.read(reinterpret_cast<char*>(m_frgRegs.data()), m_frgRegs.size());
        file.read(reinterpret_cast<char*>(&m_n664001), sizeof(m_n664001));
        file.read(reinterpret_cast<char*>(m_qscCmd), sizeof(m_qscCmd));
    }
    
    // Load Z80 state (common)
    file.read(reinterpret_cast<char*>(&m_z80Bank), sizeof(m_z80Bank));
    file.read(reinterpret_cast<char*>(&m_soundCommand), sizeof(m_soundCommand));
    file.read(reinterpret_cast<char*>(&m_soundFade), sizeof(m_soundFade));

    // Load EEPROM state
    m_eeprom.loadState(file);
}

} // namespace cps

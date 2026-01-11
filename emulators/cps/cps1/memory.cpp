#include "memory.h"
#include "cartridge.h"
#include "ppu.h"
#include "../cpu.h"
#include "../sound_cpu.h"
#include "../ppu_base.h"
#include "../apu_base.h"
#include "../cartridge_base.h"
#include "../controller.h"
#include <iostream>
#include <cstring>

/*
 * CPS1 Memory Map (68000)
 * =======================
 * 
 * 0x000000-0x3FFFFF: Program ROM (max 4MB, varies by game)
 *                    Contains the game's 68000 program code
 *                    Read-only, mapped from cartridge
 * 
 * 0x800000-0x8001FF: I/O Ports and CPS Registers (mirrored through 0x807FFF)
 *   0x800000-0x80001F: Input ports (controllers, coins, start buttons)
 *     0x800000: Player 1 inputs (low byte)
 *     0x800001: Player 1 inputs (high byte)
 *     0x800010: Player 2 inputs (low byte)
 *     0x800011: Player 2 inputs (high byte)
 *     0x800012: System inputs (coins, start buttons)
 *     0x800018: DIP switches 1
 *     0x800019: DIP switches 2
 *     0x80001A-0x80001E: Additional DIP switches (game-dependent)
 * 
 *   0x800100-0x8001FF: CPS registers and board ID
 *     0x800100+: CPS control registers (layer control, palette select, etc.)
 *                The exact layout varies by game and board type
 *                Each game reads its "Board ID" from a specific offset here
 *     0x800180+: Protection/multiplication registers (game-dependent)
 * 
 * 0x900000-0x92FFFF: Video RAM (VRAM) - 192KB
 *                    Contains tile data, palette data, scroll registers
 *                    Organized into multiple layers:
 *                    - Scroll 1/2/3 tile maps
 *                    - Palette RAM
 *                    - Sprite/Object RAM
 *                    Exact layout determined by CPS registers
 * 
 * 0xFF0000-0xFFFFFF: Work RAM - 64KB
 *                    General purpose RAM for the game program
 *                    Stack, variables, temporary data, etc.
 * 
 * 
 * CPS1 Memory Map (Z80 Sound CPU)
 * ================================
 * 
 * 0x0000-0x7FFF: Sound ROM
 *                Z80 program code for sound driver
 * 
 * 0x8000-0x9FFF: Sound RAM (2KB, mirrored)
 *                Working memory for Z80
 *                Location 0x8001 is polled for sound commands from 68000
 * 
 * 0xC000-0xC001: YM2151 FM synthesizer registers
 * 0xD000:        ADPCM sample data
 * 0xE000-0xE00F: ADPCM control registers
 */

namespace cps1 {

Memory::Memory()
    : m_cpu(nullptr)
    , m_soundCpu(nullptr)
    , m_ppu(nullptr)
    , m_apu(nullptr)
    , m_cartridge(nullptr)
    , m_controller1(nullptr)
    , m_controller2(nullptr)
    , m_protCalc{0, 0}
    , m_memProt{0x00, 0x00, 0x00, 0x00}
    , m_boardId{0x00, 0x00, 0x00} {
}

void Memory::reset() {
    // Clear RAM
    m_workRam.fill(0);
    m_soundRam.fill(0);
    
    // Reset protection calc
    m_protCalc[0] = 0;
    m_protCalc[1] = 0;
    
    // Set board ID and memProt from game database
    Cartridge* cart = static_cast<Cartridge*>(m_cartridge);
    BoardConfig config = cart->getBoardConfig();
    
    m_boardId[0] = config.boardIdOffset;
    m_boardId[1] = config.boardIdValue1;
    m_boardId[2] = config.boardIdValue2;
    
    // Set memory protection offsets
    m_memProt[0] = config.memProt[0];
    m_memProt[1] = config.memProt[1];
    m_memProt[2] = config.memProt[2];
    m_memProt[3] = config.memProt[3];
}

// ============================================================================
// 68000 Memory Access
// ============================================================================

u8 Memory::read8(u32 address) {
    // ROM (0x000000-0x3FFFFF)
    if (address < 0x400000) {
        if (m_cartridge) {
            return m_cartridge->readROM8(address);
        }
        return 0xFF;
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
    // ROM (0x000000-0x3FFFFF)
    if (address < 0x400000) {
        if (m_cartridge) {
            return m_cartridge->readROM16(address);
        }
        return 0xFFFF;
    }
    
    // I/O Ports and CPS Registers (0x800000-0x807FFF, mirrored)
    if ((address & 0xFF8000) == 0x800000) {
        // Protection multiplication result
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
    
    // VRAM (0x900000-0x92FFFF)
    if (address >= 0x900000 && address <= 0x92FFFF) {
        return readVRAM16(address - 0x900000);
    }
    
    // Work RAM (0xFF0000-0xFFFFFF)
    if (address >= 0xFF0000 && address <= 0xFFFFFF) {
        u32 offset = address & 0xFFFF;
        return (static_cast<u16>(m_workRam[offset]) << 8) | m_workRam[offset + 1];
    }
    
    // Unmapped region
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
    // ROM is read-only
    if (address < 0x400000) {
        return;
    }
    
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
    
    // VRAM (0x900000-0x92FFFF)
    if (address >= 0x900000 && address <= 0x92FFFF) {
        writeVRAM16(address - 0x900000, value);
        return;
    }
    
    // Work RAM (0xFF0000-0xFFFFFF)
    if (address >= 0xFF0000 && address <= 0xFFFFFF) {
        u32 offset = address & 0xFFFF;
        m_workRam[offset] = (value >> 8) & 0xFF;
        m_workRam[offset + 1] = value & 0xFF;
        return;
    }
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
    
    // Input ports (0x000-0x01F)
    // Based on Street Fighter II's input definitions
    // NOTE: These are typical input settings that work for most CPS1 games, but are not
    // guaranteed to work correctly for all games. Some games may use different port
    // addresses or bit layouts for their inputs.
    switch (port) {
        case 0x000:
            // Player 2 inputs (active low) - port 0x000
            if (m_controller2) {
                value = ~m_controller2->read();
            }
            return value;
            
        case 0x001:
            // Player 1 inputs (active low) - port 0x001
            if (m_controller1) {
                value = ~m_controller1->read();
            }
            return value;
            
        case 0x010:
            // Player 2 inputs (active low) - port 0x010 (some games)
            if (m_controller2) {
                value = ~m_controller2->read();
            }
            return value;
            
        case 0x011:
            // Player 1 inputs (active low) - port 0x011 (some games)
            if (m_controller1) {
                value = ~m_controller1->read();
            }
            return value;
            
        case 0x012:
        case 0x177:
            // Bit 0: P1 Kick 1
            // Bit 1: P1 Kick 2
            // Bit 2: P1 Kick 3
            // Bit 4: P2 Kick 1
            // Bit 5: P2 Kick 2
            // Bit 6: P2 Kick 3
            value = 0xFF;
            if (m_controller1) {
                value &= ~(m_controller1->readKicks() & 0x07);  // Clear bits 0-2 for P1 kicks
            }
            if (m_controller2) {
                value &= ~((m_controller2->readKicks() & 0x07) << 4);  // Clear bits 4-6 for P2 kicks
            }
            return value;
            
        case 0x018:
            // Coin/Start inputs (active low)
            // Bit 0: P1 Coin
            // Bit 1: P2 Coin
            // Bit 2: Service button
            // Bit 4: P1 Start
            // Bit 5: P2 Start
            // Bit 6: Diagnostic
            value = 0xFF;
            if (m_controller1) {
                u8 p1 = m_controller1->readCoinStart();
                if (p1 & 0x01) value &= ~0x01;  // Clear bit 0 for P1 coin
                if (p1 & 0x10) value &= ~0x10;  // Clear bit 4 for P1 start
            }
            if (m_controller2) {
                u8 p2 = m_controller2->readCoinStart();
                if (p2 & 0x01) value &= ~0x02;  // Clear bit 1 for P2 coin
                if (p2 & 0x10) value &= ~0x20;  // Clear bit 5 for P2 start
            }
            return value;
            
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
    
    // Board ID - CPS1 games read their board identifier here
    // The board ID tells the game which hardware configuration to use
    if (port >= 0x100 && port < 0x200) {
        // Check if this is the board ID location
        if (port == (0x100 + m_boardId[0])) {
            return m_boardId[1];
        }
        if (port == (0x100 + m_boardId[0] + 1)) {
            return m_boardId[2];
        }
        
        // CPS Registers - forward to PPU
        PPU* ppu = static_cast<PPU*>(m_ppu);
        return ppu->readRegister8(port - 0x100);
    }
    
    // Unmapped port - return 0xFF (bus pull-up)
    return 0xFF;
}

void Memory::writePort(u16 port, u8 value) {
    // Sound command (0x181)
    // This is how the 68000 sends commands to the Z80 sound CPU
    if (port == 0x181) {
        // Write sound command to Z80 shared memory
        // The Z80 polls address 0x001 in its RAM to check for new commands
        m_soundRam[0x001] = value;
        return;
    }
    
    // Sound fade (0x189)
    // Used by some games to fade music in/out
    if (port == 0x189) {
        // Store fade value in Z80 shared memory
        m_soundRam[0x002] = value;
        return;
    }
    
    // CPS Registers (0x100-0x1FF)
    // These control video layer scrolling, priorities, palette selection, etc.
    if (port >= 0x100 && port < 0x200) {
        u8 regNum = port - 0x100;
        
        // Forward to PPU for layer control and scroll registers
        PPU* ppu = static_cast<PPU*>(m_ppu);
        ppu->writeRegister8(regNum, value);
        
        return;
    }
}

// ============================================================================
// Z80 Memory Access
// ============================================================================

u8 Memory::readZ80(u16 address) {
    // Sound ROM (0x0000-0x7FFF)
    if (address < 0x8000) {
        // Use the cartridge's sound ROM reader (not the program ROM reader)
        if (m_cartridge) {
            auto* cart = static_cast<Cartridge*>(m_cartridge);
            return cart->readSoundROM8(address);
        }
        return 0xFF;
    }
    
    // Sound RAM (0x8000-0x9FFF, mirrored)
    if (address >= 0x8000 && address < 0xA000) {
        return m_soundRam[address & 0x7FF];
    }
    
    // YM2151 and ADPCM registers (handled by APU)
    // 0xC000-0xC001: YM2151
    // 0xD000: ADPCM data
    // 0xE000-0xE00F: ADPCM control
    
    return 0xFF;
}

void Memory::writeZ80(u16 address, u8 value) {
    // Sound ROM is read-only
    if (address < 0x8000) {
        return;
    }
    
    // Sound RAM (0x8000-0x9FFF, mirrored)
    if (address >= 0x8000 && address < 0xA000) {
        m_soundRam[address & 0x7FF] = value;
        return;
    }
    
    // Sound hardware registers (YM2151, OKI ADPCM, etc.)
    // (Implementation depends on APU)
    if (m_apu) {
        // Forward to APU for processing
    }
}

// ============================================================================
// Save/Load State
// ============================================================================

void Memory::saveState(std::ofstream& file) {
    // Save RAM
    file.write(reinterpret_cast<const char*>(m_workRam.data()), m_workRam.size());
    file.write(reinterpret_cast<const char*>(m_soundRam.data()), m_soundRam.size());
    
    // Save protection state
    file.write(reinterpret_cast<const char*>(&m_protCalc), sizeof(m_protCalc));
    file.write(reinterpret_cast<const char*>(&m_memProt), sizeof(m_memProt));
    file.write(reinterpret_cast<const char*>(&m_boardId), sizeof(m_boardId));
}

void Memory::loadState(std::ifstream& file) {
    // Load RAM
    file.read(reinterpret_cast<char*>(m_workRam.data()), m_workRam.size());
    file.read(reinterpret_cast<char*>(m_soundRam.data()), m_soundRam.size());
    
    // Load protection state
    file.read(reinterpret_cast<char*>(&m_protCalc), sizeof(m_protCalc));
    file.read(reinterpret_cast<char*>(&m_memProt), sizeof(m_memProt));
    file.read(reinterpret_cast<char*>(&m_boardId), sizeof(m_boardId));
}

} // namespace cps1

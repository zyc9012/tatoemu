#pragma once

#include "../types.h"
#include "db.h"
#include "../components/eeprom/eeprom.h"
#include <array>
#include "../components/buffer.h"

namespace cps {

class CPU;
class SoundCPU;
class PPU;
class APU;
class Cartridge;
class Controller;

// Unified Memory Map (CPS1 and CPS2):
// 0x000000-0x3FFFFF: Program ROM (max 4MB, encrypted for CPS2)
// 0x400000-0x40000F: CPS2 Registers (Frg registers, CPS2 only)
// 0x660000-0x663FFF: Extra RAM (16KB, CPS2 only)
// 0x664001: Frame toggle register (CPS2 only)
// 0x618000-0x619FFF: QSound shared RAM (4KB, CPS2 only)
// 0x708000-0x717FFF: Object RAM (64KB, CPS2 only)
// 0x800000-0x8001FF: I/O Ports and CPS Registers
// 0x900000-0x92FFFF: Video RAM (VRAM) - 192KB
// 0xFF0000-0xFFFFFF: Work RAM - 64KB

class Memory {
public:
    Memory();
    ~Memory() = default;

    void reset();
    
    // 68000 memory access
    u8 read8(u32 address);
    u16 read16(u32 address);
    u32 read32(u32 address);
    u8 read8Data(u32 address);
    u16 read16Data(u32 address);
    u32 read32Data(u32 address);
    void write8(u32 address, u8 value);
    void write16(u32 address, u16 value);
    void write32(u32 address, u32 value);
    
    // Z80 memory access
    u8 readZ80(u32 address);
    void writeZ80(u32 address, u8 value);
    u8 readZ80Opcode(u32 address);
    u8 readZ80OpcodeArg(u32 address);
    
    // Component connections
    void setCPU(CPU* cpu) { m_cpu = cpu; }
    void setSoundCPU(SoundCPU* soundCpu) { m_soundCpu = soundCpu; }
    void setPPU(PPU* ppu) { m_ppu = ppu; }
    void setAPU(APU* apu) { m_apu = apu; }
    void setCartridge(Cartridge* cartridge) { m_cartridge = cartridge; }
    void setController(Controller* controller) { m_controller = controller; }
    
    // Save/Load state
    void saveState(Buffer* buf);
    void loadState(Buffer* buf);
    
    // CPS2 raster IRQ accessors
    u16 getRasterIRQ50() const { return m_rasterIRQ50; }
    u16 getRasterIRQ52() const { return m_rasterIRQ52; }

private:
    // Component pointers
    CPU* m_cpu;
    SoundCPU* m_soundCpu;
    PPU* m_ppu;
    APU* m_apu;
    Cartridge* m_cartridge;
    Controller* m_controller;
    
    // RAM banks (common)
    std::array<u8, 64 * 1024> m_workRam;      // 0xFF0000-0xFFFFFF (64KB)
    std::array<u8, 8 * 1024> m_soundRam;      // Z80/QSound RAM (8KB) - CPS1 non-QSound: 0xD000-0xD7FF (2KB), CPS1 QSound & CPS2: 0xC000-0xCFFF (4KB) and 0xF000-0xFFFF (4KB)

    // CPS2-specific RAM banks
    std::array<u8, 16 * 1024> m_extraRam;     // 0x660000-0x663FFF (16KB, CPS2 only)
    std::array<u8, 64 * 1024> m_objRam;       // 0x708000-0x717FFF (64KB, CPS2 only)
    
    // CPS2 registers (0x400000-0x40000F, CPS2 only)
    std::array<u8, 16> m_frgRegs;
    
    // Z80 ROM banking
    u8 m_z80Bank;                              // Current ROM bank (0-15)

    // CPS2 object RAM banking
    u8 m_objectBank;                           // Current object RAM bank (0-1)
    
    // Input port reading
    u8 readPort(u16 port);
    void writePort(u16 port, u8 value);
    
    // Helper methods (now use virtual PPU methods)
    u8 readVRAM8(u32 address);
    u16 readVRAM16(u32 address);
    u32 readVRAM32(u32 address);
    void writeVRAM8(u32 address, u8 value);
    void writeVRAM16(u32 address, u16 value);
    void writeVRAM32(u32 address, u32 value);
    
    // CPS1-specific: Protection calculation
    u16 m_protCalc[2];
    
    // CPS1-specific: Memory protection offsets
    // [0] = write operand 1, [1] = write operand 2
    // [2] = read low word, [3] = read high word
    u8 m_memProt[4];
    
    // CPS1-specific: Board ID (each CPS1 game has a specific board identifier)
    u8 m_boardId[3];  // {offset, ID byte 1, ID byte 2}
    
    // Sound communication (from 68000 to Z80)
    u8 m_soundCommand;  // Sound command latch
    u8 m_soundFade;     // Sound fade value
    
    // CPS2 QSound registers
    u8 m_qscCmd[2];     // QSound command bytes [0] and [1] (written to 0xD000 and 0xD001)
    
    // CPS2-specific: Frame toggle register (0x664001)
    u8 m_n664001;
    
    // CPS2-specific: Raster interrupt registers (ports 0x50-0x53)
    u16 m_rasterIRQ50;  // Scanline for IRQ line 50 (port 0x050-0x051)
    u16 m_rasterIRQ52;  // Scanline for IRQ line 52 (port 0x052-0x053)
    
    // EEPROM
    EEPROM m_eeprom;
    
    // Helper to get CPS version from cartridge
    u8 getCPSVersion() const;
};

} // namespace cps

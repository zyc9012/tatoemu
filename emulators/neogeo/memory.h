#pragma once

#include "../types.h"
#include "consts.h"
#include <array>
#include <fstream>

namespace neogeo {

class CPU;
class SoundCPU;
class PPU;
class Cartridge;
class Controller;
class APU;

// NeoGeo Memory Map (Cartridge systems - MVS/AES):
// 0x000000-0x0003FF: Vector table (switchable BIOS/cartridge)
// 0x000400-0x0FFFFF: Program ROM (base, up to 1MB)
// 0x100000-0x1FFFFF: Work RAM (64KB mirrored)
// 0x200000-0x2FFFFF: Banked ROM (for games > 1MB, bankswitched)
// 0x300000-0x31FFFF: Input port 1 (read), Watchdog (write odd)
// 0x320000-0x33FFFF: Sound reply/Input3 (read), Sound command (write even)
// 0x340000-0x35FFFF: Input port 2
// 0x380000-0x39FFFF: Input port 3 (read), I/O port 1 (write)
// 0x3A0000-0x3BFFFF: I/O port 2 (write)
// 0x3C0000-0x3DFFFF: Video controller
// 0x400000-0x401FFF: Palette RAM (8KB, banked)
// 0x420000-0x7FFFFF: Palette RAM mirrors
// 0x800000-0xBFFFFF: Memory card (odd bytes only)
// 0xC00000-0xC003FF: BIOS vector table area (switchable)
// 0xC00400-0xC7FFFF: BIOS ROM
// 0xD00000-0xDFFFFF: NVRAM (MVS only, open bus on AES)
// 0xE00000-0xFFFFFF: Open bus (CD transfer area on NeoCD)

class Memory {
public:
    Memory();
    ~Memory() = default;

    void reset();
    
    // 68000 memory access
    u8 read8(u32 address);
    u16 read16(u32 address);
    u32 read32(u32 address);
    void write8(u32 address, u8 value);
    void write16(u32 address, u16 value);
    void write32(u32 address, u32 value);
    
    // Z80 memory access
    u8 readZ80(u32 address);
    void writeZ80(u32 address, u8 value);
    
    // Z80 I/O port access (handles bank switching)
    u8 readZ80IO(u16 port);
    void writeZ80IO(u16 port, u8 value);
    
    // Component connections
    void setCPU(CPU* cpu) { m_cpu = cpu; }
    void setSoundCPU(SoundCPU* soundCpu) { m_soundCpu = soundCpu; }
    void setPPU(PPU* ppu) { m_ppu = ppu; }
    void setCartridge(Cartridge* cartridge) { m_cartridge = cartridge; }
    void setController(Controller* controller) { m_controller = controller; }
    void setAPU(APU* apu) { m_apu = apu; }
    
    // Save/Load state
    void saveState(std::ofstream& file);
    void loadState(std::ifstream& file);
    
    // Video controller access (for PPU)
    u16 readVideoController(u32 address);
    void writeVideoController(u32 address, u16 value);
    
    // Palette access (for PPU)
    u16 readPalette16(u32 address) const;
    
    // Text ROM selection (for PPU)
    bool isBIOSTextROMEnabled() const { return m_biosTextRomEnabled; }

    // IRQ control access (for PPU)
    u16 getIRQControl() const { return m_irqControl; }

private:
    // Component pointers
    CPU* m_cpu;
    SoundCPU* m_soundCpu;
    PPU* m_ppu;
    Cartridge* m_cartridge;
    Controller* m_controller;
    APU* m_apu;
    
    // RAM banks
    std::array<u8, WORK_RAM_SIZE> m_workRam;      // 0x100000-0x1FFFFF (64KB mirrored)
    std::array<u8, SRAM_SIZE> m_sram;             // Memory card (64KB)
    std::array<u8, 0x10000> m_nvram;              // MVS NVRAM 0xD00000-0xDFFFFF (64KB mirrored)
    std::array<u16, PALETTE_RAM_SIZE> m_paletteRam;  // Two banks of 4096 colors each (8KB per bank)
    std::array<u8, Z80_RAM_SIZE> m_z80Ram;        // Z80 RAM (0xF800-0xFFFF, 2KB)
    
    // I/O registers
    u8 m_inputSelect;          // Input port selection
    bool m_sramWritable;       // SRAM write protection
    u8 m_paletteBank;          // Current palette bank (0 or 1)
    bool m_darkenPalette;      // Shadow/darken palette flag
    bool m_biosTextRomEnabled; // True when BIOS text ROM is enabled
    
    // Video controller registers
    u16 m_graphicsRamPointer;  // Graphics RAM pointer
    u16 m_graphicsRamModulo;   // Graphics RAM modulo
    u16 m_irqControl;          // IRQ control register
    u32 m_irqOffset;           // IRQ offset register
    
    // Watchdog timer (simplified)
    s32 m_watchdog;
    
    // 68K ROM banking (for games > 1MB)
    u32 m_programRomBank;  // Bank offset for 0x200000-0x2FFFFF area
    
    // Z80 banking
    // Bank 0: 0x8000-0xBFFF (16KB, bank << 14)
    // Bank 1: 0xC000-0xDFFF (8KB, bank << 13)
    // Bank 2: 0xE000-0xEFFF (4KB, bank << 12)
    // Bank 3: 0xF000-0xF7FF (2KB, bank << 11)
    u8 m_z80Bank0;  // 4-bit bank number (0-15)
    u8 m_z80Bank1;  // 5-bit bank number (0-31)
    u8 m_z80Bank2;  // 6-bit bank number (0-63)
    u8 m_z80Bank3;  // 7-bit bank number (0-127)
    bool m_z80BiosRomMapped;  // True when Z80 BIOS ROM is mapped at 0x0000-0x7FFF
    
    // Helper methods
    u8 readPalette8(u32 address);
    u16 readPalette16Private(u32 address);
    void writePalette8(u32 address, u8 value);
    void writePalette16(u32 address, u16 value);
    
    // I/O port handlers
    void writeIO1(u8 offset, u8 value);
    void writeIO2(u8 offset, u8 value);
};

} // namespace neogeo

#pragma once

#include "../../types.h"
#include "../memory_base.h"
#include <array>
#include <fstream>

namespace cps2 {

// CPS2 Memory Map:
// 0x000000-0x3FFFFF: Program ROM (max 4MB, encrypted)
// 0x400000-0x40000F: CPS2 Registers (Frg registers)
// 0x660000-0x663FFF: Extra RAM (16KB)
// 0x708000-0x717FFF: Object RAM (64KB)
// 0x800000-0x8001FF: I/O Ports (mirrored through 0x807FFF)
// 0x900000-0x92FFFF: Video RAM (VRAM) - 192KB
// 0xFF0000-0xFFFFFF: Work RAM - 64KB

class Memory : public cps::MemoryBase {
public:
    Memory();
    ~Memory() = default;

    void reset() override;
    
    // 68000 memory access
    u8 read8(u32 address) override;
    u16 read16(u32 address) override;
    u32 read32(u32 address) override;
    void write8(u32 address, u8 value) override;
    void write16(u32 address, u16 value) override;
    void write32(u32 address, u32 value) override;
    
    // Z80 memory access
    u8 readZ80(u16 address) override;
    void writeZ80(u16 address, u8 value) override;
    
    // Component connections
    void setCPU(cps::CPU* cpu) override { m_cpu = cpu; }
    void setSoundCPU(cps::SoundCPU* soundCpu) override { m_soundCpu = soundCpu; }
    void setPPU(cps::PPUBase* ppu) override { m_ppu = ppu; }
    void setAPU(cps::APUBase* apu) override { /* Not used for CPS2 yet */ }
    void setCartridge(cps::CartridgeBase* cartridge) override { m_cartridge = cartridge; }
    void setController1(cps::Controller* controller) override { m_controller1 = controller; }
    void setController2(cps::Controller* controller) override { m_controller2 = controller; }
    
    // Save/Load state
    void saveState(std::ofstream& file) override;
    void loadState(std::ifstream& file) override;

private:
    // Component pointers
    cps::CPU* m_cpu;
    cps::SoundCPU* m_soundCpu;
    cps::PPUBase* m_ppu;
    cps::CartridgeBase* m_cartridge;
    cps::Controller* m_controller1;
    cps::Controller* m_controller2;
    
    // RAM banks
    std::array<u8, 64 * 1024> m_workRam;      // 0xFF0000-0xFFFFFF (64KB)
    std::array<u8, 16 * 1024> m_extraRam;      // 0x660000-0x663FFF (16KB)
    std::array<u8, 64 * 1024> m_objRam;        // 0x708000-0x717FFF (64KB)
    // Note: VRAM is now owned by PPU (192KB at 0x900000-0x92FFFF)
    std::array<u8, 2 * 1024> m_soundRam;      // Z80 RAM (2KB)
    
    // CPS2 registers (0x400000-0x40000F)
    std::array<u8, 16> m_frgRegs;
    
    // Z80 ROM banking
    u8 m_z80Bank;                              // Current ROM bank (0-15)
    
    // Input port reading
    u8 readPort(u16 port);
    void writePort(u16 port, u8 value);
    
    // Helper methods
    u8 readVRAM8(u32 address);
    u16 readVRAM16(u32 address);
    u32 readVRAM32(u32 address);
    void writeVRAM8(u32 address, u8 value);
    void writeVRAM16(u32 address, u16 value);
    void writeVRAM32(u32 address, u32 value);
    
    // Sound communication (from 68000 to Z80) - placeholder for future
    u8 m_soundCommand;  // Sound command latch
    u8 m_soundFade;     // Sound fade value
    
    // CPS2 specific: 0x664001 register (frame toggle)
    u8 m_n664001;
};

} // namespace cps2

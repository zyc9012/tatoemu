#pragma once

#include "../../types.h"
#include "../memory_base.h"
#include <array>
#include <fstream>

namespace cps1 {

// CPS1 Memory Map:
// 0x000000-0x3FFFFF: Program ROM (max 4MB)
// 0x800000-0x8001FF: I/O Ports and CPS Registers
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
    void setAPU(cps::APUBase* apu) override { m_apu = apu; }
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
    cps::APUBase* m_apu;
    cps::CartridgeBase* m_cartridge;
    cps::Controller* m_controller1;
    cps::Controller* m_controller2;
    
    // RAM banks
    std::array<u8, 64 * 1024> m_workRam;      // 0xFF0000-0xFFFFFF (64KB)
    // Note: VRAM is now owned by PPU (192KB at 0x900000-0x92FFFF)
    std::array<u8, 256> m_cpsRegs;            // CPS1 registers (0x800100-0x8001FF)
    std::array<u8, 2 * 1024> m_soundRam;      // Z80 RAM (2KB)
    
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
    
    // Protection calculation (some games use multiplication at specific addresses)
    u16 m_protCalc[2];
    
    // Board ID (each CPS1 game has a specific board identifier)
    u8 m_boardId[3];  // {offset, ID byte 1, ID byte 2}
};

} // namespace cps1

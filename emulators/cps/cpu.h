#pragma once

#include "../types.h"
#include "../components/cpu/m68k/m68k.h"
#include "../components/buffer.h"

namespace cps {

class Memory;
class Cartridge;

// Motorola 68000 CPU emulator (shared between CPS1 and CPS2)
// 
// Uses the Musashi m68k emulator for accurate 68000 emulation.
class CPU {
public:
    CPU();
    ~CPU();

    void reset();
    u32 step(u32 cycles);

    u32 frameCycles() const { return m_executing ? m_cycles + m68k_cycles_run() : m_cycles; }
    u32 cyclesPerFrame() const { return m_cyclesPerFrame; }
    void endFrame() { m_cycles -= m_cyclesPerFrame; }
    
    void setMemory(Memory* memory) { m_memory = memory; }
    void setCartridge(Cartridge* cartridge) { m_cartridge = cartridge; }
    
    // Interrupt handling
    void irq(u8 level);  // IRQ with priority level (1-7)
    
    // Save/Load state
    void saveState(Buffer* buf);
    void loadState(Buffer* buf);

private:
    Memory* m_memory;
    Cartridge* m_cartridge;
    u32 m_cycles;
    u32 m_cyclesPerFrame;
    bool m_executing;
};

} // namespace cps

#pragma once

#include "../types.h"
#include "../../components/cpu/m68k/m68k.h"
#include "../../components/buffer.h"

namespace neogeo {

class Memory;

// Motorola 68000 CPU emulator for NeoGeo
// Uses the Musashi m68k emulator for accurate 68000 emulation.
class CPU {
public:
    CPU();
    ~CPU();

    void reset();
    u32 step(u32 cycles);
    
    u32 getCycles() const { return m_inStep ? m_cycles + m68k_cycles_run() : m_cycles; }
    void setMemory(Memory* memory) { m_memory = memory; }
    
    // Interrupt handling
    void irq(u8 level);  // IRQ with priority level (1-7)
    
    // Save/Load state
    void saveState(Buffer* buf);
    void loadState(Buffer* buf);

private:
    Memory* m_memory;
    u32 m_cycles;
    bool m_inStep;
};

} // namespace neogeo

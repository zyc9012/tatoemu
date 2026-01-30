#pragma once

#include "../types.h"
#include "../components/buffer.h"

namespace cps {

class Memory;

// Motorola 68000 CPU emulator (shared between CPS1 and CPS2)
// 
// Uses the Musashi m68k emulator for accurate 68000 emulation.
class CPU {
public:
    CPU();
    ~CPU();

    void reset();
    u32 step(u32 cycles);
    
    void setMemory(Memory* memory) { m_memory = memory; }
    
    // Interrupt handling
    void irq(u8 level);  // IRQ with priority level (1-7)
    
    // Save/Load state
    void saveState(Buffer* buf);
    void loadState(Buffer* buf);

private:
    Memory* m_memory;
};

} // namespace cps

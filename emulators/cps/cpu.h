#pragma once

#include "../types.h"
#include <fstream>

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
    void step();
    
    u32 getCycles() const { return m_cycles; }
    void setMemory(Memory* memory) { m_memory = memory; }
    
    // Interrupt handling
    void irq(u8 level);  // IRQ with priority level (1-7)
    void resetInterrupt();
    
    // Save/Load state
    void saveState(std::ofstream& file);
    void loadState(std::ifstream& file);

private:
    Memory* m_memory;
    u32 m_cycles;
};

} // namespace cps

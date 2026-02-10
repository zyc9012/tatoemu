#pragma once

#include "../types.h"
#include "../components/buffer.h"

namespace nes {

class Memory;

class CPU {
public:
    CPU();
    ~CPU();

    void setMemory(Memory* memory) { m_memory = memory; }
    Memory* getMemory() const { return m_memory; }

    void reset();
    void step(u32 cycles);

    // Interrupt handling
    void nmi();
    void irq();

    // Cycle tracking
    u32 getCycles() const { return m_cycles; }
    void resetCycles() { m_cycles = 0; }
    bool isStalled() const { return m_stallCycles > 0; }
    void stall(u32 cycles) { m_stallCycles += cycles; }

    // DMA handling
    void triggerOAMDMA(u8 page);

    // Save/Load state
    void saveState(Buffer* buf);
    void loadState(Buffer* buf);

private:
    Memory* m_memory;

    u32 m_cycles;
    u32 m_stallCycles;
};

} // namespace nes

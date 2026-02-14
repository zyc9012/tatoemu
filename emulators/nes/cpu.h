#pragma once

#include "../types.h"
#include "../components/buffer.h"
#include <memory>

class M6502;

namespace nes {

class Memory;

/**
 * @brief NES CPU wrapper for the 2A03 processor
 * 
 * The 2A03 is a modified 6502 without decimal mode support.
 * This class wraps the modern m6502 implementation and provides
 * NES-specific functionality like DMA cycles and save states.
 */
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
    void irq(u32 state);

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
    std::unique_ptr<M6502> m_cpu;

    u32 m_cycles;
    u32 m_stallCycles;
};

} // namespace nes

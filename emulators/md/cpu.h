#pragma once

#include "consts.h"
#include "../types.h"
#include "../components/buffer.h"

namespace md {

class Memory;
class VDP;

// ---------------------------------------------------------------------------
// Motorola 68000 main CPU.
// ---------------------------------------------------------------------------
class CPU {
public:
    CPU();
    ~CPU();

    void reset();
    u32 step(u32 cycles);

    // Cycles executed so far this frame (valid from inside memory callbacks).
    u32 frameCycles() const;
    void endFrame(u32 frameCycles) { m_cycles -= frameCycles; }

    void setMemory(Memory* memory) { m_memory = memory; }
    void setVDP(VDP* vdp) { m_vdp = vdp; }

    // The VDP is the only interrupt source and owns the pending state for both
    // of its autovectored levels, so it simply drives the line to the level it
    // wants serviced (0 releases it).
    void setIRQLevel(u8 level);

    // Steal cycles from the current timeslice (VDP DMA / FIFO stalls).
    void stall(u32 cycles);

    // Halt/resume the 68000 while the Z80 owns the bus is not modelled; the
    // bus request path only affects Z80 execution in this implementation.

    void saveState(Buffer* buf);
    void loadState(Buffer* buf);

private:
    Memory* m_memory = nullptr;
    VDP* m_vdp = nullptr;
    u32 m_cycles = 0;
    // Cycles taken from the current timeslice by VDP DMA, and the remainder
    // that did not fit and must be charged to the next slice.
    u32 m_stolenCycles = 0;
    u32 m_pendingStall = 0;
    bool m_executing = false;
};

} // namespace md

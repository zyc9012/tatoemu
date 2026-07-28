#pragma once

#include "consts.h"
#include "../types.h"
#include "../components/buffer.h"
#include "../components/cpu/z80/z80.h"

namespace md {

class Memory;

// ---------------------------------------------------------------------------
// Z80 sound CPU.
// ---------------------------------------------------------------------------
class SoundCPU {
public:
    SoundCPU();
    ~SoundCPU();

    void reset();
    u32 step(u32 cycles);

    u32 frameCycles() const { return m_cycles; }
    void endFrame(u32 frameCycles) {
        m_cycles = (m_cycles > frameCycles) ? m_cycles - frameCycles : 0;
    }

    void setMemory(Memory* memory) { m_memory = memory; }
    Memory* getMemory() const { return m_memory; }

    // 68000-controlled lines (0xA11100 / 0xA11200)
    void setBusRequest(bool requested);
    void setResetLine(bool held);
    bool isBusGranted() const { return m_busRequested; }
    bool isRunning() const { return !m_busRequested && !m_resetHeld; }

    void irq(bool state);

    void saveState(Buffer* buf);
    void loadState(Buffer* buf);

private:
    Memory* m_memory = nullptr;
    Z80 m_z80;
    u32 m_cycles = 0;

    bool m_busRequested = false;
    bool m_resetHeld = true;
};

} // namespace md

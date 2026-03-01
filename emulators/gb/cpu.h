#pragma once

#include "types.h"
#include "../components/buffer.h"
#include "../components/cpu/sm83/sm83.h"

namespace gb {

class MMU;

// Interrupt flags
enum Interrupts {
    INT_VBLANK   = 0x01,
    INT_LCD_STAT = 0x02,
    INT_TIMER    = 0x04,
    INT_SERIAL   = 0x08,
    INT_JOYPAD   = 0x10
};

// GB CPU — thin wrapper around the portable SM83 core.
// The SM83 accesses memory through function-pointer callbacks; this class
// sets them up so they route through the GB MMU, and manages the IF register.
class CPU {
public:
    CPU();
    ~CPU();

    void setMMU(MMU* mmu);
    void reset(bool useBootrom = false);
    void setGBCMode(bool enabled) { m_gbcMode = enabled; }
    bool isGBCMode() const { return m_gbcMode; }
    u32 step(u32 cycles);

    void requestInterrupt(u8 interrupt);
    bool isHalted() const { return m_sm83.isHalted(); }

    // Save/Load state
    void saveState(Buffer* buf);
    void loadState(Buffer* buf);

    // Accessors for callbacks
    MMU* getMMU() const { return m_mmu; }

private:
    SM83 m_sm83;
    MMU* m_mmu;
    bool m_gbcMode;
};

} // namespace gb


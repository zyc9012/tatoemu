#pragma once

#include "types.h"
#include "consts.h"
#include "../components/buffer.h"

namespace gba {

class Memory;
class APU;

struct TimerChannel {
    u16 reload;
    u16 counter;
    u16 control;
    int prescaler;
    int prescalerShift;
    int prescalerCount;
    bool enabled;
    bool overflow;
    bool countUp;
};

class Timer {
public:
    Timer();
    ~Timer();

    void setMemory(Memory* memory) { m_memory = memory; }
    void setAPU(APU* apu) { m_apu = apu; }
    
    void reset();
    void step(int cycles);
    
    void writeRegister(u32 offset, u16 value);
    u16 readCounter(int channel) const;
    
    // Save/Load state
    void saveState(Buffer* buf);
    void loadState(Buffer* buf);

private:
    void reload(int channel);
    void checkOverflow(int channel);
    
    Memory* m_memory = nullptr;
    APU* m_apu = nullptr;
    TimerChannel m_timers[4];
};

} // namespace gba

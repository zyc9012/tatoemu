#pragma once

#include "types.h"
#include "consts.h"
#include "../components/buffer.h"
#include "../components/scheduler.h"

namespace gba {

class Memory;
class APU;

struct TimerChannel {
    u16 reload;
    u16 counter;       // counter value at last update (start/overflow)
    u16 control;
    int prescalerShift;
    int prescalerMask;
    bool enabled;
    bool countUp;
    bool irqEnabled;
    u64 startTimestamp; // scheduler timestamp when counter was last set
    SchedulerEvent overflowEvent;
};

class Timer {
public:
    Timer();
    ~Timer();

    void setMemory(Memory* memory) { m_memory = memory; }
    void setAPU(APU* apu) { m_apu = apu; }
    void setScheduler(Scheduler* scheduler) { m_scheduler = scheduler; }
    
    void reset();
    
    void writeRegister(u32 offset, u16 value);
    u16 readCounter(int channel) const;
    
    // Save/Load state
    void saveState(Buffer* buf);
    void loadState(Buffer* buf);

private:
    void startTimer(int channel);
    void stopTimer(int channel);
    void handleOverflow(int channel);
    void scheduleEvents();
    u16 computeCounter(int channel) const;

    // Event callback
    static void onTimerOverflow(void* ctx, int channel);
    
    Memory* m_memory = nullptr;
    APU* m_apu = nullptr;
    Scheduler* m_scheduler = nullptr;
    TimerChannel m_timers[4];
};

} // namespace gba

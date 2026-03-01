#include "timer.h"
#include "memory.h"
#include "apu.h"

namespace gba {

Timer::Timer() {
    reset();
}

Timer::~Timer() {}

void Timer::reset() {
    for (int i = 0; i < 4; i++) {
        m_timers[i] = {};
        m_timers[i].reload = 0;
        m_timers[i].counter = 0;
        m_timers[i].control = 0;
        m_timers[i].prescaler = 1;
        m_timers[i].prescalerShift = 0;
        m_timers[i].prescalerCount = 0;
        m_timers[i].enabled = false;
        m_timers[i].overflow = false;
        m_timers[i].countUp = false;
    }
}

void Timer::step(int cycles) {
    // Clear overflow flags from previous step
    m_timers[0].overflow = false;
    m_timers[1].overflow = false;
    m_timers[2].overflow = false;
    m_timers[3].overflow = false;

    for (int i = 0; i < 4; i++) {
        TimerChannel& t = m_timers[i];
        if (!t.enabled) continue;

        if (t.countUp) {
            // Count-up mode: increment when previous timer overflows
            if (m_timers[i - 1].overflow) {
                t.counter++;
                if (t.counter == 0) {
                    checkOverflow(i);
                }
            }
        } else {
            // Normal mode: count CPU cycles with prescaler
            t.prescalerCount += cycles;

            if (t.prescalerCount < t.prescaler) continue;

            // Use bit shift/mask instead of division (prescaler is always a power of 2)
            int ticks = t.prescalerCount >> t.prescalerShift;
            t.prescalerCount &= (t.prescaler - 1);

            // Fast path: no overflow
            u32 distToOverflow = 0x10000u - t.counter;
            if (static_cast<u32>(ticks) < distToOverflow) {
                t.counter += ticks;
            } else {
                // First overflow
                ticks -= distToOverflow;
                checkOverflow(i);

                // Handle additional overflows (rare: only when ticks spans multiple periods)
                if (ticks > 0) {
                    u32 period = 0x10000u - t.reload;
                    if (period > 0) {
                        u32 extra = static_cast<u32>(ticks) / period;
                        u32 remainder = static_cast<u32>(ticks) % period;
                        for (u32 j = 0; j < extra; j++) {
                            checkOverflow(i);
                        }
                        t.counter = t.reload + remainder;
                    }
                    // period == 0 (reload == 0): infinite overflows, just stay at reload
                }
            }
        }
    }
}

void Timer::reload(int channel) {
    m_timers[channel].counter = m_timers[channel].reload;
}

void Timer::checkOverflow(int channel) {
    m_timers[channel].overflow = true;
    reload(channel);
    
    // Trigger IRQ if enabled
    if (m_timers[channel].control & (1 << 6)) {
        m_memory->requestIRQ(IRQ::TIMER0 << channel);
    }

    // Notify APU of timer overflow (for DMA sound / FIFO channels)
    if (channel == 0 || channel == 1) {
        m_apu->onTimerOverflow(channel);
    }
}

void Timer::writeRegister(u32 offset, u16 value) {
    int channel = -1;
    bool isControl = false;
    
    if (offset >= IO::TM0CNT_L && offset <= IO::TM3CNT_H) {
        channel = (offset - IO::TM0CNT_L) / 4;
        isControl = ((offset - IO::TM0CNT_L) % 4) >= 2;
    }
    
    if (channel < 0 || channel > 3) return;
    
    if (isControl) {
        bool wasEnabled = m_timers[channel].enabled;
        m_timers[channel].control = value;
        m_timers[channel].enabled = (value & (1 << 7)) != 0;
        
        int prescalerBits = value & 3;
        m_timers[channel].prescaler = TIMER_PRESCALER[prescalerBits];
        m_timers[channel].prescalerShift = TIMER_PRESCALER_SHIFT[prescalerBits];
        m_timers[channel].countUp = (value & (1 << 2)) && (channel > 0);
        
        // Reload counter when transitioning from disabled to enabled
        if (!wasEnabled && m_timers[channel].enabled) {
            reload(channel);
            m_timers[channel].prescalerCount = 0;
        }
    } else {
        m_timers[channel].reload = value;
    }
}

u16 Timer::readCounter(int channel) const {
    if (channel >= 0 && channel < 4) {
        return m_timers[channel].counter;
    }
    return 0;
}

void Timer::saveState(Buffer* buf) {
    for (int i = 0; i < 4; i++) {
        buffer_write(buf, &m_timers[i], sizeof(TimerChannel));
    }
}

void Timer::loadState(Buffer* buf) {
    for (int i = 0; i < 4; i++) {
        buffer_read(buf, &m_timers[i], sizeof(TimerChannel));
    }
}

} // namespace gba

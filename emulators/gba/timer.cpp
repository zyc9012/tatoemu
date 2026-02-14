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
        m_timers[i].prescalerCount = 0;
        m_timers[i].enabled = false;
        m_timers[i].overflow = false;
    }
}

void Timer::step(int cycles) {
    // Clear overflow flags from previous step
    for (int i = 0; i < 4; i++) {
        m_timers[i].overflow = false;
    }
    
    for (int i = 0; i < 4; i++) {
        if (!m_timers[i].enabled) continue;
        
        bool countUp = (m_timers[i].control & (1 << 2)) && (i > 0);
        
        if (countUp) {
            // Count-up mode: increment when previous timer overflows
            if (m_timers[i - 1].overflow) {
                m_timers[i].counter++;
                if (m_timers[i].counter == 0) {
                    checkOverflow(i);
                }
            }
        } else {
            // Normal mode: count CPU cycles with prescaler
            m_timers[i].prescalerCount += cycles;
            while (m_timers[i].prescalerCount >= m_timers[i].prescaler) {
                m_timers[i].prescalerCount -= m_timers[i].prescaler;
                m_timers[i].counter++;
                if (m_timers[i].counter == 0) {
                    checkOverflow(i);
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
        if (m_memory) {
            m_memory->requestIRQ(IRQ::TIMER0 << channel);
        }
    }

    // Notify APU of timer overflow (for DMA sound / FIFO channels)
    if (m_apu && (channel == 0 || channel == 1)) {
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

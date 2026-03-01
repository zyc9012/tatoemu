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
        if (m_scheduler && m_scheduler->isScheduled(m_timers[i].overflowEvent)) {
            m_scheduler->cancel(m_timers[i].overflowEvent);
        }
        m_timers[i] = {};
        m_timers[i].reload = 0;
        m_timers[i].counter = 0;
        m_timers[i].control = 0;
        m_timers[i].prescalerShift = 0;
        m_timers[i].prescalerMask = 0;
        m_timers[i].enabled = false;
        m_timers[i].countUp = false;
        m_timers[i].irqEnabled = false;
        m_timers[i].startTimestamp = 0;
        // Set up overflow event callback
        m_timers[i].overflowEvent.callback = onTimerOverflow;
        m_timers[i].overflowEvent.context = this;
        m_timers[i].overflowEvent.userData = i;
    }
}

u16 Timer::computeCounter(int channel) const {
    const auto& t = m_timers[channel];
    if (!t.enabled || t.countUp) return t.counter;
    if (!m_scheduler->isScheduled(t.overflowEvent)) return t.counter;
    u64 elapsed = m_scheduler->now() - t.startTimestamp;
    u32 ticks = static_cast<u32>(elapsed >> t.prescalerShift);
    return static_cast<u16>(t.counter + ticks);
}

u16 Timer::readCounter(int channel) const {
    if (channel < 0 || channel > 3) return 0;
    return computeCounter(channel);
}

void Timer::startTimer(int channel) {
    auto& t = m_timers[channel];
    int prescalerOffset = static_cast<int>(m_scheduler->now()) & t.prescalerMask;
    t.startTimestamp = m_scheduler->now() - prescalerOffset;
    int delay = ((0x10000 - t.counter) << t.prescalerShift) - prescalerOffset;
    if (delay <= 0) delay = 0;
    m_scheduler->schedule(t.overflowEvent, delay);
}

void Timer::stopTimer(int channel) {
    auto& t = m_timers[channel];
    if (m_scheduler->isScheduled(t.overflowEvent)) {
        // Snapshot current counter value
        t.counter = computeCounter(channel);
        m_scheduler->cancel(t.overflowEvent);
    }
}

void Timer::handleOverflow(int channel) {
    auto& t = m_timers[channel];

    // Reload counter
    t.counter = t.reload;
    t.startTimestamp = m_scheduler->now();

    // Trigger IRQ if enabled
    if (t.irqEnabled) {
        m_memory->requestIRQ(IRQ::TIMER0 << channel);
    }

    // Notify APU of timer overflow (for DMA sound / FIFO channels)
    if (channel <= 1) {
        m_apu->onTimerOverflow(channel);
    }

    // Handle cascade: check if next timer is count-up
    if (channel < 3) {
        auto& next = m_timers[channel + 1];
        if (next.enabled && next.countUp) {
            next.counter++;
            if (next.counter == 0) {
                handleOverflow(channel + 1);
            }
        }
    }

    // Schedule next overflow
    int delay = (0x10000 - t.reload) << t.prescalerShift;
    if (delay <= 0) delay = 1; // prevent zero-period infinite loop
    m_scheduler->schedule(t.overflowEvent, delay);
}

void Timer::onTimerOverflow(void* ctx, int channel) {
    static_cast<Timer*>(ctx)->handleOverflow(channel);
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
        auto& t = m_timers[channel];
        bool wasEnabled = t.enabled;

        // If currently running a non-cascade timer, stop it (snapshot counter)
        if (wasEnabled && !t.countUp) {
            stopTimer(channel);
        }

        t.control = value;
        t.enabled = (value & (1 << 7)) != 0;
        t.irqEnabled = (value & (1 << 6)) != 0;
        t.countUp = (value & (1 << 2)) && (channel > 0);

        int prescalerBits = value & 3;
        t.prescalerShift = TIMER_PRESCALER_SHIFT[prescalerBits];
        t.prescalerMask = (1 << t.prescalerShift) - 1;

        if (t.enabled) {
            if (!wasEnabled) {
                // Disabled → Enabled: reload counter
                t.counter = t.reload;
            }
            if (!t.countUp) {
                startTimer(channel);
            }
        }
    } else {
        m_timers[channel].reload = value;
    }
}

void Timer::scheduleEvents() {
    for (int i = 0; i < 4; i++) {
        // Re-initialize event callbacks (lost during loadState)
        m_timers[i].overflowEvent.callback = onTimerOverflow;
        m_timers[i].overflowEvent.context = this;
        m_timers[i].overflowEvent.userData = i;

        if (m_timers[i].enabled && !m_timers[i].countUp) {
            startTimer(i);
        }
    }
}

void Timer::saveState(Buffer* buf) {
    for (int i = 0; i < 4; i++) {
        // Snapshot current counter before saving
        u16 counter = computeCounter(i);
        buffer_write(buf, &m_timers[i].reload, sizeof(u16));
        buffer_write(buf, &counter, sizeof(u16));
        buffer_write(buf, &m_timers[i].control, sizeof(u16));
        buffer_write(buf, &m_timers[i].prescalerShift, sizeof(int));
        buffer_write(buf, &m_timers[i].prescalerMask, sizeof(int));
        buffer_write(buf, &m_timers[i].enabled, sizeof(bool));
        buffer_write(buf, &m_timers[i].countUp, sizeof(bool));
        buffer_write(buf, &m_timers[i].irqEnabled, sizeof(bool));
    }
}

void Timer::loadState(Buffer* buf) {
    // Cancel any existing events
    for (int i = 0; i < 4; i++) {
        if (m_scheduler && m_scheduler->isScheduled(m_timers[i].overflowEvent)) {
            m_scheduler->cancel(m_timers[i].overflowEvent);
        }
    }

    for (int i = 0; i < 4; i++) {
        buffer_read(buf, &m_timers[i].reload, sizeof(u16));
        buffer_read(buf, &m_timers[i].counter, sizeof(u16));
        buffer_read(buf, &m_timers[i].control, sizeof(u16));
        buffer_read(buf, &m_timers[i].prescalerShift, sizeof(int));
        buffer_read(buf, &m_timers[i].prescalerMask, sizeof(int));
        buffer_read(buf, &m_timers[i].enabled, sizeof(bool));
        buffer_read(buf, &m_timers[i].countUp, sizeof(bool));
        buffer_read(buf, &m_timers[i].irqEnabled, sizeof(bool));
    }

    // Reschedule overflow events
    scheduleEvents();
}

} // namespace gba

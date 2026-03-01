#include "timer.h"
#include "cpu.h"

namespace gb {

Timer::Timer()
    : m_cpu(nullptr)
    , m_dividerCounter(0)
    , m_div(0)
    , m_tima(0)
    , m_tma(0)
    , m_tac(0)
    , m_timerCounter(0)
    , m_timerOverflow(false)
    , m_overflowDelay(0) {
}

Timer::~Timer() {
}

void Timer::setCPU(CPU* cpu) {
    m_cpu = cpu;
}

void Timer::reset() {
    m_dividerCounter = 0;
    m_div = 0;
    m_tima = 0;
    m_tma = 0;
    m_tac = 0;
    m_timerCounter = 0;
    m_timerOverflow = false;
    m_overflowDelay = 0;
}

void Timer::step(u32 cycles) {
    // Update DIV register (always runs)
    updateDivider(cycles);
    
    // Handle timer overflow delay
    if (m_timerOverflow) {
        if (m_overflowDelay > 0) {
            m_overflowDelay--;
            if (m_overflowDelay == 0) {
                // Reload TIMA from TMA and request interrupt
                m_tima = m_tma;
                m_cpu->requestInterrupt(INT_TIMER);
                m_timerOverflow = false;
            }
        }
    }
    
    // Update TIMA register (only if timer is enabled)
    if (m_tac & 0x04) {
        updateTimer(cycles);
    }
}

void Timer::updateDivider(u32 cycles) {
    // DIV register increments at 16384 Hz at normal speed (CPU clock / 256)
    // In double speed mode, the CPU clock is 2x faster, so DIV increments 2x faster
    // (at 32768 Hz)
    const u32 threshold = 256;
    m_dividerCounter += cycles;
    
    // Update DIV register for every threshold cycles
    while (m_dividerCounter >= threshold) {
        m_dividerCounter -= threshold;
        m_div++;
    }
}

void Timer::updateTimer(u32 cycles) {
    m_timerCounter += cycles;
    
    // Timer frequency is based on CPU clock, so in double speed mode
    // it runs 2x faster
    u32 frequency = getTimerFrequency();
    
    while (m_timerCounter >= frequency) {
        m_timerCounter -= frequency;
        
        // Increment TIMA
        if (m_tima == 0xFF) {
            // Overflow will occur
            m_tima = 0;
            m_timerOverflow = true;
            // Interrupt fires after 4 M-cycles
            m_overflowDelay = 4;
        } else {
            m_tima++;
        }
    }
}

u32 Timer::getTimerFrequency() const {
    // Return the number of cycles per TIMA increment based on TAC bits 0-1
    switch (m_tac & 0x03) {
        case 0x00: return 1024; // CPU Clock / 1024 (4096 Hz)
        case 0x01: return 16;   // CPU Clock / 16 (262144 Hz)
        case 0x02: return 64;   // CPU Clock / 64 (65536 Hz)
        case 0x03: return 256;  // CPU Clock / 256 (16384 Hz)
        default: return 1024;
    }
}

u8 Timer::read(u16 address) const {
    switch (address) {
        case 0xFF04: // DIV - Divider Register
            return m_div;
        case 0xFF05: // TIMA - Timer Counter
            return m_tima;
        case 0xFF06: // TMA - Timer Modulo
            return m_tma;
        case 0xFF07: // TAC - Timer Control
            return m_tac | 0xF8; // Upper 5 bits are always 1
        default:
            return 0xFF;
    }
}

void Timer::write(u16 address, u8 value) {
    switch (address) {
        case 0xFF04: // DIV - Divider Register
            // Writing any value to DIV resets it to 0
            m_div = 0;
            m_dividerCounter = 0;
            break;
        case 0xFF05: // TIMA - Timer Counter
            // Writing to TIMA during the overflow delay cancels the interrupt
            if (m_overflowDelay > 0 && m_timerOverflow) {
                m_timerOverflow = false;
                m_overflowDelay = 0;
            }
            m_tima = value;
            break;
        case 0xFF06: // TMA - Timer Modulo
            m_tma = value;
            break;
        case 0xFF07: // TAC - Timer Control
            {
                u8 oldTAC = m_tac;
                m_tac = value & 0x07; // Only lower 3 bits are used
                
                // If timer was disabled and is now enabled, reset counter
                if ((oldTAC & 0x04) == 0 && (m_tac & 0x04) != 0) {
                    m_timerCounter = 0;
                }
                
                // If frequency changed while timer was enabled, reset counter
                if ((m_tac & 0x04) && ((oldTAC & 0x03) != (m_tac & 0x03))) {
                    m_timerCounter = 0;
                }
            }
            break;
        default:
            break;
    }
}

void Timer::saveState(Buffer* buf) {
    buffer_write(buf, &m_dividerCounter, sizeof(m_dividerCounter));
    buffer_write(buf, &m_div, sizeof(m_div));
    buffer_write(buf, &m_tima, sizeof(m_tima));
    buffer_write(buf, &m_tma, sizeof(m_tma));
    buffer_write(buf, &m_tac, sizeof(m_tac));
    buffer_write(buf, &m_timerCounter, sizeof(m_timerCounter));
    buffer_write(buf, &m_timerOverflow, sizeof(m_timerOverflow));
    buffer_write(buf, &m_overflowDelay, sizeof(m_overflowDelay));
}

void Timer::loadState(Buffer* buf) {
    buffer_read(buf, &m_dividerCounter, sizeof(m_dividerCounter));
    buffer_read(buf, &m_div, sizeof(m_div));
    buffer_read(buf, &m_tima, sizeof(m_tima));
    buffer_read(buf, &m_tma, sizeof(m_tma));
    buffer_read(buf, &m_tac, sizeof(m_tac));
    buffer_read(buf, &m_timerCounter, sizeof(m_timerCounter));
    buffer_read(buf, &m_timerOverflow, sizeof(m_timerOverflow));
    buffer_read(buf, &m_overflowDelay, sizeof(m_overflowDelay));
}

} // namespace gb


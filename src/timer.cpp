#include "timer.h"
#include "cpu.h"

Timer::Timer()
    : m_cpu(nullptr)
    , m_dividerCounter(0)
    , m_timerCounter(0)
    , m_div(0)
    , m_tima(0)
    , m_tma(0)
    , m_tac(0) {
}

Timer::~Timer() {
}

void Timer::setCPU(CPU* cpu) {
    m_cpu = cpu;
}

void Timer::reset() {
    m_dividerCounter = 0;
    m_timerCounter = 0;
    m_div = 0;
    m_tima = 0;
    m_tma = 0;
    m_tac = 0;
}

void Timer::step(u32 cycles) {
    updateDivider(cycles);
    updateTimer(cycles);
}

void Timer::updateDivider(u32 cycles) {
    m_dividerCounter += cycles;
    
    // DIV increments at 16384 Hz (every 256 cycles)
    while (m_dividerCounter >= 256) {
        m_dividerCounter -= 256;
        m_div++;
    }
}

void Timer::updateTimer(u32 cycles) {
    // Check if timer is enabled
    if (!(m_tac & 0x04)) {
        return;
    }
    
    m_timerCounter += cycles;
    
    // Determine timer frequency
    u16 threshold = 1024; // 4096 Hz (default)
    switch (m_tac & 0x03) {
        case 0: threshold = 1024; break;  // 4096 Hz
        case 1: threshold = 16; break;    // 262144 Hz
        case 2: threshold = 64; break;    // 65536 Hz
        case 3: threshold = 256; break;   // 16384 Hz
    }
    
    while (m_timerCounter >= threshold) {
        m_timerCounter -= threshold;
        m_tima++;
        
        // Check for overflow
        if (m_tima == 0) {
            m_tima = m_tma;
            if (m_cpu) {
                m_cpu->requestInterrupt(INT_TIMER);
            }
        }
    }
}

u8 Timer::read(u16 address) const {
    switch (address) {
        case 0xFF04: return m_div;
        case 0xFF05: return m_tima;
        case 0xFF06: return m_tma;
        case 0xFF07: return m_tac | 0xF8; // Upper 5 bits are always 1
        default: return 0xFF;
    }
}

void Timer::write(u16 address, u8 value) {
    switch (address) {
        case 0xFF04:
            m_div = 0;
            m_dividerCounter = 0;
            break;
        case 0xFF05:
            m_tima = value;
            break;
        case 0xFF06:
            m_tma = value;
            break;
        case 0xFF07:
            m_tac = value & 0x07;
            break;
    }
}


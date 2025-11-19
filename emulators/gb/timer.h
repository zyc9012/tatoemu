#pragma once

#include "types.h"
#include <fstream>

namespace gb {

class CPU;

class Timer {
public:
    Timer();
    ~Timer();

    void setCPU(CPU* cpu);
    void reset();
    void step(u32 cycles);
    
    u8 read(u16 address) const;
    void write(u16 address, u8 value);
    
    // Save/Load state
    void saveState(std::ofstream& file) const;
    void loadState(std::ifstream& file);

private:
    void updateDivider(u32 cycles);
    void updateTimer(u32 cycles);
    u32 getTimerFrequency() const;

    CPU* m_cpu;
    
    // Timer registers
    u16 m_dividerCounter;  // Internal counter for DIV register
    u8 m_div;              // DIV register (0xFF04) - Divider Register
    u8 m_tima;             // TIMA register (0xFF05) - Timer Counter
    u8 m_tma;              // TMA register (0xFF06) - Timer Modulo
    u8 m_tac;              // TAC register (0xFF07) - Timer Control
    
    // Internal state
    u32 m_timerCounter;    // Internal counter for TIMA
    bool m_timerOverflow;  // Track if overflow just occurred
    u8 m_overflowDelay;    // Delay cycles before interrupt fires
};

} // namespace gb


#pragma once

#include "types.h"
#include <fstream>

class CPU;

class Timer {
public:
    Timer();
    ~Timer();

    void setCPU(CPU* cpu);
    void step(u32 cycles);
    void reset();

    u8 read(u16 address) const;
    void write(u16 address, u8 value);
    
    // Save/Load state
    void saveState(std::ofstream& file) const;
    void loadState(std::ifstream& file);

private:
    void updateDivider(u32 cycles);
    void updateTimer(u32 cycles);

    CPU* m_cpu;
    
    u16 m_dividerCounter;
    u16 m_timerCounter;
    u8 m_div;   // Divider Register (0xFF04)
    u8 m_tima;  // Timer Counter (0xFF05)
    u8 m_tma;   // Timer Modulo (0xFF06)
    u8 m_tac;   // Timer Control (0xFF07)
};


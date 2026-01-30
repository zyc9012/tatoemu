#pragma once

#include "../../types.h"
#include "../../components/buffer.h"
#include <functional>

namespace neogeo {

// NEC uPD4990A Real-Time Clock/Calendar chip
// Used in NeoGeo MVS systems for timekeeping
class UPD4990A {
public:
    UPD4990A();
    ~UPD4990A() = default;

    // Initialization
    void initialize(u32 ticksPerSecond, std::function<u32()> totalCyclesCallback);

    // Reset
    void reset();

    // Update (call once per frame)
    void update();

    // I/O interface
    void write(u8 clk, u8 stb, u8 data);
    u8 read();

    // State save/load
    void saveState(Buffer* buf);
    void loadState(Buffer* buf);

private:
    // Current time
    u32 m_seconds;
    u32 m_minutes;
    u32 m_hours;
    u32 m_day;
    u32 m_month;
    u32 m_year;
    u32 m_weekDay;

    // Modes for both outputs
    int m_mode;
    int m_tpMode;

    // Shift register and command
    u32 m_register[2];
    u32 m_command;

    // Counters
    u32 m_count;
    u32 m_tpCount;
    u32 m_interval;

    // Outputs
    u8 m_tp;
    u8 m_prevCLK;
    u8 m_prevSTB;

    // Timing
    u32 m_ticksPerSecond;
    u32 m_oneSecond;
    std::function<int()> m_totalCyclesCallback;
    u32 m_lastCycleCount;
};

} // namespace neogeo
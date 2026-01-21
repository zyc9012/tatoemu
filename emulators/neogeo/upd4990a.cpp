#include "upd4990a.h"
#include "../../types.h"
#include <ctime>
#include <iostream>
#include <fstream>

namespace neogeo {

UPD4990A::UPD4990A()
    : m_seconds(0), m_minutes(0), m_hours(0), m_day(0), m_month(0), m_year(0), m_weekDay(0)
    , m_mode(0), m_tpMode(0)
    , m_command(0)
    , m_count(0), m_tpCount(0), m_interval(0)
    , m_tp(0), m_prevCLK(0), m_prevSTB(0)
    , m_ticksPerSecond(0), m_oneSecond(0), m_lastCycleCount(0) {
}

void UPD4990A::initialize(u32 ticksPerSecond, std::function<u32()> totalCyclesCallback) {
    m_ticksPerSecond = ticksPerSecond;
    m_oneSecond = ticksPerSecond;
    m_totalCyclesCallback = totalCyclesCallback;

    reset();

    // Set the time to the current local time
    std::time_t now = std::time(nullptr);
    std::tm* tmLocalTime = std::localtime(&now);

    m_seconds = tmLocalTime->tm_sec;
    m_minutes = tmLocalTime->tm_min;
    m_hours = tmLocalTime->tm_hour;
    m_day = tmLocalTime->tm_mday;
    m_weekDay = tmLocalTime->tm_wday;
    m_month = tmLocalTime->tm_mon + 1;  // tm_mon is 0-based
    m_year = tmLocalTime->tm_year % 100;  // Get last 2 digits of year
}

void UPD4990A::reset() {
    m_register[0] = m_register[1] = 0;
    m_command = 0;
    m_mode = m_tpMode = 0;
    m_count = m_tpCount = 0;
    m_interval = m_oneSecond / 64;
    m_prevCLK = m_prevSTB = 0;
    m_tp = 0;
}

void UPD4990A::update() {
    // Calculate elapsed cycles since last update
    int currentCycles = m_totalCyclesCallback();
    u32 elapsedCycles = currentCycles - m_lastCycleCount;
    m_lastCycleCount = currentCycles;

    if (m_tpMode != 2) {
        m_tpCount += elapsedCycles;

        if (m_tpMode == 1) {
            if (m_tpCount >= m_interval) {
                m_tpMode = 0;
                m_tpCount %= m_interval;
                m_tp = (m_tpCount >= (m_interval >> 1));
            }
        } else {
            if (m_tpCount >= m_interval) {
                m_tpCount %= m_interval;
            }
            m_tp = (m_tpCount >= (m_interval >> 1));
        }
    }

    m_count += elapsedCycles;
    if (m_count >= m_oneSecond) {
        m_count %= m_oneSecond;

        m_seconds++;
        if (m_seconds >= 60) {
            m_seconds = 0;
            m_minutes++;
            if (m_minutes >= 60) {
                m_minutes = 0;
                m_hours++;
                if (m_hours >= 24) {
                    m_hours = 0;
                    m_weekDay++;
                    if (m_weekDay >= 7) {
                        m_weekDay = 0;
                    }

                    // Month lengths (not leap year aware for simplicity)
                    static const u32 monthLengths[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
                    bool isLeapYear = ((m_year % 4) == 0);
                    u32 daysInMonth = monthLengths[m_month - 1];
                    if (m_month == 2 && isLeapYear) {
                        daysInMonth = 29;
                    }

                    m_day++;
                    if (m_day > daysInMonth) {
                        m_day = 1;
                        m_month++;
                        if (m_month > 12) {
                            m_month = 1;
                            m_year++;
                            if (m_year >= 100) {
                                m_year = 0;
                            }
                        }
                    }
                }
            }
        }
    }
}

void UPD4990A::write(u8 clk, u8 stb, u8 data) {
    update();

    if (stb && m_prevSTB == 0) {
        // Process command
        switch (m_command & 0x0F) {
            case 0x00:  // Register hold
                m_mode = 0;
                m_tpMode = 0;
                m_interval = m_oneSecond / 64;
                m_tpCount %= m_interval;
                break;
            case 0x01:  // Register shift
                m_mode = 1;
                break;
            case 0x02:  // Time set & counter hold
                m_mode = 2;

                // Convert BCD values to normal numbers
                m_seconds = ((m_register[0] >> 0) & 0x0F);
                m_seconds += ((m_register[0] >> 4) & 0x0F) * 10;
                m_minutes = ((m_register[0] >> 8) & 0x0F);
                m_minutes += ((m_register[0] >> 12) & 0x0F) * 10;
                m_hours = ((m_register[0] >> 16) & 0x0F);
                m_hours += ((m_register[0] >> 20) & 0x0F) * 10;
                m_day = ((m_register[0] >> 24) & 0x0F);
                m_day += ((m_register[0] >> 28) & 0x0F) * 10;
                m_weekDay = ((m_register[1] >> 0) & 0x0F);
                m_month = ((m_register[1] >> 4) & 0x0F);
                m_year = ((m_register[1] >> 8) & 0x0F);
                m_year += ((m_register[1] >> 12) & 0x0F) * 10;
                break;
            case 0x03:  // Time read
                m_mode = 0;

                // Convert normal numbers to BCD values
                m_register[0] = (m_seconds % 10) << 0;
                m_register[0] |= (m_seconds / 10) << 4;
                m_register[0] |= (m_minutes % 10) << 8;
                m_register[0] |= (m_minutes / 10) << 12;
                m_register[0] |= (m_hours % 10) << 16;
                m_register[0] |= (m_hours / 10) << 20;
                m_register[0] |= (m_day % 10) << 24;
                m_register[0] |= (m_day / 10) << 28;
                m_register[1] = m_weekDay << 0;
                m_register[1] |= m_month << 4;
                m_register[1] |= (m_year % 10) << 8;
                m_register[1] |= (m_year / 10) << 12;
                break;
            case 0x04:
            case 0x05:
            case 0x06:
            case 0x07: {  // TP = nn Hz
                static const int frequencies[4] = {64, 256, 2048, 4096};
                m_tpMode = 0;
                m_interval = m_oneSecond / frequencies[m_command & 3];
                m_tpCount %= m_interval;
                break;
            }
            case 0x08:
            case 0x09:
            case 0x0A:
            case 0x0B: {  // TP = nn s interval set (counter reset & start)
                static const int intervals[4] = {1, 10, 30, 60};
                m_tpMode = 0;
                m_interval = intervals[m_command & 3] * m_oneSecond;
                m_tpCount = 0;
                break;
            }
            case 0x0C:  // Interval reset
                m_tpMode = 1;
                m_tp = 1;
                break;
            case 0x0D:  // Interval start
                m_tpMode = 0;
                break;
            case 0x0E:  // Interval stop
                m_tpMode = 2;
                break;
            case 0x0F:  // Test mode set (not implemented)
                break;
        }
    }

    if (stb == 0 && clk && m_prevCLK == 0) {
        // Shift a new bit into the register
        if (m_mode == 1) {
            m_register[0] >>= 1;
            if (m_register[1] & 1) {
                m_register[0] |= (1 << 31);
            }
            m_register[1] >>= 1;
            m_register[1] &= 0x7FFF;
            if (m_command & 1) {
                m_register[1] |= (1 << 15);
            }
        }

        // Shift a new bit into the command
        m_command >>= 1;
        m_command &= 7;
        if (data) {
            m_command |= 8;
        }
    }

    m_prevCLK = clk;
    m_prevSTB = stb;
}

u8 UPD4990A::read() {
    update();

    u8 out;
    if (m_mode == 0) {
        // 1Hz pulse appears at output
        out = (m_count >= (m_oneSecond >> 1));
    } else {
        // LSB of the shift register appears at output
        out = m_register[0] & 1;
    }

    return (out << 1) | m_tp;
}

void UPD4990A::saveState(std::ofstream& file) {
    file.write(reinterpret_cast<const char*>(&m_seconds), sizeof(m_seconds));
    file.write(reinterpret_cast<const char*>(&m_minutes), sizeof(m_minutes));
    file.write(reinterpret_cast<const char*>(&m_hours), sizeof(m_hours));
    file.write(reinterpret_cast<const char*>(&m_day), sizeof(m_day));
    file.write(reinterpret_cast<const char*>(&m_month), sizeof(m_month));
    file.write(reinterpret_cast<const char*>(&m_year), sizeof(m_year));
    file.write(reinterpret_cast<const char*>(&m_weekDay), sizeof(m_weekDay));

    file.write(reinterpret_cast<const char*>(&m_mode), sizeof(m_mode));
    file.write(reinterpret_cast<const char*>(&m_tpMode), sizeof(m_tpMode));

    file.write(reinterpret_cast<const char*>(&m_register), sizeof(m_register));
    file.write(reinterpret_cast<const char*>(&m_command), sizeof(m_command));

    file.write(reinterpret_cast<const char*>(&m_count), sizeof(m_count));
    file.write(reinterpret_cast<const char*>(&m_tpCount), sizeof(m_tpCount));
    file.write(reinterpret_cast<const char*>(&m_interval), sizeof(m_interval));

    file.write(reinterpret_cast<const char*>(&m_tp), sizeof(m_tp));
    file.write(reinterpret_cast<const char*>(&m_prevCLK), sizeof(m_prevCLK));
    file.write(reinterpret_cast<const char*>(&m_prevSTB), sizeof(m_prevSTB));
}

void UPD4990A::loadState(std::ifstream& file) {
    file.read(reinterpret_cast<char*>(&m_seconds), sizeof(m_seconds));
    file.read(reinterpret_cast<char*>(&m_minutes), sizeof(m_minutes));
    file.read(reinterpret_cast<char*>(&m_hours), sizeof(m_hours));
    file.read(reinterpret_cast<char*>(&m_day), sizeof(m_day));
    file.read(reinterpret_cast<char*>(&m_month), sizeof(m_month));
    file.read(reinterpret_cast<char*>(&m_year), sizeof(m_year));
    file.read(reinterpret_cast<char*>(&m_weekDay), sizeof(m_weekDay));

    file.read(reinterpret_cast<char*>(&m_mode), sizeof(m_mode));
    file.read(reinterpret_cast<char*>(&m_tpMode), sizeof(m_tpMode));

    file.read(reinterpret_cast<char*>(&m_register), sizeof(m_register));
    file.read(reinterpret_cast<char*>(&m_command), sizeof(m_command));

    file.read(reinterpret_cast<char*>(&m_count), sizeof(m_count));
    file.read(reinterpret_cast<char*>(&m_tpCount), sizeof(m_tpCount));
    file.read(reinterpret_cast<char*>(&m_interval), sizeof(m_interval));

    file.read(reinterpret_cast<char*>(&m_tp), sizeof(m_tp));
    file.read(reinterpret_cast<char*>(&m_prevCLK), sizeof(m_prevCLK));
    file.read(reinterpret_cast<char*>(&m_prevSTB), sizeof(m_prevSTB));
}

} // namespace neogeo
#pragma once

#include "types.h"
#include "../components/buffer.h"
#include <ctime>

namespace gba {

// GPIO register offsets within ROM (byte addresses relative to 0x08000000)
namespace GPIOReg {
    constexpr u32 DATA      = 0xC4;  // Pin data (low 4 bits)
    constexpr u32 DIRECTION = 0xC6;  // Pin direction (1=output from GBA, 0=input)
    constexpr u32 CONTROL   = 0xC8;  // Readable flag (0=write-only, 1=readable)
}

// RTC pin assignments within GPIO
namespace RTC_PIN {
    constexpr u8 SCK = 0;  // Pin 0: Serial Clock
    constexpr u8 SIO = 1;  // Pin 1: Serial I/O (bidirectional data)
    constexpr u8 CS  = 2;  // Pin 2: Chip Select
}

// RTC commands (3-bit command field)
enum class RTCCommand : u8 {
    RESET     = 0,  // Force reset (0 data bytes)
    DATETIME  = 2,  // Date/Time (7 data bytes)
    FORCE_IRQ = 3,  // Force IRQ (0 data bytes)
    CONTROL   = 4,  // Control register (1 data byte)
    TIME      = 6,  // Time only (3 data bytes)
};

// Number of data bytes per RTC command
static constexpr int RTC_BYTES[8] = { 0, 0, 7, 0, 1, 0, 3, 0 };

struct RTCState {
    int bytesRemaining = 0;
    int bitsRead = 0;
    int bits = 0;
    bool commandActive = false;
    bool reading = false;
    u8 command = 0;       // 3-bit command code
    bool sckEdge = true;  // Previous SCK state (starts "high")
    bool sioOutput = true;

    // RTC control register
    // Bit 3: MinIRQ, Bit 6: Hour24, Bit 7: Poweroff
    u8 control = 0x40;   // Default: 24-hour mode

    // Latched time data (max 7 bytes for datetime)
    u8 time[7] = {};
};

class GPIO {
public:
    GPIO();
    ~GPIO() = default;

    void reset();

    // Set pointer to ROM data for direct-mapped GPIO reads
    void setROM(u8* rom, u32 romSize);

    // Check if a ROM offset is a GPIO register
    static bool isGPIOAddress(u32 offset) {
        return offset == GPIOReg::DATA || offset == GPIOReg::DIRECTION || offset == GPIOReg::CONTROL;
    }

    // Called when CPU writes to GPIO-mapped ROM addresses
    void write(u32 offset, u16 value);

    // Save/Load state
    void saveState(Buffer* buf);
    void loadState(Buffer* buf);

private:
    void updatePinState();
    void outputPins(u8 pins);
    void storeToROM();

    // RTC protocol
    void rtcReadPins();
    void rtcBeginCommand();
    void rtcProcessByte();
    u8 rtcOutput();
    void rtcUpdateClock();
    static u8 toBCD(int value);

    u8* m_rom = nullptr;
    u32 m_romSize = 0;

    // GPIO state
    u8 m_pinState = 0;     // Current pin values (4 bits)
    u8 m_direction = 0;    // Direction register (1=output from GBA)
    u8 m_writeLatch = 0;   // Last written data value
    bool m_readable = false; // Control register: GPIO readable?

    // RTC state
    RTCState m_rtc;
};

} // namespace gba

#pragma once

#include "../../types.h"
#include "../buffer.h"
#include <cstring>
#include <string>

// EEPROM Interface Configuration
struct EEPROMInterface
{
    u32 address_bits;           // EEPROM has 2^address_bits cells
    u32 data_bits;              // Every cell has this many bits (8 or 16)
    const char* cmd_read;       // Read command string, e.g. "*110"
    const char* cmd_write;      // Write command string, e.g. "*101"
    const char* cmd_erase;      // Erase command string, or nullptr if n/a
    const char* cmd_lock;       // Lock command string, or nullptr if n/a
    const char* cmd_unlock;     // Unlock command string, or nullptr if n/a
    u32 enable_multi_read;      // Enable multiple values to be read from one read command
    u32 reset_delay;            // Number of times read should return 0 after reset before returning 1
};

// EEPROM Emulator Class
// Handles serial EEPROM communication protocol
class EEPROM
{
public:
    // Line state definitions
    static constexpr u32 CLEAR_LINE = 0;
    static constexpr u32 ASSERT_LINE = 1;
    static constexpr u32 PULSE_LINE = 2;

    EEPROM();
    ~EEPROM() = default;

    // Initialize with interface configuration
    void init(const EEPROMInterface* interface);

    // Reset the EEPROM state
    void reset();

    // Serial interface functions
    void writeBit(u32 bit);
    void setCSLine(u32 state);
    void setClockLine(u32 state);
    u32 read();

    // Convenience function for combined write
    void write(u32 clock, u32 cs, u32 bit);

    // Direct data access
    void fillData(const u8* data, u32 offset, u32 length);
    void fillByte(u8 byte, u32 length);
    u8 readByte(u32 offset);
    void writeByte(u32 offset, u8 data);

    // Save/Load state
    void saveState(Buffer* buf);
    void loadState(Buffer* buf);

    // Check if EEPROM file is available (loaded)
    bool isAvailable() const { return m_available; }

private:
    template <typename Visit> void visitState(Visit visit);

    static constexpr u32 SERIAL_BUFFER_LENGTH = 40;
    static constexpr u32 MEMORY_SIZE = 1024;

    const EEPROMInterface* m_interface;

    u8 m_serialBuffer[SERIAL_BUFFER_LENGTH];
    u8 m_eepromData[MEMORY_SIZE];

    u32 m_serialCount;
    u32 m_eepromDataBits;
    u32 m_eepromReadAddress;
    u32 m_eepromClockCount;

    u32 m_latch;
    u32 m_resetLine;
    u32 m_clockLine;
    bool m_sending;
    bool m_locked;
    u32 m_resetDelay;

    bool m_initialized;
    bool m_available;

    // Internal helper functions
    bool commandMatch(const char* buf, const char* cmd, u32 len);
    void processWrite(u32 bit);
};

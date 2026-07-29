#pragma once

#include "../compact.h"
#include "../buffer.h"
#include <cstring>
#include <string>

// EEPROM Interface Configuration
struct EEPROMInterface
{
    UINT32 address_bits;        // EEPROM has 2^address_bits cells
    UINT32 data_bits;           // Every cell has this many bits (8 or 16)
    const char* cmd_read;       // Read command string, e.g. "*110"
    const char* cmd_write;      // Write command string, e.g. "*101"
    const char* cmd_erase;      // Erase command string, or nullptr if n/a
    const char* cmd_lock;       // Lock command string, or nullptr if n/a
    const char* cmd_unlock;     // Unlock command string, or nullptr if n/a
    UINT32 enable_multi_read;   // Enable multiple values to be read from one read command
    UINT32 reset_delay;         // Number of times read should return 0 after reset before returning 1
};

// EEPROM Emulator Class
// Handles serial EEPROM communication protocol
class EEPROM
{
public:
    // Line state definitions
    static constexpr UINT32 CLEAR_LINE = 0;
    static constexpr UINT32 ASSERT_LINE = 1;
    static constexpr UINT32 PULSE_LINE = 2;

    EEPROM();
    ~EEPROM() = default;

    // Initialize with interface configuration
    void init(const EEPROMInterface* interface);

    // Reset the EEPROM state
    void reset();

    // Serial interface functions
    void writeBit(UINT32 bit);
    void setCSLine(UINT32 state);
    void setClockLine(UINT32 state);
    UINT32 read();

    // Convenience function for combined write
    void write(UINT32 clock, UINT32 cs, UINT32 bit);

    // Direct data access
    void fillData(const UINT8* data, UINT32 offset, UINT32 length);
    void fillByte(UINT8 byte, UINT32 length);
    UINT8 readByte(UINT32 offset);
    void writeByte(UINT32 offset, UINT8 data);

    // Save/Load state
    void saveState(Buffer* buf);
    void loadState(Buffer* buf);

    // Check if EEPROM file is available (loaded)
    bool isAvailable() const { return m_available; }

private:
    template <typename Visit> void visitState(Visit visit);

    static constexpr UINT32 SERIAL_BUFFER_LENGTH = 40;
    static constexpr UINT32 MEMORY_SIZE = 1024;

    const EEPROMInterface* m_interface;

    UINT8 m_serialBuffer[SERIAL_BUFFER_LENGTH];
    UINT8 m_eepromData[MEMORY_SIZE];

    UINT32 m_serialCount;
    UINT32 m_eepromDataBits;
    UINT32 m_eepromReadAddress;
    UINT32 m_eepromClockCount;

    UINT32 m_latch;
    UINT32 m_resetLine;
    UINT32 m_clockLine;
    bool m_sending;
    bool m_locked;
    UINT32 m_resetDelay;

    bool m_initialized;
    bool m_available;

    // Internal helper functions
    bool commandMatch(const char* buf, const char* cmd, UINT32 len);
    void processWrite(UINT32 bit);
};

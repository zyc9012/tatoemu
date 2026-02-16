#include "gpio.h"
#include <cstring>
#include <ctime>

namespace gba {

GPIO::GPIO() {
    reset();
}

void GPIO::reset() {
    m_pinState = 0;
    m_direction = 0;
    m_writeLatch = 0;
    m_readable = false;

    m_rtc = RTCState{};
    m_rtc.control = 0x40; // 24-hour mode
    m_rtc.sckEdge = true;
    m_rtc.sioOutput = true;
}

void GPIO::setROM(u8* rom, u32 romSize) {
    m_rom = rom;
    m_romSize = romSize;
}

void GPIO::write(u32 offset, u16 value) {
    switch (offset) {
        case GPIOReg::DATA:
            m_writeLatch = value & 0xF;
            updatePinState();
            rtcReadPins();
            break;
        case GPIOReg::DIRECTION:
            m_direction = value & 0xF;
            updatePinState();
            rtcReadPins();
            break;
        case GPIOReg::CONTROL:
            m_readable = (value & 1) != 0;
            break;
    }

    storeToROM();
}

void GPIO::updatePinState() {
    // Output bits come from writeLatch, input bits preserved from previous state
    m_pinState = (m_writeLatch & m_direction) | (m_pinState & ~m_direction);
    m_pinState &= 0xF;
}

void GPIO::outputPins(u8 pins) {
    // Device can only drive pins where direction=0 (input from GBA = output from device)
    m_pinState &= m_direction;                    // Keep GBA output bits
    m_pinState |= (pins & ~m_direction & 0xF);    // Set device input bits
    storeToROM();
}

void GPIO::storeToROM() {
    if (!m_rom || m_romSize < GPIOReg::CONTROL + 2) return;

    if (m_readable) {
        *reinterpret_cast<u16*>(&m_rom[GPIOReg::DATA]) = m_pinState;
        *reinterpret_cast<u16*>(&m_rom[GPIOReg::DIRECTION]) = m_direction;
        *reinterpret_cast<u16*>(&m_rom[GPIOReg::CONTROL]) = m_readable ? 1 : 0;
    } else {
        // When not readable, zero the GPIO addresses so ROM reads return original values
        // (original ROM content at these addresses is typically 0)
        *reinterpret_cast<u16*>(&m_rom[GPIOReg::DATA]) = 0;
        *reinterpret_cast<u16*>(&m_rom[GPIOReg::DIRECTION]) = 0;
        *reinterpret_cast<u16*>(&m_rom[GPIOReg::CONTROL]) = 0;
    }
}

// ----- RTC Protocol -----

void GPIO::rtcReadPins() {
    // RTC drives only SIO (pin 1); other pins are held low
    u8 output = m_pinState & (1 << RTC_PIN::SIO);

    bool csHigh = (m_pinState >> RTC_PIN::CS) & 1;
    bool sck = (m_pinState >> RTC_PIN::SCK) & 1;
    bool sio = (m_pinState >> RTC_PIN::SIO) & 1;

    if (!csHigh) {
        // CS low: reset/idle
        m_rtc.bitsRead = 0;
        m_rtc.bytesRemaining = 0;
        m_rtc.commandActive = false;
        m_rtc.command = 0;
        m_rtc.sckEdge = true; // SCK considered high when CS is low
        m_rtc.sioOutput = true;
        output = (1 << RTC_PIN::SIO); // SIO held high
    } else if (!m_rtc.commandActive) {
        // Command phase: clocking in the 8-bit command byte
        if (!sck) {
            // SCK low: latch SIO bit
            m_rtc.bits &= ~(1 << m_rtc.bitsRead);
            m_rtc.bits |= (sio ? 1 : 0) << m_rtc.bitsRead;
        } else if (!m_rtc.sckEdge) {
            // SCK rising edge: advance bit counter
            m_rtc.bitsRead++;
            if (m_rtc.bitsRead == 8) {
                rtcBeginCommand();
            }
        }
    } else if (!m_rtc.reading) {
        // Data phase - Writing to RTC
        if (!sck) {
            // SCK low: latch SIO bit
            m_rtc.bits &= ~(1 << m_rtc.bitsRead);
            m_rtc.bits |= (sio ? 1 : 0) << m_rtc.bitsRead;
        } else if (!m_rtc.sckEdge) {
            // SCK rising edge: advance bit counter
            m_rtc.bitsRead++;
            if (m_rtc.bitsRead == 8) {
                rtcProcessByte();
            }
        }
    } else {
        // Data phase - Reading from RTC
        if (m_rtc.sckEdge && !sck) {
            // SCK falling edge: output next bit
            m_rtc.sioOutput = (rtcOutput() >> m_rtc.bitsRead) & 1;
            m_rtc.bitsRead++;
            if (m_rtc.bitsRead == 8) {
                m_rtc.bytesRemaining--;
                m_rtc.bitsRead = 0;
                if (m_rtc.bytesRemaining <= 0) {
                    m_rtc.commandActive = false;
                }
            }
        }
        output = m_rtc.sioOutput ? (1 << RTC_PIN::SIO) : 0;
    }

    m_rtc.sckEdge = sck;
    outputPins(output);
}

void GPIO::rtcBeginCommand() {
    u8 cmdByte = m_rtc.bits & 0xFF;
    u8 magic = cmdByte & 0xF;

    if (magic != 0x6) {
        // Invalid command magic — ignore
        m_rtc.bitsRead = 0;
        m_rtc.bits = 0;
        return;
    }

    m_rtc.command = (cmdByte >> 4) & 0x7;
    m_rtc.reading = (cmdByte >> 7) & 1;
    m_rtc.bytesRemaining = RTC_BYTES[m_rtc.command];
    m_rtc.commandActive = true;
    m_rtc.bitsRead = 0;
    m_rtc.bits = 0;

    switch (static_cast<RTCCommand>(m_rtc.command)) {
        case RTCCommand::RESET:
            m_rtc.control = 0;
            m_rtc.commandActive = false;
            break;
        case RTCCommand::DATETIME:
        case RTCCommand::TIME:
            rtcUpdateClock();
            break;
        case RTCCommand::FORCE_IRQ:
            m_rtc.commandActive = false;
            break;
        case RTCCommand::CONTROL:
        default:
            break;
    }

    // If no data bytes, command is complete
    if (m_rtc.bytesRemaining == 0) {
        m_rtc.commandActive = false;
    }
}

void GPIO::rtcProcessByte() {
    u8 byte = m_rtc.bits & 0xFF;

    switch (static_cast<RTCCommand>(m_rtc.command)) {
        case RTCCommand::CONTROL:
            m_rtc.control = byte;
            break;
        default:
            break;
    }

    m_rtc.bytesRemaining--;
    m_rtc.bitsRead = 0;
    m_rtc.bits = 0;

    if (m_rtc.bytesRemaining <= 0) {
        m_rtc.commandActive = false;
    }
}

u8 GPIO::rtcOutput() {
    u8 outputByte = 0xFF;
    int byteIndex;

    switch (static_cast<RTCCommand>(m_rtc.command)) {
        case RTCCommand::CONTROL:
            outputByte = m_rtc.control;
            break;
        case RTCCommand::DATETIME:
            byteIndex = 7 - m_rtc.bytesRemaining;
            if (byteIndex >= 0 && byteIndex < 7) {
                outputByte = m_rtc.time[byteIndex];
            }
            break;
        case RTCCommand::TIME:
            byteIndex = 3 - m_rtc.bytesRemaining;
            if (byteIndex >= 0 && byteIndex < 3) {
                // TIME returns Hour, Min, Sec (indices 4,5,6 of datetime)
                outputByte = m_rtc.time[4 + byteIndex];
            }
            break;
        default:
            break;
    }

    return outputByte;
}

u8 GPIO::toBCD(int value) {
    return static_cast<u8>((value % 10) | ((value / 10) << 4));
}

void GPIO::rtcUpdateClock() {
    time_t t = std::time(nullptr);
    std::tm* date = std::localtime(&t);

    int year = date->tm_year - 100; // Years since 2000
    if (year < 0) year = 0;
    if (year > 99) year = 99;

    int hour = date->tm_hour;
    bool hour24 = (m_rtc.control & 0x40) != 0;
    if (!hour24) {
        hour = hour % 12;
    }

    m_rtc.time[0] = toBCD(year);             // Year (00-99)
    m_rtc.time[1] = toBCD(date->tm_mon + 1); // Month (1-12)
    m_rtc.time[2] = toBCD(date->tm_mday);    // Day (1-31)
    m_rtc.time[3] = toBCD(date->tm_wday);    // Day of week (0=Sun)
    m_rtc.time[4] = toBCD(hour);             // Hour
    m_rtc.time[5] = toBCD(date->tm_min);     // Minute
    m_rtc.time[6] = toBCD(date->tm_sec);     // Second
}

void GPIO::saveState(Buffer* buf) {
    buffer_write(buf, &m_pinState, sizeof(m_pinState));
    buffer_write(buf, &m_direction, sizeof(m_direction));
    buffer_write(buf, &m_writeLatch, sizeof(m_writeLatch));
    buffer_write(buf, &m_readable, sizeof(m_readable));
    buffer_write(buf, &m_rtc, sizeof(m_rtc));
}

void GPIO::loadState(Buffer* buf) {
    buffer_read(buf, &m_pinState, sizeof(m_pinState));
    buffer_read(buf, &m_direction, sizeof(m_direction));
    buffer_read(buf, &m_writeLatch, sizeof(m_writeLatch));
    buffer_read(buf, &m_readable, sizeof(m_readable));
    buffer_read(buf, &m_rtc, sizeof(m_rtc));

    // Restore ROM-mapped values
    storeToROM();
}

} // namespace gba

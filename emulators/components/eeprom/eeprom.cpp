// Based on MAME/FBNeo EEPROM implementation
#include "eeprom.h"
#include "../../types.h"

EEPROM::EEPROM()
    : m_interface(nullptr)
    , m_serialCount(0)
    , m_eepromDataBits(0)
    , m_eepromReadAddress(0)
    , m_eepromClockCount(0)
    , m_latch(0)
    , m_resetLine(ASSERT_LINE)
    , m_clockLine(ASSERT_LINE)
    , m_sending(false)
    , m_locked(false)
    , m_resetDelay(0)
    , m_initialized(false)
    , m_available(false)
{
    std::memset(m_serialBuffer, 0, sizeof(m_serialBuffer));
    std::memset(m_eepromData, 0xFF, sizeof(m_eepromData));
}

void EEPROM::init(const EEPROMInterface* interface)
{
    m_interface = interface;

    if (!m_interface)
    {
        m_initialized = false;
        return;
    }

    if ((1 << m_interface->address_bits) * m_interface->data_bits / 8 > MEMORY_SIZE)
    {
        log_error("error: EEPROM larger than memory allows");
        m_initialized = false;
        return;
    }

    std::memset(m_eepromData, 0xFF, (1 << m_interface->address_bits) * m_interface->data_bits / 8);
    m_serialCount = 0;
    m_latch = 0;
    m_resetLine = ASSERT_LINE;
    m_clockLine = ASSERT_LINE;
    m_eepromReadAddress = 0;
    m_sending = false;
    m_locked = (m_interface->cmd_unlock != nullptr);

    m_initialized = true;
    m_available = false; // File loading not implemented in this version
}

void EEPROM::reset()
{
    if (!m_initialized) return;

    m_serialCount = 0;
    m_sending = false;
    m_resetDelay = m_interface->reset_delay;
}

bool EEPROM::commandMatch(const char* buf, const char* cmd, UINT32 len)
{
    if (cmd == nullptr) return false;
    if (len == 0) return false;

    for (; len > 0;)
    {
        char b = *buf;
        char c = *cmd;

        if ((b == 0) || (c == 0))
            return (b == c);

        switch (c)
        {
            case '0':
            case '1':
                if (b != c) return false;
            case 'X':
            case 'x':
                buf++;
                len--;
                cmd++;
                break;

            case '*':
                c = cmd[1];
                switch (c)
                {
                    case '0':
                    case '1':
                        if (b == c) {
                            cmd++;
                        } else {
                            buf++;
                            len--;
                        }
                        break;
                    default:
                        return false;
                }
                break;
        }
    }
    return (*cmd == 0);
}

void EEPROM::processWrite(UINT32 bit)
{
    if (m_serialCount >= SERIAL_BUFFER_LENGTH - 1)
    {
        log_error("error: EEPROM serial buffer overflow");
        return;
    }

    m_serialBuffer[m_serialCount++] = (bit ? '1' : '0');
    m_serialBuffer[m_serialCount] = 0;

    // Check for read command
    if ((m_serialCount > m_interface->address_bits) &&
        commandMatch((char*)m_serialBuffer, m_interface->cmd_read,
                    std::strlen((char*)m_serialBuffer) - m_interface->address_bits))
    {
        UINT32 i, address = 0;

        for (i = m_serialCount - m_interface->address_bits; i < m_serialCount; i++)
        {
            address <<= 1;
            if (m_serialBuffer[i] == '1') address |= 1;
        }

        if (m_interface->data_bits == 16)
            m_eepromDataBits = (m_eepromData[2 * address + 0] << 8) + m_eepromData[2 * address + 1];
        else
            m_eepromDataBits = m_eepromData[address];

        m_eepromReadAddress = address;
        m_eepromClockCount = 0;
        m_sending = true;
        m_serialCount = 0;
    }
    // Check for erase command
    else if ((m_serialCount > m_interface->address_bits) &&
             commandMatch((char*)m_serialBuffer, m_interface->cmd_erase,
                         std::strlen((char*)m_serialBuffer) - m_interface->address_bits))
    {
        UINT32 i, address = 0;

        for (i = m_serialCount - m_interface->address_bits; i < m_serialCount; i++)
        {
            address <<= 1;
            if (m_serialBuffer[i] == '1') address |= 1;
        }

        if (!m_locked)
        {
            if (m_interface->data_bits == 16)
            {
                m_eepromData[2 * address + 0] = 0xFF;
                m_eepromData[2 * address + 1] = 0xFF;
            }
            else
                m_eepromData[address] = 0xFF;
        }
        m_serialCount = 0;
    }
    // Check for write command
    else if ((m_serialCount > (m_interface->address_bits + m_interface->data_bits)) &&
             commandMatch((char*)m_serialBuffer, m_interface->cmd_write,
                         std::strlen((char*)m_serialBuffer) - (m_interface->address_bits + m_interface->data_bits)))
    {
        UINT32 i, address = 0, data = 0;

        for (i = m_serialCount - m_interface->data_bits - m_interface->address_bits;
             i < (m_serialCount - m_interface->data_bits); i++)
        {
            address <<= 1;
            if (m_serialBuffer[i] == '1') address |= 1;
        }

        for (i = m_serialCount - m_interface->data_bits; i < m_serialCount; i++)
        {
            data <<= 1;
            if (m_serialBuffer[i] == '1') data |= 1;
        }

        if (!m_locked)
        {
            if (m_interface->data_bits == 16)
            {
                m_eepromData[2 * address + 0] = data >> 8;
                m_eepromData[2 * address + 1] = data & 0xFF;
            }
            else
                m_eepromData[address] = data;
        }
        m_serialCount = 0;
    }
    // Check for lock command
    else if (commandMatch((char*)m_serialBuffer, m_interface->cmd_lock,
                         std::strlen((char*)m_serialBuffer)))
    {
        m_locked = true;
        m_serialCount = 0;
    }
    // Check for unlock command
    else if (commandMatch((char*)m_serialBuffer, m_interface->cmd_unlock,
                         std::strlen((char*)m_serialBuffer)))
    {
        m_locked = false;
        m_serialCount = 0;
    }
}

void EEPROM::writeBit(UINT32 bit)
{
    if (!m_initialized) return;
    m_latch = bit;
}

void EEPROM::setCSLine(UINT32 state)
{
    if (!m_initialized) return;

    m_resetLine = state;

    if (m_resetLine != CLEAR_LINE)
        reset();
}

void EEPROM::setClockLine(UINT32 state)
{
    if (!m_initialized) return;

    if (state == PULSE_LINE || (m_clockLine == CLEAR_LINE && state != CLEAR_LINE))
    {
        if (m_resetLine == CLEAR_LINE)
        {
            if (m_sending)
            {
                if (m_eepromClockCount == m_interface->data_bits && m_interface->enable_multi_read)
                {
                    m_eepromReadAddress = (m_eepromReadAddress + 1) & ((1 << m_interface->address_bits) - 1);
                    if (m_interface->data_bits == 16)
                        m_eepromDataBits = (m_eepromData[2 * m_eepromReadAddress + 0] << 8) + m_eepromData[2 * m_eepromReadAddress + 1];
                    else
                        m_eepromDataBits = m_eepromData[m_eepromReadAddress];
                    m_eepromClockCount = 0;
                }
                m_eepromDataBits = (m_eepromDataBits << 1) | 1;
                m_eepromClockCount++;
            }
            else
            {
                processWrite(m_latch);
            }
        }
    }

    m_clockLine = state;
}

UINT32 EEPROM::read()
{
    if (!m_initialized) return 1;

    UINT32 res;

    if (m_sending)
    {
        res = (m_eepromDataBits >> m_interface->data_bits) & 1;
    }
    else
    {
        if (m_resetDelay > 0)
        {
            // This is needed by wbeachvl
            m_resetDelay--;
            res = 0;
        }
        else
        {
            res = 1;
        }
    }

    return res;
}

void EEPROM::write(UINT32 clock, UINT32 cs, UINT32 bit)
{
    writeBit(bit);
    setCSLine(cs ? CLEAR_LINE : ASSERT_LINE);
    setClockLine(clock ? ASSERT_LINE : CLEAR_LINE);
}

void EEPROM::fillData(const UINT8* data, UINT32 offset, UINT32 length)
{
    if (!m_initialized) return;
    std::memcpy(m_eepromData + offset, data, length);
}

void EEPROM::fillByte(UINT8 byte, UINT32 length)
{
    if (!m_initialized) return;
    std::memset(m_eepromData, byte, length);
}

UINT8 EEPROM::readByte(UINT32 offset)
{
    if (!m_initialized) return 0xFF;
    return m_eepromData[offset];
}

void EEPROM::writeByte(UINT32 offset, UINT8 data)
{
    if (!m_initialized) return;
    m_eepromData[offset] = data;
}

template <typename Visit>
void EEPROM::visitState(Visit visit)
{
    visit(m_eepromData);
    visit(m_serialBuffer);

    visit(m_serialCount);
    visit(m_eepromDataBits);
    visit(m_eepromReadAddress);
    visit(m_eepromClockCount);
    visit(m_latch);
    visit(m_resetLine);
    visit(m_clockLine);
    visit(m_sending);
    visit(m_locked);
    visit(m_resetDelay);
}

void EEPROM::saveState(Buffer* buf)
{
    if (!m_initialized) return;
    visitState(StateWriter{buf});
}

void EEPROM::loadState(Buffer* buf)
{
    if (!m_initialized) return;
    visitState(StateReader{buf});
}



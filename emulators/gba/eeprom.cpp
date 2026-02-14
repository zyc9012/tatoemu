#include "eeprom.h"
#include "../types.h"
#include <cstring>

namespace gba {

EEPROM::EEPROM() {
    reset();
}

EEPROM::~EEPROM() {}

void EEPROM::reset() {
    m_command = EEPROMCommand::NULL_CMD;
    m_writeAddress = 0;
    m_readAddress = 0;
    m_readBitsRemaining = 0;
}

void EEPROM::init(bool is8K) {
    m_is8K = is8K;
    u32 size = is8K ? EEPROM_8K_SIZE : EEPROM_512_SIZE;
    m_data.resize(size, 0xFF);
    m_initialized = true;
    reset();
}

void EEPROM::upgradeTo8K() {
    if (m_is8K || !m_initialized) return;
    
    // Expand from 512 bytes to 8KB
    std::vector<u8> newData(EEPROM_8K_SIZE, 0xFF);
    std::memcpy(newData.data(), m_data.data(), EEPROM_512_SIZE);
    m_data = std::move(newData);
    m_is8K = true;
}

u16 EEPROM::read() {
    if (m_command != EEPROMCommand::READ) {
        // Return 1 when ready, 0 when busy
        return 1;
    }
    
    --m_readBitsRemaining;
    if (m_readBitsRemaining < 64) {
        int step = 63 - m_readBitsRemaining;
        u32 address = (m_readAddress + step) >> 3;
        
        // Check bounds
        if (address >= m_data.size()) {
            return 0xFF;
        }
        
        u8 data = m_data[address] >> (7 - (step & 7));
        
        if (!m_readBitsRemaining) {
            m_command = EEPROMCommand::NULL_CMD;
        }
        
        return data & 1;
    }
    
    return 0;
}

void EEPROM::write(u16 value, u32 writeSize) {
    switch (m_command) {
    // Read header
    case EEPROMCommand::NULL_CMD:
    default:
        m_command = static_cast<EEPROMCommand>(value & 1);
        break;
        
    case EEPROMCommand::PENDING:
        m_command = static_cast<EEPROMCommand>((static_cast<int>(m_command) << 1) | (value & 1));
        if (m_command == EEPROMCommand::WRITE) {
            m_writeAddress = 0;
        } else if (m_command == EEPROMCommand::READ_PENDING) {
            m_readAddress = 0;
        }
        break;
        
    // Write command
    case EEPROMCommand::WRITE:
        // Address bits come first (either 6 or 14 bits depending on size)
        // For 512B EEPROM: 2 bit command + 6 bit address + 64 bit data = 72 bits total
        // For 8K EEPROM: 2 bit command + 14 bit address + 64 bit data = 80 bits total
        if (writeSize > 65) {
            // Still receiving address bits
            // Detect 8K EEPROM: if we're receiving bits beyond writeSize 72, it's 8K
            if (writeSize > 72 && !m_is8K) {
                upgradeTo8K();
            }
            m_writeAddress <<= 1;
            m_writeAddress |= (value & 1) << 6;
        } else if (writeSize == 1) {
            // Final bit - stop bit
            m_command = EEPROMCommand::NULL_CMD;
        } else if ((m_writeAddress >> 3) < m_data.size()) {
            // Data bits
            u32 byteAddr = m_writeAddress >> 3;
            u32 bitAddr = m_writeAddress & 7;
            
            // Write bit
            if (byteAddr < m_data.size()) {
                u8 current = m_data[byteAddr];
                current &= ~(1 << (7 - bitAddr));
                current |= (value & 1) << (7 - bitAddr);
                m_data[byteAddr] = current;
            }
            
            ++m_writeAddress;
        }
        break;
        
    // Read command
    case EEPROMCommand::READ_PENDING:
        if (writeSize > 1) {
            // Receiving address bits
            // Detect 8K EEPROM: if we're receiving bits beyond writeSize 72, it's 8K
            if (writeSize > 72 && !m_is8K) {
                upgradeTo8K();
            }
            m_readAddress <<= 1;
            if (value & 1) {
                m_readAddress |= 0x40;
            }
        } else {
            // Start reading - return 68 bits (4 dummy + 64 data)
            m_readBitsRemaining = 68;
            m_command = EEPROMCommand::READ;
        }
        break;
    }
}

void EEPROM::saveState(Buffer* buf) {
    u8 initialized = m_initialized ? 1 : 0;
    u8 is8k = m_is8K ? 1 : 0;
    buffer_write(buf, &initialized, sizeof(initialized));
    buffer_write(buf, &is8k, sizeof(is8k));
    
    if (m_initialized) {
        u32 size = static_cast<u32>(m_data.size());
        buffer_write(buf, &size, sizeof(size));
        buffer_write(buf, m_data.data(), size);
        
        u8 cmd = static_cast<u8>(m_command);
        buffer_write(buf, &cmd, sizeof(cmd));
        buffer_write(buf, &m_writeAddress, sizeof(m_writeAddress));
        buffer_write(buf, &m_readAddress, sizeof(m_readAddress));
        buffer_write(buf, &m_readBitsRemaining, sizeof(m_readBitsRemaining));
    }
}

void EEPROM::loadState(Buffer* buf) {
    u8 initialized, is8k;
    buffer_read(buf, &initialized, sizeof(initialized));
    buffer_read(buf, &is8k, sizeof(is8k));
    m_initialized = initialized != 0;
    m_is8K = is8k != 0;
    
    if (m_initialized) {
        u32 size;
        buffer_read(buf, &size, sizeof(size));
        m_data.resize(size);
        buffer_read(buf, m_data.data(), size);
        
        u8 cmd;
        buffer_read(buf, &cmd, sizeof(cmd));
        m_command = static_cast<EEPROMCommand>(cmd);
        buffer_read(buf, &m_writeAddress, sizeof(m_writeAddress));
        buffer_read(buf, &m_readAddress, sizeof(m_readAddress));
        buffer_read(buf, &m_readBitsRemaining, sizeof(m_readBitsRemaining));
    }
}

} // namespace gba

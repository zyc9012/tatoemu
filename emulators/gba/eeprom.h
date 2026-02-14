#pragma once

#include "types.h"
#include "../components/buffer.h"
#include <vector>

namespace gba {

// EEPROM sizes
constexpr u32 EEPROM_512_SIZE = 0x200;   // 512 bytes (4kbit)
constexpr u32 EEPROM_8K_SIZE = 0x2000;   // 8KB (64kbit)

// EEPROM commands
enum class EEPROMCommand {
    NULL_CMD = 0,
    PENDING = 1,
    WRITE = 2,
    READ_PENDING = 3,
    READ = 4
};

class EEPROM {
public:
    EEPROM();
    ~EEPROM();

    void reset();
    
    // Initialize with size (512 bytes or 8KB)
    void init(bool is8K);
    
    // DMA access (called when DMA accesses ROM2_EX region 0x0D000000)
    u16 read();
    void write(u16 value, u32 writeSize);
    
    // Get data pointer for battery save
    u8* getData() { return m_data.data(); }
    const u8* getData() const { return m_data.data(); }
    u32 getSize() const { return static_cast<u32>(m_data.size()); }
    
    bool isInitialized() const { return m_initialized; }
    bool is8K() const { return m_is8K; }
    
    // Upgrade from 512B to 8K if needed
    void upgradeTo8K();
    
    // Save/Load state
    void saveState(Buffer* buf);
    void loadState(Buffer* buf);
    
private:
    bool m_initialized = false;
    bool m_is8K = false;
    std::vector<u8> m_data;
    
    // State machine
    EEPROMCommand m_command = EEPROMCommand::NULL_CMD;
    u32 m_writeAddress = 0;
    u32 m_readAddress = 0;
    u8 m_readBitsRemaining = 0;
};

} // namespace gba

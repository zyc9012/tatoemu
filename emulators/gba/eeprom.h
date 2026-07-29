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
    
    // One walk over the persistent state.  Defined here rather than in the
    // .cpp because the cartridge walks the chip as part of its own state.
    template <typename Visit> void visitState(Visit visit) {
        // The two flags are stored as bytes, not as bool.
        u8 initialized = m_initialized ? 1 : 0;
        u8 is8k = m_is8K ? 1 : 0;
        visit(initialized);
        visit(is8k);
        m_initialized = initialized != 0;
        m_is8K = is8k != 0;

        // An uninitialized chip holds nothing else.
        if (m_initialized) {
            u32 size = static_cast<u32>(m_data.size());
            visit(size);
            if constexpr (Visit::loading) {
                m_data.resize(size);
            }
            visit.bytes(m_data.data(), size);

            u8 cmd = static_cast<u8>(m_command);
            visit(cmd);
            m_command = static_cast<EEPROMCommand>(cmd);
            visit(m_writeAddress);
            visit(m_readAddress);
            visit(m_readBitsRemaining);
        }
    }

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

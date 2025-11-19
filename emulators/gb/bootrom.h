#pragma once

#include "types.h"
#include <array>
#include <string>

namespace gb {

class Bootrom {
public:
    Bootrom();
    ~Bootrom();

    // Load bootrom from file
    bool load(const fs::path& filename);
    
    // Check if bootrom is loaded and enabled
    bool isLoaded() const { return m_loaded; }
    bool isEnabled() const { return m_enabled; }
    
    // Read bootrom data
    u8 read(u16 address) const;
    
    // Disable bootrom (called when 0xFF50 is written to)
    void disable();
    
    // Reset bootrom to enabled state
    void reset();
    
    // Get bootrom size
    u16 getSize() const { return m_size; }
    
    // Check if bootrom is GBC type
    bool isGBC() const { return m_isGBC; }

private:
    static constexpr u16 DMG_BOOTROM_SIZE = 256;   // 0x0000-0x00FF
    static constexpr u16 GBC_BOOTROM_SIZE = 2304;  // 0x0000-0x08FF (includes gap at 0x100-0x1FF)
    
    std::array<u8, GBC_BOOTROM_SIZE> m_data;
    bool m_loaded;
    bool m_enabled;
    u16 m_size;
    bool m_isGBC;
};

} // namespace gb


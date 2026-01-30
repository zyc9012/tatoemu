#include "bootrom.h"
#include "../types.h"
#include <algorithm>

namespace gb {

Bootrom::Bootrom()
    : m_loaded(false)
    , m_enabled(false)
    , m_size(0)
    , m_isGBC(false) {
    std::fill(m_data.begin(), m_data.end(), 0);
}

Bootrom::~Bootrom() {
}

bool Bootrom::load(const fs::path& filename) {
    FILE* file = fopen(filename.c_str(), "rb");
    if (!file) {
        log_error("Failed to open bootrom file: %s", filename.c_str());
        return false;
    }
    
    // Get file size
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    // Determine bootrom type based on size
    if (size == DMG_BOOTROM_SIZE) {
        m_size = DMG_BOOTROM_SIZE;
        m_isGBC = false;
        log_info("Loading DMG bootrom (%ld bytes)", size);
    } else if (size == GBC_BOOTROM_SIZE) {
        m_size = GBC_BOOTROM_SIZE;
        m_isGBC = true;
        log_info("Loading GBC bootrom (%ld bytes)", size);
    } else {
        log_error("Invalid bootrom size: %ld bytes. Expected %d (DMG) or %d (GBC)", size, DMG_BOOTROM_SIZE, GBC_BOOTROM_SIZE);
        return false;
    }
    
    // Read bootrom data
    if (!fread(m_data.data(), 1, m_size, file)) {
        log_error("Failed to read bootrom data");
        fclose(file);
        return false;
    }

    fclose(file);
    m_loaded = true;
    m_enabled = true;
    
    log_info("Bootrom loaded successfully");
    return true;
}

u8 Bootrom::read(u16 address) const {
    if (!m_enabled || !m_loaded) {
        return 0xFF;
    }
    
    // DMG bootrom: 0x0000-0x00FF (256 bytes)
    // GBC bootrom: 0x0000-0x00FF and 0x0200-0x09FF (file has gap at 0x100-0x1FF)
    if (m_isGBC) {
        // GBC bootrom file is laid out with memory addresses directly
        // File structure: [0x000-0x0FF][gap: 0x100-0x1FF][0x200-0x8FF]
        if (address < 0x0900) {
            return m_data[address];
        }
    } else {
        // DMG bootrom: 0x0000-0x00FF only
        if (address < 0x0100) {
            return m_data[address];
        }
    }
    
    return 0xFF; // Outside bootrom range
}

void Bootrom::disable() {
    if (m_enabled) {
        m_enabled = false;
        log_info("Bootrom disabled");
    }
}

void Bootrom::reset() {
    if (m_loaded) {
        m_enabled = true;
    }
}

} // namespace gb


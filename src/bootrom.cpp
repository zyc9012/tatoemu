#include "bootrom.h"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cstring>
#include <vector>

Bootrom::Bootrom()
    : m_loaded(false)
    , m_enabled(false)
    , m_size(0)
    , m_isGBC(false) {
    std::fill(m_data.begin(), m_data.end(), 0);
}

Bootrom::~Bootrom() {
}

bool Bootrom::load(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "Failed to open bootrom file: " << filename << std::endl;
        return false;
    }
    
    // Get file size
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<u8> data(GBC_BOOTROM_SIZE);
    if (!file.read(reinterpret_cast<char*>(data.data()), size)) {
        std::cerr << "Failed to read bootrom data" << std::endl;
        return false;
    }
    
    return loadFromMemory(data.data(), size);
}

bool Bootrom::loadFromMemory(const u8* data, size_t size) {
    if (!data) {
        std::cerr << "Invalid bootrom data" << std::endl;
        return false;
    }
    
    // Determine bootrom type based on size
    if (size == DMG_BOOTROM_SIZE) {
        m_size = DMG_BOOTROM_SIZE;
        m_isGBC = false;
        std::cout << "Loading DMG bootrom (" << size << " bytes)" << std::endl;
    } else if (size == GBC_BOOTROM_SIZE) {
        m_size = GBC_BOOTROM_SIZE;
        m_isGBC = true;
        std::cout << "Loading GBC bootrom (" << size << " bytes)" << std::endl;
    } else {
        std::cerr << "Invalid bootrom size: " << size << " bytes. Expected " 
                  << DMG_BOOTROM_SIZE << " (DMG) or " << GBC_BOOTROM_SIZE << " (GBC)" << std::endl;
        return false;
    }
    
    // Copy bootrom data
    std::memcpy(m_data.data(), data, m_size);
    
    m_loaded = true;
    m_enabled = true;
    
    std::cout << "Bootrom loaded successfully" << std::endl;
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
        std::cout << "Bootrom disabled" << std::endl;
    }
}

void Bootrom::reset() {
    if (m_loaded) {
        m_enabled = true;
    }
}


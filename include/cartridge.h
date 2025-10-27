#pragma once

#include "types.h"
#include <string>
#include <vector>

class Cartridge {
public:
    Cartridge();
    ~Cartridge();

    bool load(const std::string& filename);
    u8 read(u16 address) const;
    void write(u16 address, u8 value);

    bool isLoaded() const { return m_loaded; }
    const std::string& getTitle() const { return m_title; }

private:
    void parseHeader();

    std::vector<u8> m_rom;
    std::vector<u8> m_ram;
    std::string m_title;
    u8 m_cartridgeType;
    u8 m_romSize;
    u8 m_ramSize;
    bool m_loaded;
    
    // MBC state
    u8 m_currentRomBank;
    u8 m_currentRamBank;
    bool m_ramEnabled;
    u8 m_bankingMode;
};


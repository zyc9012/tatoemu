#pragma once

#include "types.h"
#include <filesystem>
#include <string>
#include <vector>
#include <fstream>
#include <ctime>

namespace fs = std::filesystem;

// MBC Types
enum class MBCType {
    ROM_ONLY = 0x00,
    MBC1 = 0x01,
    MBC1_RAM = 0x02,
    MBC1_RAM_BATTERY = 0x03,
    MBC2 = 0x05,
    MBC2_BATTERY = 0x06,
    ROM_RAM = 0x08,
    ROM_RAM_BATTERY = 0x09,
    MMM01 = 0x0B,
    MMM01_RAM = 0x0C,
    MMM01_RAM_BATTERY = 0x0D,
    MBC3_TIMER_BATTERY = 0x0F,
    MBC3_TIMER_RAM_BATTERY = 0x10,
    MBC3 = 0x11,
    MBC3_RAM = 0x12,
    MBC3_RAM_BATTERY = 0x13,
    MBC5 = 0x19,
    MBC5_RAM = 0x1A,
    MBC5_RAM_BATTERY = 0x1B,
    MBC5_RUMBLE = 0x1C,
    MBC5_RUMBLE_RAM = 0x1D,
    MBC5_RUMBLE_RAM_BATTERY = 0x1E,
    MBC6 = 0x20,
    MBC7_SENSOR_RUMBLE_RAM_BATTERY = 0x22,
    POCKET_CAMERA = 0xFC,
    BANDAI_TAMA5 = 0xFD,
    HUDSON_HUC3 = 0xFE,
    HUDSON_HUC1 = 0xFF
};

// RTC Registers for MBC3
struct RTC {
    u8 seconds;
    u8 minutes;
    u8 hours;
    u8 days_low;
    u8 days_high;
    u8 halt;
    u8 carry;
    
    // Timestamp when RTC was last updated
    std::time_t lastUpdate;
    
    RTC() : seconds(0), minutes(0), hours(0), days_low(0), days_high(0), 
            halt(0), carry(0), lastUpdate(std::time(nullptr)) {}
};

class Cartridge {
public:
    Cartridge();
    ~Cartridge();

    bool load(const std::string& filename);
    u8 read(u16 address) const;
    void write(u16 address, u8 value);

    bool isLoaded() const { return m_loaded; }
    const std::string& getTitle() const { return m_title; }
    bool isGBC() const { return m_isGBC; }
    bool isGBCOnly() const { return m_isGBCOnly; }
    
    // Save/Load state
    void saveState(std::ofstream& file) const;
    void loadState(std::ifstream& file);
    
    // Battery save/load
    void saveBattery() const;
    void loadBattery();
    bool hasBattery() const;

private:
    void parseHeader();
    void updateRTC() const;
    void latchRTC() const;
    
    // MBC-specific read/write functions
    u8 readMBC1(u16 address) const;
    u8 readMBC1M(u16 address) const;
    u8 readMBC2(u16 address) const;
    u8 readMBC3(u16 address) const;
    u8 readMBC30(u16 address) const;
    u8 readMBC5(u16 address) const;
    u8 readMBC7(u16 address) const;
    
    void writeMBC1(u16 address, u8 value);
    void writeMBC1M(u16 address, u8 value);
    void writeMBC2(u16 address, u8 value);
    void writeMBC3(u16 address, u8 value);
    void writeMBC30(u16 address, u8 value);
    void writeMBC5(u16 address, u8 value);
    void writeMBC7(u16 address, u8 value);

    std::vector<u8> m_rom;
    std::vector<u8> m_ram;
    std::string m_title;
    fs::path m_romFilename;
    u8 m_cartridgeType;
    u8 m_romSize;
    u8 m_ramSize;
    bool m_loaded;
    MBCType m_mbcType;
    bool m_isGBC;
    bool m_isGBCOnly;
    
    // MBC state
    u8 m_currentRomBank;
    u8 m_currentRamBank;
    bool m_ramEnabled;
    u8 m_bankingMode;
    
    // MBC1M specific
    u8 m_romBankHigh;
    
    // MBC3 specific
    mutable RTC m_rtc;
    u8 m_rtcRegister;
    mutable bool m_rtcLatched;
    
    // MBC5 specific
    u16 m_romBankHighBits;
    
    // MBC7 specific (accelerometer)
    s16 m_accelX;
    s16 m_accelY;
    s16 m_accelZ;
    u8 m_accelRegister;
    bool m_accelEnabled;
};


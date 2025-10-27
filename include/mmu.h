#pragma once

#include "types.h"
#include <memory>
#include <array>
#include <fstream>

class Cartridge;
class PPU;
class Joypad;
class Timer;
class APU;

class MMU {
public:
    MMU();
    ~MMU();

    void setCartridge(Cartridge* cartridge);
    void setPPU(PPU* ppu);
    void setJoypad(Joypad* joypad);
    void setTimer(Timer* timer);
    void setAPU(APU* apu);

    u8 read(u16 address) const;
    void write(u16 address, u8 value);
    
    void setGBCMode(bool enabled);

    u8 readIO(u16 address) const;
    void writeIO(u16 address, u8 value);
    
    // Save/Load state
    void saveState(std::ofstream& file) const;
    void loadState(std::ifstream& file);

private:
    Cartridge* m_cartridge;
    PPU* m_ppu;
    Joypad* m_joypad;
    Timer* m_timer;
    APU* m_apu;

    // Memory regions
    std::array<u8, 0x8000> m_wram;     // Work RAM (32KB for GBC, 8 banks)
    std::array<u8, 0x80> m_hram;       // High RAM (127 bytes)
    u8 m_ie;                            // Interrupt Enable register
    
    // GBC specific
    bool m_gbcMode;
    u8 m_wramBank;                      // WRAM bank (1-7 for GBC)
    u8 m_speedSwitch;                   // Speed switch register (KEY1)
    bool m_doubleSpeed;                 // Current speed mode
};


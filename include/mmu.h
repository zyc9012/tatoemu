#pragma once

#include "types.h"
#include <memory>
#include <array>

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

    u8 readIO(u16 address) const;
    void writeIO(u16 address, u8 value);

private:
    Cartridge* m_cartridge;
    PPU* m_ppu;
    Joypad* m_joypad;
    Timer* m_timer;
    APU* m_apu;

    // Memory regions
    std::array<u8, 0x2000> m_wram;     // Work RAM (8KB)
    std::array<u8, 0x80> m_hram;       // High RAM (127 bytes)
    u8 m_ie;                            // Interrupt Enable register
};


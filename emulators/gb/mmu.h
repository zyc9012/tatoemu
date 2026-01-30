#pragma once

#include "types.h"
#include <memory>
#include <array>
#include "../components/buffer.h"

namespace gb {

class Cartridge;
class PPU;
class Joypad;
class Timer;
class APU;
class Bootrom;

class MMU {
public:
    MMU();
    ~MMU();

    void setCartridge(Cartridge* cartridge);
    void setPPU(PPU* ppu);
    void setJoypad(Joypad* joypad);
    void setTimer(Timer* timer);
    void setAPU(APU* apu);
    void setBootrom(Bootrom* bootrom);

    u8 read(u16 address) const;
    void write(u16 address, u8 value);
    
    void setGBCMode(bool enabled);

    u8 readIO(u16 address) const;
    void writeIO(u16 address, u8 value);
    
    // GBC speed switching
    void performSpeedSwitch();
    bool isDoubleSpeed() const { return m_doubleSpeed; }
    
    // Save/Load state
    void saveState(Buffer* buf);
    void loadState(Buffer* buf);

private:
    Cartridge* m_cartridge;
    PPU* m_ppu;
    Joypad* m_joypad;
    Timer* m_timer;
    APU* m_apu;
    Bootrom* m_bootrom;

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

} // namespace gb


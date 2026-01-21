#pragma once

#include "../types.h"
#include <fstream>
#include <vector>

namespace neogeo {

class SoundCPU;
class Memory;
class Cartridge;
class AudioDevice;

// NeoGeo Audio Processing Unit
// Uses YM2610 (OPNB) for FM synthesis, SSG (AY-3-8910), and ADPCM
class APU {
public:
    APU();
    ~APU();

    void init(u32 sampleRate = 44100);
    void reset();
    void step(u32 cycles, double gameSpeed);
    
    void setSoundCPU(SoundCPU* soundCpu);
    void setMemory(Memory* memory) { m_memory = memory; }
    void setAudioDevice(::AudioDevice* audioDevice) { m_audioDevice = audioDevice; }
    void setCartridge(Cartridge* cartridge) { m_cartridge = cartridge; }
    
    void setSampleRate(u32 sampleRate);
    void setVolume(float volume) { m_volume = volume; }
    
    // I/O port access for Z80
    u8 readPort(u16 port);
    void writePort(u16 port, u8 value);
    
    // Sound command from 68000
    void setSoundCommand(u8 command);
    u8 getSoundReply() const { return m_soundReply; }
    bool getSoundStatus() const { return m_soundStatus; }

    // YM2610 timer handling - called by SoundCPU during execution
    void updateTimers(u32 cycles);
    
    // Set timer value (called by YM2610 timer handler)
    // timer: 0=A, 1=B, cycles: countdown value (-1=disabled)
    void setTimer(int timer, s32 cycles);
    
    // NMI control
    bool isNMIEnabled() const { return m_nmiEnabled; }
    
    // Save/Load state
    void saveState(std::ofstream& file);
    void loadState(std::ifstream& file);

private:
    SoundCPU* m_soundCpu;
    Memory* m_memory;
    Cartridge* m_cartridge;
    ::AudioDevice* m_audioDevice;
    
    u32 m_sampleRate;
    float m_volume;
    
    // Sound communication
    u8 m_soundCommand;     // Command from 68000
    u8 m_soundReply;       // Reply to 68000
    bool m_soundStatus;    // True when command received
    
    // NMI control
    bool m_nmiEnabled;
    
    // YM2610 timers (Timer A and Timer B)
    // Timer values are in Z80 cycles (4 MHz clock)
    s32 m_timerA;          // Timer A countdown (cycles until fire, -1 = disabled)
    s32 m_timerB;          // Timer B countdown (cycles until fire, -1 = disabled)
    
    // Sample generation
    double m_cycleAccumulator;
    u32 m_cyclesPerSample;
};

} // namespace neogeo

#pragma once

#include "../types.h"
#include <fstream>

namespace neogeo {

class SoundCPU;
class Memory;
class Cartridge;
class AudioDevice;

// NeoGeo Audio Processing Unit (Stub)
// Uses YM2610 (OPNB) for FM synthesis and ADPCM
// TODO: Implement actual YM2610 emulation
class APU {
public:
    APU();
    ~APU() = default;

    void reset();
    void step(u32 cycles, double gameSpeed);
    
    void setSoundCPU(SoundCPU* soundCpu) { m_soundCpu = soundCpu; }
    void setMemory(Memory* memory) { m_memory = memory; }
    void setAudioDevice(::AudioDevice* audioDevice) { m_audioDevice = audioDevice; }
    void setCartridge(Cartridge* cartridge) { m_cartridge = cartridge; }
    
    void setSampleRate(u32 sampleRate) { m_sampleRate = sampleRate; }
    void setVolume(float volume) { m_volume = volume; }
    
    // I/O port access for Z80
    u8 readPort(u16 port);
    void writePort(u16 port, u8 value);
    
    // Sound command from 68000
    void setSoundCommand(u8 command);
    u8 getSoundReply() const { return m_soundReply; }
    bool getSoundStatus() const { return m_soundStatus; }
    
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
    
    // YM2610 registers (stub)
    u8 m_ym2610RegSelect[2];  // Register select for address A and B
    
    // Sample generation
    double m_cycleAccumulator;
    u64 m_cyclesPerSample;
};

} // namespace neogeo

#pragma once

#include "../types.h"
#include <fstream>

namespace cps {

class SoundCPU;
class Memory;
class AudioDevice;

// Base class for Audio Processing Unit (shared interface)
class APUBase {
public:
    virtual ~APUBase() = default;

    virtual void reset() = 0;
    virtual void step(u32 cycles, double gameSpeed) = 0;
    
    virtual void setSoundCPU(SoundCPU* soundCpu) = 0;
    virtual void setMemory(Memory* memory) = 0;
    virtual void setAudioDevice(::AudioDevice* audioDevice) = 0;
    
    virtual void setSampleRate(u32 sampleRate) = 0;
    virtual void setVolume(float volume) = 0;
    
    // I/O port access for Z80
    virtual u8 readPort(u16 port) = 0;
    virtual void writePort(u16 port, u8 value) = 0;
    
    // Save/Load state
    virtual void saveState(std::ofstream& file) = 0;
    virtual void loadState(std::ifstream& file) = 0;
};

} // namespace cps

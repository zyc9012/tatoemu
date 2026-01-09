#pragma once

#include "../types.h"
#include <fstream>

namespace cps {

class SoundCPU;
class MemoryBase;
class AudioDevice;

// Base class for Audio Processing Unit (shared interface)
class APUBase {
public:
    virtual ~APUBase() = default;

    virtual void reset() = 0;
    virtual void step(u32 cycles, double gameSpeed) = 0;
    
    virtual void setSoundCPU(SoundCPU* soundCpu) = 0;
    virtual void setMemory(MemoryBase* memory) = 0;
    virtual void setAudioDevice(::AudioDevice* audioDevice) = 0;
    
    virtual void setSampleRate(u32 sampleRate) = 0;
    virtual void setVolume(float volume) = 0;
    
    // Save/Load state
    virtual void saveState(std::ofstream& file) = 0;
    virtual void loadState(std::ifstream& file) = 0;
};

} // namespace cps

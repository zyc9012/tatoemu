#pragma once

#include "../../types.h"
#include "../apu_base.h"
#include <fstream>

namespace cps1 {

class Memory;

// Audio Processing Unit for CPS1 (YM2151 + MSM6295)
class APU : public cps::APUBase {
public:
    APU();
    ~APU() = default;

    void reset() override;
    void step(u32 cycles, double gameSpeed) override;
    
    void setSoundCPU(cps::SoundCPU* soundCpu) override { m_soundCpu = soundCpu; }
    void setMemory(cps::MemoryBase* memory) override;
    void setAudioDevice(::AudioDevice* audioDevice) override { m_audioDevice = audioDevice; }
    
    void setSampleRate(u32 sampleRate) override;
    void setVolume(float volume) override;
    
    // Save/Load state
    void saveState(std::ofstream& file) override;
    void loadState(std::ifstream& file) override;

private:
    cps::SoundCPU* m_soundCpu;
    Memory* m_memory;
    ::AudioDevice* m_audioDevice;
    
    u32 m_sampleRate;
    float m_volume;
    
    // YM2151 state
    // TODO: Implement YM2151 FM synthesis chip
    
    // MSM6295 state
    // TODO: Implement MSM6295 ADPCM chip
    
    void generateSamples(u32 cycles);
};

} // namespace cps1

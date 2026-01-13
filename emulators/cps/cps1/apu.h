#pragma once

#include "../../types.h"
#include "../apu_base.h"
#include "../../components/sound/ym2151/ym2151.h"
#include "../../components/sound/msm6295/msm6295.h"
#include "../../components/compact.h"
#include <fstream>
#include <memory>
#include <vector>

namespace cps1 {

class Memory;
class Cartridge;

// Audio Processing Unit for CPS1 (YM2151 + MSM6295)
class APU : public cps::APUBase {
public:
    APU();
    ~APU();

    void reset() override;
    void step(u32 cycles, double gameSpeed) override;
    
    void setSoundCPU(cps::SoundCPU* soundCpu) override;
    void setMemory(cps::MemoryBase* memory) override;
    void setAudioDevice(::AudioDevice* audioDevice) override { m_audioDevice = audioDevice; }
    void setCartridge(Cartridge* cartridge);
    
    void setSampleRate(u32 sampleRate) override;
    void setVolume(float volume) override;
    
    // I/O port access for Z80
    u8 readPort(u16 port) override;
    void writePort(u16 port, u8 value) override;
    
    // Save/Load state
    void saveState(std::ofstream& file) override;
    void loadState(std::ifstream& file) override;

private:
    cps::SoundCPU* m_soundCpu;
    Memory* m_memory;
    Cartridge* m_cartridge;
    ::AudioDevice* m_audioDevice;
    
    u32 m_sampleRate;
    float m_volume;
    
    // Sample buffers
    s16 m_ym2151LeftSample = 0;
    s16 m_ym2151RightSample = 0;
    s16 m_msm6295Samples[2] = { 0, 0 };
    
    // Sample generation
    double m_cycleAccumulator;   // Accumulator for cycle timing
    u64 m_cyclesPerSample;    // CPU cycles per audio sample
    
    // YM2151 register addressing (two-step process)
    u8 m_ym2151RegSelect;
    
    void generateSamples(u32 cycles, double gameSpeed);
    void setROMData();
};

} // namespace cps1

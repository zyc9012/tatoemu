#pragma once

#include "../../types.h"
#include "../apu_base.h"
#include "../../components/sound/ym2151/ym2151.h"
#include "../../components/sound/msm6295/msm6295.h"
#include "../../components/sound/burnint.h"
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
    
    // FBNeo chip numbers
    static constexpr INT32 YM2151_CHIP = 0;
    static constexpr INT32 MSM6295_CHIP = 0;
    
    // Sample buffers for FBNeo
    INT16* m_ym2151LeftBuffer;
    INT16* m_ym2151RightBuffer;
    INT16* m_msm6295Buffer;
    
    // Sample generation
    u64 m_cycleAccumulator;   // Accumulator for cycle timing
    u64 m_cyclesPerSample;    // CPU cycles per audio sample
    std::vector<float> m_sampleBufferLeft;
    std::vector<float> m_sampleBufferRight;
    
    // YM2151 register addressing (two-step process)
    u8 m_ym2151RegSelect;
    
    void generateSamples(u32 cycles);
    void setROMData();
};

} // namespace cps1

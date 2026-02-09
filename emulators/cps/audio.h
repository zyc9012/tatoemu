#pragma once

#include "../types.h"
#include "memory.h"
#include "cartridge.h"
extern "C" {
#include "../components/sound/ym2151/ym2151.h"
}
#include "../components/sound/msm6295/msm6295.h"
#include "../components/sound/qsound/qsound.h"
#include "../components/compact.h"
#include "../components/buffer.h"
#include <memory>
#include <vector>

namespace cps {

// Audio - CPS1: YM2151 + MSM6295, CPS2: + QSound
class Audio {
public:
    Audio();
    ~Audio();

    void reset();
    void step(u32 cycles, double gameSpeed);
    
    void setSoundCPU(SoundCPU* soundCpu);
    void setMemory(Memory* memory);
    void setAudioDevice(::AudioDevice* audioDevice) { m_audioDevice = audioDevice; }
    void setCartridge(Cartridge* cartridge);
    
    void setSampleRate(u32 sampleRate);
    void setVolume(float volume);
    
    // I/O port access for Z80
    u8 readPort(u16 port);
    u8 readQSound();
    void writePort(u16 port, u8 value);
    void writeQSound(u16 port, u16 value);
    
    // Save/Load state
    void saveState(Buffer* buf);
    void loadState(Buffer* buf);

private:
    SoundCPU* m_soundCpu;
    Memory* m_memory;
    Cartridge* m_cartridge;
    ::AudioDevice* m_audioDevice;
    
    u32 m_sampleRate;
    float m_volume;
    
    // Sample buffers
    s16 m_ym2151LeftSample = 0;
    s16 m_ym2151RightSample = 0;
    s16 m_msm6295Samples[2] = { 0, 0 };
    s16 m_qsoundSamples[2] = { 0, 0 };
    
    // Sample generation
    double m_cycleAccumulator;   // Accumulator for cycle timing
    u64 m_cyclesPerSample;    // CPU cycles per audio sample
    
    // YM2151 register addressing (two-step process)
    u8 m_ym2151RegSelect;
    
    u8 getCPSVersion() const;
    void generateSamples(u32 cycles, double gameSpeed);
    void setROMData();
};

} // namespace cps

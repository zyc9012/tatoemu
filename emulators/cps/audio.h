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
#include <array>

namespace cps {

class CPU;

// Maximum samples per frame (44100 / 59.63 ≈ 740, round up with headroom)
static constexpr u32 AUDIO_BUFFER_SIZE = 1024;

// Audio - CPS1: YM2151 + MSM6295, CPS2: + QSound
class Audio {
public:
    Audio();
    ~Audio();

    void reset();

    // Called at end of frame — catches up Z80, renders all samples, mixes, outputs.
    void endFrame(double gameSpeed);
    
    void setSoundCPU(SoundCPU* soundCpu);
    void setCPU(CPU* cpu) { m_cpu = cpu; }
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
    CPU* m_cpu;
    Memory* m_memory;
    Cartridge* m_cartridge;
    ::AudioDevice* m_audioDevice;
    
    u32 m_sampleRate;
    float m_volume;
    
    // YM2151 register addressing (two-step process)
    u8 m_ym2151RegSelect;

    // Cached timing constants (set during reset based on CPS version)
    u32 m_soundCPUCyclesPerFrame;
    u32 m_soundCPUFrequency;
    u32 m_soundCyclesPerSample;  // m_soundCPUFrequency / m_sampleRate

    // Intermediate chip-rate buffers (CPS1)
    std::array<s16, AUDIO_BUFFER_SIZE> m_ym2151Left;
    std::array<s16, AUDIO_BUFFER_SIZE> m_ym2151Right;
    std::array<s16, AUDIO_BUFFER_SIZE * 2> m_msm6295Buf;  // interleaved L,R

    // QSound output buffers (CPS2 / CPS1 QSound)
    std::array<s16, AUDIO_BUFFER_SIZE> m_qsoundLeft;
    std::array<s16, AUDIO_BUFFER_SIZE> m_qsoundRight;

    // Stereo interleaved mix buffer (L,R,L,R,...)
    std::array<float, AUDIO_BUFFER_SIZE * 2> m_mixBuffer;
    
    void setROMData();

    // Render one sample from sound chips at buffer position `index`
    void renderOneSample(u32 index);

    // Run Z80 to catch up to the given Z80 cycle position
    void runSoundCPUTo(s32 targetZ80Cycle);
};

} // namespace cps

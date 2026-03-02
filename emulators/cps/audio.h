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

    // Stereo interleaved mix buffer (L,R,L,R,...) — flushed when full
    static constexpr u32 MIX_BUFFER_SIZE = 2048;
    std::array<float, MIX_BUFFER_SIZE> m_mixBuffer;
    u32 m_mixPos;
    
    void setROMData();

    // Run Z80, render one sample, and mix it into the output buffer
    void renderAndMixOneSample();

    // Flush mix buffer to audio device
    void flushMixBuffer();

    // Run Z80 to catch up to the given Z80 cycle position
    void runSoundCPUTo(s32 targetZ80Cycle);
};

} // namespace cps

#pragma once

#include "../types.h"
#include "../../components/buffer.h"
#include <vector>
#include <array>

namespace neogeo {

class SoundCPU;
class CPU;
class Memory;
class Cartridge;
class AudioDevice;

// Maximum samples per frame (44100 / 59.18 ≈ 745, round up with headroom)
static constexpr u32 AUDIO_BUFFER_SIZE = 1024;

// NeoGeo Audio - Uses YM2610 (OPNB) for FM synthesis, SSG (AY-3-8910), and ADPCM
class Audio {
public:
    Audio();
    ~Audio();

    void init(u32 sampleRate = 44100);
    void reset();

    void renderUpTo();
    void endFrame(double gameSpeed);
    
    void setSoundCPU(SoundCPU* soundCpu);
    void setCPU(CPU* cpu) { m_cpu = cpu; }
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
    u8 getSoundReply();
    bool getSoundStatus() const { return m_soundStatus; }

    // YM2610 timer handling - called by SoundCPU during execution
    void updateTimers(u32 cycles);

    // Returns Z80 cycles until the next timer fires (or UINT32_MAX if none active)
    u32 cyclesToNextTimer() const;
    
    // Set timer value (called by YM2610 timer handler)
    // timer: 0=A, 1=B, cycles: countdown value (-1=disabled)
    void setTimer(int timer, s32 cycles);
    
    // NMI control
    bool isNMIEnabled() const { return m_nmiEnabled; }
    
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
    
    // Intermediate chip-rate buffers
    std::array<s16, AUDIO_BUFFER_SIZE> m_ym2610Left;
    std::array<s16, AUDIO_BUFFER_SIZE> m_ym2610Right;
    std::array<s16, AUDIO_BUFFER_SIZE> m_ay8910A;
    std::array<s16, AUDIO_BUFFER_SIZE> m_ay8910B;
    std::array<s16, AUDIO_BUFFER_SIZE> m_ay8910C;

    // How many samples have been rendered into the buffers so far
    u32 m_ym2610Position;
    u32 m_ay8910Position;

    // Stereo interleaved mix buffer (L,R,L,R,...)
    std::array<float, AUDIO_BUFFER_SIZE * 2> m_mixBuffer;

    u32 computeSamplesNeeded() const;
    void renderSamples(u32 samplesNeeded);

    // Run Z80 to catch up to the given Z80 cycle position
    void runSoundCPUTo(s32 targetZ80Cycle);
};

} // namespace neogeo

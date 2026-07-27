#pragma once

#include "consts.h"
#include "../types.h"
#include "../components/buffer.h"
#include "../../components/sound/sn76496/sn76496.h"
#include <array>

namespace md {

class CPU;

// Worst case is PAL at 48 kHz plus headroom for slow-motion playback.
constexpr u32 AUDIO_BUFFER_SIZE = 2048;

// ---------------------------------------------------------------------------
// Mega Drive audio: YM2612 (OPN2) FM synthesiser plus the SN76489 PSG that
// lives inside the VDP.
// ---------------------------------------------------------------------------
class Audio {
public:
    Audio();
    ~Audio();

    void init(u32 sampleRate);
    void reset();

    void setCPU(CPU* cpu) { m_cpu = cpu; }
    void setAudioDevice(::AudioDevice* device) { m_audioDevice = device; }
    void setFrameCycles(u32 cycles) { m_frameCycles = cycles ? cycles : 1; }

    void setSampleRate(u32 sampleRate);
    void setVolume(float volume) { m_volume = volume; }

    // --- chip access ---
    void writeFM(u8 port, u8 value);
    u8   readFM(u8 port);
    void writePSG(u8 value);

    // Advances the YM2612 timers; called once per scanline by Core.
    void updateTimers(u32 m68kCycles);
    void setTimer(int timer, s32 cycles);

    // Renders everything generated so far, then flushes the frame to the device.
    void renderUpTo();
    void endFrame(double gameSpeed);

    void saveState(Buffer* buf);
    void loadState(Buffer* buf);

private:
    u32  computeSamplesNeeded() const;
    void renderSamples(u32 samplesNeeded);

    CPU* m_cpu = nullptr;
    ::AudioDevice* m_audioDevice = nullptr;

    u32 m_sampleRate = 44100;
    float m_volume = 1.0f;
    u32 m_frameCycles = 1;

    SN76496 m_psg;

    // YM2612 timers, counted in 68000 cycles (-1 when disabled).
    s32 m_timerA = -1;
    s32 m_timerB = -1;

    std::array<s16, AUDIO_BUFFER_SIZE> m_fmLeft{};
    std::array<s16, AUDIO_BUFFER_SIZE> m_fmRight{};
    std::array<s16, AUDIO_BUFFER_SIZE> m_psgOut{};

    u32 m_fmPosition = 0;
    u32 m_psgPosition = 0;

    std::array<float, AUDIO_BUFFER_SIZE * 2> m_mixBuffer{};
};

} // namespace md

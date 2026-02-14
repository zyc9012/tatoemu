#pragma once

#include "types.h"
#include "consts.h"
#include "../components/buffer.h"
#include <array>

namespace gba {

class Memory;
class Timer;
class DMA;

class APU {
public:
    APU();
    ~APU();

    void setMemory(Memory* memory) { m_memory = memory; }
    void setTimer(Timer* timer) { m_timer = timer; }
    void setDMA(DMA* dma) { m_dma = dma; }
    void setAudioDevice(AudioDevice* device) { m_audioDevice = device; }

    void reset();
    void step(u32 cycles, double gameSpeed = 1.0);

    // IO register access
    u8 readRegister(u32 offset) const;
    void writeRegister(u32 offset, u16 value);

    // Timer overflow callback — feeds FIFO channels
    void onTimerOverflow(int timerChannel);

    // Save/Load state
    void saveState(Buffer* buf);
    void loadState(Buffer* buf);

    // Configuration
    void setSampleRate(u32 sampleRate) { m_sampleRate = sampleRate; }
    void setVolume(float volume) { m_volume = volume; }

private:
    // -------------------------------------------------------
    // GB-compatible PSG channels
    // -------------------------------------------------------
    struct SquareChannel {
        // NRx0 — Sweep (Channel 1 only)
        u8 sweepPeriod;
        bool sweepNegate;
        u8 sweepShift;
        u8 sweepTimer;
        u16 sweepShadow;
        bool sweepEnabled;

        // NRx1 — Duty / length
        u8 dutyCycle;
        u8 lengthCounter;

        // NRx2 — Volume envelope
        u8 volume;
        bool envelopeAddMode;
        u8 envelopePeriod;
        u8 envelopeTimer;
        u8 currentVolume;

        // NRx3/NRx4 — Frequency / control
        u16 frequency;
        bool lengthEnable;
        bool dacEnabled;

        // Internal
        bool enabled;
        u16 frequencyTimer;
        u8 dutyPosition;

        void reset();
        void trigger(bool hasSweep);
        void clockLength();
        void clockEnvelope();
        void clockSweep();
        u32 getFrequencyTimerPeriod() const;
        s16 getOutput() const;
    };

    struct WaveChannel {
        bool dacEnabled;
        u16 lengthCounter;
        u8 outputLevel;         // 0-3
        u16 frequency;
        bool lengthEnable;

        std::array<u8, 16> waveRAM;
        bool enabled;
        u16 frequencyTimer;
        u8 wavePosition;

        // GBA extension: force 75% volume flag (SOUNDCNT_H bit 2)
        bool forceVolume;

        void reset();
        void trigger();
        void clockLength();
        s16 getOutput() const;
    };

    struct NoiseChannel {
        u8 lengthCounter;
        u8 volume;
        bool envelopeAddMode;
        u8 envelopePeriod;
        u8 envelopeTimer;
        u8 currentVolume;
        u8 clockShift;
        bool widthMode;
        u8 divisorCode;
        bool lengthEnable;
        bool dacEnabled;

        bool enabled;
        u16 frequencyTimer;
        u16 lfsr;

        void reset();
        void trigger();
        void clockLength();
        void clockEnvelope();
        u32 getFrequencyPeriod() const;
        s16 getOutput() const;
    };

    // -------------------------------------------------------
    // DMA Sound / FIFO channels (A & B)
    // -------------------------------------------------------
    struct FIFOChannel {
        s8 fifo[32];
        int readPos;
        int writePos;
        int size;
        s8 currentSample;       // latched output sample
        int timerSelect;        // 0 or 1 (Timer 0 or Timer 1)
        bool enableRight;
        bool enableLeft;
        bool fullVolume;        // false = 50%, true = 100%

        void reset();
        void write8(u8 value);
        void write32(u32 value);
        s8 dequeue();
        int availableSamples() const { return size; }
    };

    // Frame sequencer (512 Hz = 8192 CPU cycles per tick at 16.78 MHz)
    void clockFrameSequencer();

    // Sample generation and output
    void generateSample(double gameSpeed);

    // Duty cycle patterns (same as GB)
    static constexpr u8 DUTY_PATTERNS[4][8] = {
        {0, 0, 0, 0, 0, 0, 0, 1}, // 12.5%
        {1, 0, 0, 0, 0, 0, 0, 1}, // 25%
        {1, 0, 0, 0, 0, 1, 1, 1}, // 50%
        {0, 1, 1, 1, 1, 1, 1, 0}, // 75%
    };

    Memory* m_memory = nullptr;
    Timer* m_timer = nullptr;
    DMA* m_dma = nullptr;
    AudioDevice* m_audioDevice = nullptr;

    // PSG channels
    SquareChannel m_square1;
    SquareChannel m_square2;
    WaveChannel m_wave;
    NoiseChannel m_noise;

    // FIFO channels
    FIFOChannel m_fifoA;
    FIFOChannel m_fifoB;

    // SOUNDCNT_L (0x080) — PSG master volume / panning
    u8 m_psgVolumeRight;    // bits 0-2
    u8 m_psgVolumeLeft;     // bits 4-6
    u8 m_psgEnableRight;    // bits 8-11 (channels 1-4)
    u8 m_psgEnableLeft;     // bits 12-15

    // SOUNDCNT_H (0x082) — Mixing / DMA sound control
    u8 m_psgMasterVolume;   // bits 0-1 (0=25%, 1=50%, 2=100%, 3=prohibited)

    // SOUNDCNT_X (0x084) — Master enable
    bool m_masterEnable;

    // SOUNDBIAS (0x088)
    u16 m_soundBias;

    // Frame sequencer
    u32 m_frameSequencerTimer;
    u8 m_frameSequencerStep;

    // Sample generation
    u32 m_sampleTimer;
    u32 m_sampleRate;
    float m_volume;

    // High-pass filter state
    float m_capacitorLeft;
    float m_capacitorRight;
};

} // namespace gba

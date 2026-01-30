#pragma once

#include "types.h"
#include "config.h"
#include "../components/buffer.h"
#include <array>

namespace gb {

class CPU;
class MMU;

class APU {
public:
    static constexpr s16 DAC_BIAS = 7;

    APU();
    ~APU();

    void setCPU(CPU* cpu);
    void setMMU(MMU* mmu);
    void setAudioDevice(AudioDevice* device);
    void reset();
    void step(u32 cycles, double playbackSpeed);
    
    u8 readRegister(u16 address) const;
    void writeRegister(u16 address, u8 value);
    
    // Save/Load state
    void saveState(Buffer* buf);
    void loadState(Buffer* buf);

    // Global configuration
    void setSampleRate(u32 sampleRate) { m_sampleRate = sampleRate; }
    void setVolume(float volume) { m_volume = volume; }

private:
    // Channel structures
    struct SquareChannel {
        // NRx0 - Sweep (Channel 1 only)
        u8 sweepPeriod;
        bool sweepNegate;
        u8 sweepShift;
        u8 sweepTimer;
        u16 sweepShadow;
        bool sweepEnabled;
        
        // NRx1 - Length timer & duty
        u8 dutyCycle;        // 0-3
        u8 lengthCounter;    // 6-bit (0-63)
        
        // NRx2 - Volume envelope
        u8 volume;           // Initial volume (0-15)
        bool envelopeAddMode;
        u8 envelopePeriod;
        u8 envelopeTimer;
        u8 currentVolume;
        
        // NRx3/NRx4 - Frequency & control
        u16 frequency;       // 11-bit frequency
        bool lengthEnable;
        bool dacEnabled;
        
        // Internal state
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
        // NR30 - DAC enable
        bool dacEnabled;
        
        // NR31 - Length timer
        u16 lengthCounter;   // Can be 0-256
        
        // NR32 - Output level
        u8 outputLevel;      // 0-3 (0=mute, 1=100%, 2=50%, 3=25%)
        
        // NR33/NR34 - Frequency & control
        u16 frequency;
        bool lengthEnable;
        
        // Wave RAM (32 4-bit samples)
        std::array<u8, 16> waveRAM;
        
        // Internal state
        bool enabled;
        u16 frequencyTimer;
        u8 wavePosition;
        
        void reset();
        void trigger();
        void clockLength();
        s16 getOutput() const;
    };
    
    struct NoiseChannel {
        // NR41 - Length timer
        u8 lengthCounter;    // 6-bit (0-63)
        
        // NR42 - Volume envelope
        u8 volume;
        bool envelopeAddMode;
        u8 envelopePeriod;
        u8 envelopeTimer;
        u8 currentVolume;
        
        // NR43 - Frequency & randomness
        u8 clockShift;       // 0-15
        bool widthMode;      // 0=15-bit, 1=7-bit
        u8 divisorCode;      // 0-7
        
        // NR44 - Control
        bool lengthEnable;
        bool dacEnabled;
        
        // Internal state
        bool enabled;
        u16 frequencyTimer;
        u16 lfsr;            // Linear feedback shift register
        
        void reset();
        void trigger();
        void clockLength();
        void clockEnvelope();
        u32 getFrequencyPeriod() const;
        s16 getOutput() const;
    };
    
    // Frame sequencer steps (512 Hz)
    void clockFrameSequencer();
    
    // Channel mixing and output
    void generateSample(double playbackSpeed);
    
    // Helper functions
    bool isChannelEnabled(u8 channel) const;
    void updateNR52();
    
    CPU* m_cpu;
    MMU* m_mmu;
    AudioDevice* m_audioDevice;
    
    // Channels
    SquareChannel m_square1;
    SquareChannel m_square2;
    WaveChannel m_wave;
    NoiseChannel m_noise;
    
    // Master control (NR50, NR51, NR52)
    u8 m_leftVolume;     // NR50 bits 4-6
    u8 m_rightVolume;    // NR50 bits 0-2
    bool m_leftVinEnable;  // NR50 bit 7
    bool m_rightVinEnable; // NR50 bit 3
    
    u8 m_leftEnable;     // NR51 - which channels to left
    u8 m_rightEnable;    // NR51 - which channels to right
    
    bool m_enabled;      // NR52 bit 7 - Master enable
    
    // Frame sequencer (512 Hz timer)
    u32 m_frameSequencerTimer;
    u8 m_frameSequencerStep;  // 0-7
    
    // Sample generation
    u32 m_sampleTimer;
    
    // Cycle accumulator for double speed mode (to handle odd cycle counts)
    u32 m_cycleAccumulator;
    
    // Capacitor simulation for high-pass filter
    float m_capacitorLeft;
    float m_capacitorRight;
    
    // Duty cycle patterns
    static constexpr u8 DUTY_PATTERNS[4][8] = {
        {0, 0, 0, 0, 0, 0, 0, 1}, // 12.5%
        {1, 0, 0, 0, 0, 0, 0, 1}, // 25%
        {1, 0, 0, 0, 0, 1, 1, 1}, // 50%
        {0, 1, 1, 1, 1, 1, 1, 0}, // 75%
    };

    // Global configuration
    u32 m_sampleRate;
    float m_volume;
};

} // namespace gb


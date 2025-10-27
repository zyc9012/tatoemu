#pragma once

#include "types.h"
#include <array>
#include <SDL3/SDL.h>

class CPU;

// APU manages the 4 sound channels of the Game Boy
class APU {
public:
    APU();
    ~APU();

    void setCPU(CPU* cpu);
    void reset();
    void step(u32 cycles);
    
    // Register access
    u8 readRegister(u16 address) const;
    void writeRegister(u16 address, u8 value);
    
    // SDL Audio initialization
    bool initializeAudio();
    void closeAudio();
    
    // Audio callback
    void generateSamples(float* stream, int length);

private:
    // Sound Channel 1: Square wave with sweep
    struct Channel1 {
        bool enabled;
        u8 sweep;           // NR10
        u8 lengthDuty;      // NR11
        u8 envelope;        // NR12
        u8 freqLow;         // NR13
        u8 freqHigh;        // NR14
        
        // Internal state
        u16 frequency;
        u8 duty;
        u8 volume;
        u8 envelopePeriod;
        u8 envelopeCounter;
        bool envelopeIncrease;
        u16 frequencyCounter;
        u8 dutyPosition;
        u8 lengthCounter;
        u8 sweepPeriod;
        u8 sweepCounter;
        u8 sweepShift;
        bool sweepIncrease;
    } m_channel1;
    
    // Sound Channel 2: Square wave
    struct Channel2 {
        bool enabled;
        u8 lengthDuty;      // NR21
        u8 envelope;        // NR22
        u8 freqLow;         // NR23
        u8 freqHigh;        // NR24
        
        // Internal state
        u16 frequency;
        u8 duty;
        u8 volume;
        u8 envelopePeriod;
        u8 envelopeCounter;
        bool envelopeIncrease;
        u16 frequencyCounter;
        u8 dutyPosition;
        u8 lengthCounter;
    } m_channel2;
    
    // Sound Channel 3: Wave output
    struct Channel3 {
        bool enabled;
        u8 onOff;           // NR30
        u8 length;          // NR31
        u8 outputLevel;     // NR32
        u8 freqLow;         // NR33
        u8 freqHigh;        // NR34
        
        // Internal state
        u16 frequency;
        u16 frequencyCounter;
        u8 wavePosition;
        u16 lengthCounter;  // u16 to support value of 256
        std::array<u8, 16> waveRAM;
    } m_channel3;
    
    // Sound Channel 4: Noise
    struct Channel4 {
        bool enabled;
        u8 length;          // NR41
        u8 envelope;        // NR42
        u8 polynomial;      // NR43
        u8 control;         // NR44
        
        // Internal state
        u8 volume;
        u8 envelopePeriod;
        u8 envelopeCounter;
        bool envelopeIncrease;
        u8 lengthCounter;
        u16 lfsr;           // Linear Feedback Shift Register
        u16 clockCounter;
    } m_channel4;
    
    // Master control
    u8 m_nr50;  // Channel control / ON-OFF / Volume
    u8 m_nr51;  // Sound output terminal selection
    u8 m_nr52;  // Sound on/off
    
    // Timing
    u32 m_frameSequencerCounter;
    u8 m_frameSequencer;
    
    // Audio
    SDL_AudioStream* m_audioStream;
    CPU* m_cpu;
    
    // Sample rate and buffer
    static constexpr int SAMPLE_RATE = 44100;
    static constexpr int BUFFER_SIZE = 2048;
    
    // Helper methods
    void tickFrameSequencer();
    void tickLengthCounters();
    void tickVolumeEnvelopes();
    void tickSweep();
    
    float getChannel1Sample();
    float getChannel2Sample();
    float getChannel3Sample();
    float getChannel4Sample();
    
    void triggerChannel1();
    void triggerChannel2();
    void triggerChannel3();
    void triggerChannel4();
};


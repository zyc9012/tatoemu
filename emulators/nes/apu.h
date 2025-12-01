#pragma once

#include "../types.h"
#include <fstream>
#include <vector>
#include <array>

namespace nes {

class CPU;
class Memory;
class Cartridge;

// NES APU - Highly accurate implementation
// Based on nesdev.org documentation and hardware analysis
class APU {
public:
    // ============================================================
    // Envelope Generator - Used by Pulse and Noise channels
    // ============================================================
    struct Envelope {
        bool start;           // Start flag (set on $4003/$4007/$400F write)
        bool loop;            // Loop flag (also length counter halt)
        bool constantVolume;  // Constant volume flag
        u8 dividerPeriod;     // Divider reload value (V in register)
        u8 dividerCounter;    // Current divider value
        u8 decayLevel;        // Current envelope level (0-15)
        
        void reset();
        void clock();
        u8 volume() const;
    };

    // ============================================================
    // Sweep Unit - Used by Pulse channels
    // ============================================================
    struct Sweep {
        bool enabled;         // Sweep enabled
        bool negate;          // Negate flag
        bool reload;          // Reload flag
        u8 dividerPeriod;     // Divider period (P)
        u8 dividerCounter;    // Current divider value
        u8 shiftCount;        // Shift count
        bool pulseChannel;    // true = pulse 1 (uses one's complement), false = pulse 2
        
        void reset();
        void clock(u16& timerPeriod);
        u16 targetPeriod(u16 currentPeriod) const;
        bool isMuting(u16 timerPeriod) const;
    };

    // ============================================================
    // Length Counter - Used by all melodic channels
    // ============================================================
    struct LengthCounter {
        bool enabled;         // Channel enabled (from $4015)
        bool halt;            // Halt flag (prevents decrement)
        u8 counter;           // Current counter value
        
        void reset();
        void clock();
        void load(u8 index);
        bool isZero() const { return counter == 0; }
        
        static const u8 LENGTH_TABLE[32];
    };

    // ============================================================
    // Pulse Channel (Square Wave)
    // ============================================================
    struct PulseChannel {
        // Timer
        u16 timerPeriod;      // Timer reload value (11-bit)
        u16 timerCounter;     // Current timer value
        
        // Sequencer
        u8 dutyMode;          // Duty cycle mode (0-3)
        u8 sequencerStep;     // Current step in duty cycle (0-7)
        
        // Components
        Envelope envelope;
        Sweep sweep;
        LengthCounter lengthCounter;
        
        // Channel identification (for sweep one's complement)
        bool isPulse1;
        
        void reset();
        void clockTimer();
        void clockEnvelope() { envelope.clock(); }
        void clockSweep();
        void clockLength() { lengthCounter.clock(); }
        
        u8 output() const;
        
        // Register writes
        void writeControl(u8 value);  // $4000/$4004
        void writeSweep(u8 value);    // $4001/$4005
        void writeTimerLow(u8 value); // $4002/$4006
        void writeTimerHigh(u8 value);// $4003/$4007
        
        static const u8 DUTY_TABLE[4][8];
    };

    // ============================================================
    // Triangle Channel
    // ============================================================
    struct TriangleChannel {
        // Timer
        u16 timerPeriod;      // Timer reload value (11-bit)
        u16 timerCounter;     // Current timer value
        
        // Sequencer
        u8 sequencerStep;     // Current step (0-31)
        
        // Linear counter
        bool linearCounterReload; // Reload flag
        u8 linearCounterPeriod;   // Reload value
        u8 linearCounter;         // Current value
        bool controlFlag;         // Control flag (also length counter halt)
        
        // Length counter
        LengthCounter lengthCounter;
        
        void reset();
        void clockTimer();
        void clockLinearCounter();
        void clockLength() { lengthCounter.clock(); }
        
        u8 output() const;
        
        // Register writes
        void writeControl(u8 value);  // $4008
        void writeTimerLow(u8 value); // $400A
        void writeTimerHigh(u8 value);// $400B
        
        static const u8 TRIANGLE_TABLE[32];
    };

    // ============================================================
    // Noise Channel
    // ============================================================
    struct NoiseChannel {
        // Timer
        u16 timerPeriod;      // Timer period (from lookup table)
        u16 timerCounter;     // Current timer value
        
        // Shift register (LFSR)
        u16 shiftRegister;    // 15-bit shift register
        bool mode;            // Mode flag (short/long sequence)
        
        // Components
        Envelope envelope;
        LengthCounter lengthCounter;
        
        void reset();
        void clockTimer();
        void clockEnvelope() { envelope.clock(); }
        void clockLength() { lengthCounter.clock(); }
        
        u8 output() const;
        
        // Register writes
        void writeControl(u8 value);  // $400C
        void writePeriod(u8 value);   // $400E
        void writeLength(u8 value);   // $400F
        
        static const u16 NOISE_PERIOD_TABLE[16];
        static const u16 NOISE_PERIOD_TABLE_PAL[16];
    };

    // ============================================================
    // DMC (Delta Modulation Channel)
    // ============================================================
    struct DMCChannel {
        // Timer
        u16 timerPeriod;      // Rate timer period
        u16 timerCounter;     // Current timer value
        
        // Memory reader
        u16 sampleAddress;    // Current address ($C000-$FFFF range)
        u16 sampleLength;     // Current bytes remaining
        u16 addressStart;     // Starting address
        u16 lengthStart;      // Starting length
        u8 sampleBuffer;      // Sample buffer
        bool sampleBufferEmpty; // Buffer empty flag
        
        // Output unit
        u8 shiftRegister;     // 8-bit shift register
        u8 bitsRemaining;     // Bits remaining in shift register
        u8 outputLevel;       // Current output level (0-127)
        bool silenceFlag;     // Silence flag
        
        // Flags
        bool irqEnabled;      // IRQ enable
        bool loopFlag;        // Loop flag
        bool irqFlag;         // IRQ flag (set when sample finishes)
        
        // Reference to memory for DMA reads
        Memory* memory;
        
        void reset();
        void clockTimer();
        void startSample();
        void readSampleByte();
        
        u8 output() const { return outputLevel; }
        
        // Register writes
        void writeControl(u8 value);  // $4010
        void writeDirectLoad(u8 value); // $4011
        void writeAddress(u8 value);  // $4012
        void writeLength(u8 value);   // $4013
        
        static const u16 DMC_RATE_TABLE[16];
        static const u16 DMC_RATE_TABLE_PAL[16];
    };

    // ============================================================
    // Frame Counter
    // ============================================================
    struct FrameCounter {
        u32 cycleCounter;     // CPU cycles since last reset
        u8 step;              // Current step (0-4)
        bool mode;            // false = 4-step, true = 5-step
        bool irqInhibit;      // IRQ inhibit flag
        bool irqFlag;         // IRQ pending
        bool resetDelay;      // Delay reset by 3-4 cycles
        u8 resetCounter;      // Cycles until reset
        
        void reset();
        
        // Step timings (in CPU cycles)
        static const u32 STEP_CYCLES_4[4];
        static const u32 STEP_CYCLES_5[5];
    };

    // ============================================================
    // APU Main Class
    // ============================================================
    APU();
    ~APU() = default;

    void setCPU(CPU* cpu) { m_cpu = cpu; }
    void setMemory(Memory* memory);
    void setCartridge(Cartridge* cartridge) { m_cartridge = cartridge; }
    void setAudioDevice(AudioDevice* audioDevice) { m_audioDevice = audioDevice; }
    
    void reset();
    void step(u32 cpuCycles, double gameSpeed = 1.0);
    
    // Register access
    u8 readStatus();              // $4015 read
    void writeRegister(u16 address, u8 value);
    
    // Audio configuration
    void setSampleRate(u32 sampleRate);
    void setVolume(float volume) { m_volume = volume; }
    
    // Frame counter (for IRQ)
    bool irqPending() const { return m_frameCounter.irqFlag || m_dmc.irqFlag; }
    void clearFrameIRQ() { m_frameCounter.irqFlag = false; }
    void clearDMCIRQ() { m_dmc.irqFlag = false; }
    
    // Save/Load state
    void saveState(std::ofstream& file) const;
    void loadState(std::ifstream& file);

private:
    // Clock different APU components
    void clockTimers();           // Every CPU cycle
    void clockQuarterFrame();     // Envelopes and linear counter
    void clockHalfFrame();        // Length counters and sweep
    
    // Audio mixing and output
    float mix();                  // Mix all channels
    void outputSample(float sample);
    
    // Build mixer lookup tables
    void buildMixerTables();

    // Components
    CPU* m_cpu;
    Memory* m_memory;
    Cartridge* m_cartridge;
    AudioDevice* m_audioDevice;
    
    // Channels
    PulseChannel m_pulse1;
    PulseChannel m_pulse2;
    TriangleChannel m_triangle;
    NoiseChannel m_noise;
    DMCChannel m_dmc;
    
    // Frame counter
    FrameCounter m_frameCounter;
    
    // Cycle tracking
    u64 m_totalCycles;            // Total CPU cycles elapsed
    u64 m_lastFrameCycle;         // Cycle of last frame counter clock
    bool m_oddCycle;              // APU runs at half CPU rate for some things
    
    // Audio output
    u32 m_sampleRate;
    float m_volume;
    double m_sampleCounter;       // For sample rate conversion
    double m_cyclesPerSample;     // CPU cycles per audio sample
    
    // Audio buffer for batched output (stereo: L, R, L, R, ...)
    std::vector<float> m_sampleBuffer;
    static constexpr size_t SAMPLE_BUFFER_SIZE = 2048; // Smaller buffer for lower latency
    
    // Mixer lookup tables (for non-linear mixing)
    std::array<float, 31> m_pulseTable;
    std::array<float, 203> m_tndTable;
    
    // High-pass and low-pass filters for accurate audio
    float m_highPass1;
    float m_highPass2;
    float m_lowPass;
    
    // Filter coefficients
    static constexpr float HIGH_PASS_90HZ = 0.999835f;
    static constexpr float HIGH_PASS_440HZ = 0.996039f;
    static constexpr float LOW_PASS_14KHZ = 0.815686f;
};

} // namespace nes

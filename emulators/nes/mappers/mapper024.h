#pragma once

#include "../cartridge.h"

namespace nes {

// VRC6 Pulse Channel - Square wave with 8 duty cycle settings
class VRC6Pulse {
public:
    void reset();
    void writeReg(u16 addr, u8 value);
    void setFrequencyShift(u8 shift) { m_frequencyShift = shift; }
    void clock();
    u8 getVolume() const;
    
    // State save/load
    void saveState(std::ofstream& file) const;
    void loadState(std::ifstream& file);
    
private:
    u8 m_volume = 0;           // Volume (0-15)
    u8 m_dutyCycle = 0;        // Duty cycle (0-7)
    bool m_ignoreDuty = false; // Ignore duty (output constant volume)
    u16 m_frequency = 1;       // 12-bit frequency
    bool m_enabled = false;    // Channel enabled
    
    s32 m_timer = 1;           // Timer countdown
    u8 m_step = 0;             // Current step in duty cycle (0-15)
    u8 m_frequencyShift = 0;   // Frequency shift for speed control
};

// VRC6 Sawtooth Channel - Accumulating sawtooth waveform
class VRC6Saw {
public:
    void reset();
    void writeReg(u16 addr, u8 value);
    void setFrequencyShift(u8 shift) { m_frequencyShift = shift; }
    void clock();
    u8 getVolume() const;
    
    // State save/load
    void saveState(std::ofstream& file) const;
    void loadState(std::ifstream& file);
    
private:
    u8 m_accumulatorRate = 0;  // Rate added to accumulator
    u8 m_accumulator = 0;      // 8-bit accumulator
    u16 m_frequency = 1;       // 12-bit frequency
    bool m_enabled = false;    // Channel enabled
    
    s32 m_timer = 1;           // Timer countdown
    u8 m_step = 0;             // Current step (0-13)
    u8 m_frequencyShift = 0;   // Frequency shift for speed control
};

// VRC6 Audio subsystem - 2 pulse channels + 1 sawtooth
class VRC6Audio {
public:
    void reset();
    void writeRegister(u16 addr, u8 value);
    void clock();
    float getOutput() const;
    
    // State save/load
    void saveState(std::ofstream& file) const;
    void loadState(std::ifstream& file);
    
private:
    VRC6Pulse m_pulse1;
    VRC6Pulse m_pulse2;
    VRC6Saw m_saw;
    bool m_haltAudio = false;  // Halt all audio processing
    s32 m_lastOutput = 0;      // For delta-based output
};

// Mapper 24: VRC6a (Konami)
// Used by games like Akumajou Densetsu (Castlevania III Japan)
class Mapper024 : public Mapper {
public:
    Mapper024(Cartridge* cartridge);
    void reset() override;
    
    u8 cpuRead(u16 address) override;
    void cpuWrite(u16 address, u8 value) override;
    u8 readCHR(u16 address) override;
    void writeCHR(u16 address, u8 value) override;
    
    void scanlineCounter() override;
    
    // Expansion audio
    void clockAudio() override;
    float getAudioOutput() const override;
    bool hasExpansionAudio() const override { return true; }
    
    void saveState(std::ofstream& file) const override;
    void loadState(std::ifstream& file) override;
    
private:
    void updateBanks();
    void updateMirroring();
    void clockIRQ();
    
    // PRG banking
    u8 m_prgBank16k;           // 16KB bank at $8000-$BFFF
    u8 m_prgBank8k;            // 8KB bank at $C000-$DFFF
    u32 m_prgBankOffset[4];    // Calculated PRG offsets for 8KB banks
    
    // CHR banking
    u8 m_chrBank[8];           // 1KB CHR banks
    u32 m_chrBankOffset[8];    // Calculated CHR offsets
    
    // Banking mode (controls mirroring and PPU banking)
    u8 m_bankingMode;
    
    // IRQ (VRC-style CPU cycle counter)
    u8 m_irqLatch;             // IRQ reload value
    u8 m_irqCounter;           // IRQ counter
    bool m_irqEnable;          // IRQ enabled
    bool m_irqEnableOnAck;     // Enable IRQ after acknowledge
    bool m_irqCycleMode;       // true = cycle mode, false = scanline mode
    s16 m_irqPrescaler;        // Prescaler for scanline mode (counts PPU cycles)
    
    // VRC6 expansion audio
    VRC6Audio m_audio;
};

} // namespace nes


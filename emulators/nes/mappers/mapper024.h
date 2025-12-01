#pragma once

#include "../cartridge.h"

namespace nes {

// VRC6 Audio Pulse Channel
struct VRC6Pulse {
    u8 volume;          // 4-bit volume (0-15)
    u8 duty;            // 3-bit duty cycle (0-7)
    u16 period;         // 12-bit period
    u16 timer;          // Current timer counter
    u8 step;            // Current duty cycle step (0-15)
    bool enabled;       // Channel enabled
    bool mode;          // Mode flag (constant volume)
    
    void reset();
    void clockTimer();
    u8 output() const;
};

// VRC6 Audio Sawtooth Channel
struct VRC6Sawtooth {
    u8 accumRate;       // 6-bit accumulator rate
    u16 period;         // 12-bit period
    u16 timer;          // Current timer counter
    u8 accumulator;     // 8-bit accumulator
    u8 step;            // Step counter (0-13, reset to 0 on 14)
    bool enabled;       // Channel enabled
    
    void reset();
    void clockTimer();
    u8 output() const;
};

// Mapper 24: VRC6a (Konami with extra audio)
class Mapper024 : public Mapper {
public:
    Mapper024(Cartridge* cartridge);
    void reset() override;
    
    u8 cpuRead(u16 address) override;
    void cpuWrite(u16 address, u8 value) override;
    u8 readCHR(u16 address) override;
    void writeCHR(u16 address, u8 value) override;
    
    MirrorMode getMirrorMode() const override;
    void scanlineCounter() override;
    
    // VRC6 Audio
    void clockAudio() override;
    float getAudioOutput() const override;
    bool hasExpansionAudio() const override { return true; }
    
    void saveState(std::ofstream& file) const override;
    void loadState(std::ifstream& file) override;
    
private:
    void updateBanks();
    
    u8 m_prgBank16k;        // 16KB PRG bank at $8000
    u8 m_prgBank8k;         // 8KB PRG bank at $C000
    u8 m_chrBank[8];        // 1KB CHR banks
    MirrorMode m_mirrorMode;
    
    // IRQ (same as VRC4)
    u8 m_irqLatch;
    u8 m_irqCounter;
    u8 m_irqPrescaler;
    u16 m_irqPrescalerCounter;
    bool m_irqEnable;
    bool m_irqEnableOnAck;
    bool m_irqMode;
    
    u32 m_prgBankOffset[4];
    u32 m_chrBankOffset[8];
    
    // VRC6 Audio channels
    VRC6Pulse m_vrcPulse1;
    VRC6Pulse m_vrcPulse2;
    VRC6Sawtooth m_vrcSaw;
    bool m_audioHalt;       // Global halt flag
};

} // namespace nes


#pragma once

#include "../cartridge.h"

namespace nes {

// Mapper 19: Namco 163 (simplified, without expansion audio)
class Mapper019 : public Mapper {
public:
    explicit Mapper019(Cartridge* cartridge);
    
    void reset() override;
    
    u8 cpuRead(u16 address) override;
    void cpuWrite(u16 address, u8 value) override;
    
    u8 readCHR(u16 address) override;
    void writeCHR(u16 address, u8 value) override;
    
    void scanlineCounter() override {}
    void clockAudio() override; // Used to tick the IRQ timer every CPU cycle
    
    void saveState(std::ofstream& file) const override;
    void loadState(std::ifstream& file) override;
    
    bool hasExpansionAudio() const override { return false; }
    float getAudioOutput() const override { return 0.0f; }
    
private:
    void updateChrMapping(u8 bankIndex, u8 value);
    
    // PRG: three 8KB switchable banks at $8000/$A000/$C000, last bank fixed
    u8 m_prgBank[3];
    
    // CHR: eight 1KB banks, can point to CHR or CIRAM (when value >= 0xE0)
    u8 m_chrBank[8];
    bool m_chrUseCiram[8];
    
    // IRQ counter (15-bit + enable)
    u16 m_irqCounter;   // lower 15 bits used as counter
    bool m_irqEnable;   // enable flag (bit 15 in hardware)
};

} // namespace nes



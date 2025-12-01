#pragma once

#include "../cartridge.h"

namespace nes {

// Mapper 25: VRC4b / VRC4d (Konami) - Similar to 23 with different address lines
class Mapper025 : public Mapper {
public:
    Mapper025(Cartridge* cartridge);
    void reset() override;
    
    u8 cpuRead(u16 address) override;
    void cpuWrite(u16 address, u8 value) override;
    u8 readCHR(u16 address) override;
    void writeCHR(u16 address, u8 value) override;
    
    MirrorMode getMirrorMode() const override;
    void scanlineCounter() override;
    
    void saveState(std::ofstream& file) const override;
    void loadState(std::ifstream& file) override;
    
private:
    void updateBanks();
    
    u8 m_prgBank[2];
    u8 m_chrBank[8];
    u8 m_chrBankHigh[8];
    u8 m_prgSwapMode;
    MirrorMode m_mirrorMode;
    
    // IRQ
    u8 m_irqLatch;
    u8 m_irqCounter;
    u8 m_irqPrescaler;
    u16 m_irqPrescalerCounter;
    bool m_irqEnable;
    bool m_irqEnableOnAck;
    bool m_irqMode;
    
    u32 m_prgBankOffset[4];
    u32 m_chrBankOffset[8];
};

} // namespace nes


#pragma once

#include "../cartridge.h"

namespace nes {

// Mapper 23: VRC2b / VRC4e / VRC4f (Konami)
class Mapper023 : public Mapper {
public:
    Mapper023(Cartridge* cartridge);
    void reset() override;
    
    u8 cpuRead(u16 address) override;
    void cpuWrite(u16 address, u8 value) override;
    u8 readCHR(u16 address) override;
    void writeCHR(u16 address, u8 value) override;
    
    void scanlineCounter() override;
    
    void saveState(Buffer* buf) override;
    void loadState(Buffer* buf) override;
    
private:
    template <typename Visit> void visitState(Visit visit);

    void updateBanks();
    
    u8 m_prgBank[2];        // 8KB PRG banks
    u8 m_chrBank[8];        // 1KB CHR banks (low nibbles)
    u8 m_chrBankHigh[8];    // 1KB CHR banks (high nibbles)
    u8 m_prgSwapMode;       // PRG swap mode
    
    // IRQ
    u8 m_irqLatch;
    u8 m_irqCounter;
    u8 m_irqPrescaler;
    u16 m_irqPrescalerCounter;
    bool m_irqEnable;
    bool m_irqEnableOnAck;
    bool m_irqMode;         // 0 = scanline, 1 = cycle
    
    u32 m_prgBankOffset[4];
    u32 m_chrBankOffset[8];
};

} // namespace nes


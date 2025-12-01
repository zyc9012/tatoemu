#pragma once

#include "../cartridge.h"

namespace nes {

// Mapper 4: MMC3 (TxROM)
class Mapper004 : public Mapper {
public:
    Mapper004(Cartridge* cartridge);
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
    
    u8 m_bankSelect;
    u8 m_bankData[8];
    
    u8 m_irqLatch;
    u8 m_irqCounter;
    bool m_irqEnable;
    bool m_irqReload;
    
    MirrorMode m_mirrorMode;
    bool m_prgRamEnable;
    
    u32 m_prgBankOffset[4];
    u32 m_chrBankOffset[8];
};

} // namespace nes


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
    
    void scanlineCounter() override;
    
    void saveState(Buffer* buf) override;
    void loadState(Buffer* buf) override;
    
protected:
    // Defined here rather than in the .cpp because Mapper074 walks it too.
    template <typename Visit> void visitState(Visit visit) {
        Mapper::visitState(visit);
        visit(m_bankSelect);
        visit(m_bankData);
        visit(m_irqLatch);
        visit(m_irqCounter);
        visit(m_irqEnable);
        visit(m_irqReload);
        visit(m_prgRamEnable);
    }

    virtual void updateBanks();
    
    u8 m_bankSelect;
    u8 m_bankData[8];
    
    u8 m_irqLatch;
    u8 m_irqCounter;
    bool m_irqEnable;
    bool m_irqReload;
    
    bool m_prgRamEnable;
    
    u32 m_prgBankOffset[4];
    u32 m_chrBankOffset[8];
};

} // namespace nes


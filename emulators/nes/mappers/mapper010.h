#pragma once

#include "../cartridge.h"

namespace nes {

// Mapper 10: MMC4 (FxROM)
class Mapper010 : public Mapper {
public:
    Mapper010(Cartridge* cartridge);
    void reset() override;
    
    u8 cpuRead(u16 address) override;
    void cpuWrite(u16 address, u8 value) override;
    u8 readCHR(u16 address) override;
    void writeCHR(u16 address, u8 value) override;
    
    void saveState(Buffer* buf) override;
    void loadState(Buffer* buf) override;
    
private:
    template <typename Visit> void visitState(Visit visit);

    u8 m_prgBank;
    u8 m_chrBank0FD;
    u8 m_chrBank0FE;
    u8 m_chrBank1FD;
    u8 m_chrBank1FE;
    u8 m_latch0;
    u8 m_latch1;
};

} // namespace nes


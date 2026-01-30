#pragma once

#include "../cartridge.h"

namespace nes {

// Mapper 1: MMC1 (SxROM)
class Mapper001 : public Mapper {
public:
    Mapper001(Cartridge* cartridge);
    void reset() override;
    
    u8 cpuRead(u16 address) override;
    void cpuWrite(u16 address, u8 value) override;
    u8 readCHR(u16 address) override;
    void writeCHR(u16 address, u8 value) override;
    
    MirrorMode getMirrorMode() const override;
    
    void saveState(Buffer* buf) override;
    void loadState(Buffer* buf) override;
    
private:
    void updateBanks();
    
    u8 m_shiftRegister;
    u8 m_shiftCount;
    u64 m_lastWriteCycle;  // For filtering consecutive writes (RMW instructions)
    
    u8 m_control;       // $8000-$9FFF
    u8 m_chrBank0;      // $A000-$BFFF
    u8 m_chrBank1;      // $C000-$DFFF
    u8 m_prgBank;       // $E000-$FFFF

    u16 m_lastChrReg;   // Last CHR register written to (for SUROM page selection)
    
    u32 m_prgBankOffset[2];
    u32 m_chrBankOffset[2];
};

} // namespace nes


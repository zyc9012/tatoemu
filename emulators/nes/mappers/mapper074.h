#pragma once

#include "mapper004.h"

namespace nes {

// Mapper 74: MMC3 variant with CHR RAM (Taiwan MMC3)
// CHR banks 0x08-0x09 use 2KB CHR RAM instead of CHR ROM
class Mapper074 : public Mapper004 {
public:
    Mapper074(Cartridge* cartridge);
    void reset() override;
    
    u8 readCHR(u16 address) override;
    void writeCHR(u16 address, u8 value) override;
    
    void saveState(Buffer* buf) override;
    void loadState(Buffer* buf) override;
    
protected:
    template <typename Visit> void visitState(Visit visit);

    void updateBanks() override;
    bool isChrRamBank(u8 bank) const;
    
    bool m_chrRamBank[8];  // Track which CHR banks use RAM
    u8 m_chrBankValue[8];  // Store the bank value for CHR RAM mapping
    
    // 2KB CHR RAM (for banks 0x08-0x09)
    u8 m_chrRam[0x800];
};

} // namespace nes

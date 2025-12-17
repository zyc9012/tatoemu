#pragma once

#include "../cartridge.h"

namespace nes {

// Mapper 9: MMC2 (PxROM)
class Mapper009 : public Mapper {
public:
    Mapper009(Cartridge* cartridge);
    void reset() override;
    
    u8 cpuRead(u16 address) override;
    void cpuWrite(u16 address, u8 value) override;
    u8 readCHR(u16 address) override;
    void writeCHR(u16 address, u8 value) override;
    
    MirrorMode getMirrorMode() const override;
    
    void saveState(std::ofstream& file) const override;
    void loadState(std::ifstream& file) override;
    
private:
    void updateBanks();
    
    u8 m_prgBank;
    u8 m_leftChrPage[2];   // Two pages for left bank (4KB at $0000)
    u8 m_rightChrPage[2];  // Two pages for right bank (4KB at $1000)
    u8 m_leftLatch;        // 0 or 1
    u8 m_rightLatch;       // 0 or 1
    MirrorMode m_mirrorMode;
    
    u32 m_prgBankOffset[4];
    u32 m_chrBankOffset[2];
};

} // namespace nes

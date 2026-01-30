#pragma once

#include "../cartridge.h"

namespace nes {

// Mapper 69: Sunsoft FME-7
class Mapper069 : public Mapper {
public:
    Mapper069(Cartridge* cartridge);
    void reset() override;
    
    u8 cpuRead(u16 address) override;
    void cpuWrite(u16 address, u8 value) override;
    u8 readCHR(u16 address) override;
    void writeCHR(u16 address, u8 value) override;
    
    void clockAudio() override;
    
    void saveState(Buffer* buf) override;
    void loadState(Buffer* buf) override;
    
private:
    void updateBanks();
    void updateWorkRam();
    
    u8 m_command;           // Command register ($8000)
    u8 m_chrBanks[8];       // CHR bank registers (commands 0-7)
    u8 m_prgBanks[3];       // PRG bank registers (commands 9-A-B)
    u8 m_workRamValue;      // Work RAM register (command 8)
    bool m_irqEnabled;
    bool m_irqCounterEnabled;
    u16 m_irqCounter;
    
    u32 m_prgBankOffset[4];
    u32 m_chrBankOffset[8];
    
    // Work RAM (8KB, can be mapped to $6000-$7FFF)
    std::vector<u8> m_workRam;
};

} // namespace nes

#pragma once

#include "../cartridge.h"

namespace nes {

// Mapper 163: Nanjing
// Chinese unlicensed mapper with register read capability and CHR switching
class Mapper163 : public Mapper {
public:
    Mapper163(Cartridge* cartridge);
    void reset() override;
    
    u8 cpuRead(u16 address) override;
    void cpuWrite(u16 address, u8 value) override;
    u8 readCHR(u16 address) override;
    void writeCHR(u16 address, u8 value) override;
    
    void saveState(Buffer* buf) override;
    void loadState(Buffer* buf) override;
    
private:
    void updateState();
    
    u8 m_registers[5];
    bool m_toggle;
    bool m_autoSwitchCHR;
    
    u8 m_prgBank;
    u8 m_chrBank0;
    u8 m_chrBank1;
};

} // namespace nes

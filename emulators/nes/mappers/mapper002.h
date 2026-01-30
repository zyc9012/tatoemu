#pragma once

#include "../cartridge.h"

namespace nes {

// Mapper 2: UxROM
class Mapper002 : public Mapper {
public:
    Mapper002(Cartridge* cartridge);
    void reset() override;
    
    u8 cpuRead(u16 address) override;
    void cpuWrite(u16 address, u8 value) override;
    u8 readCHR(u16 address) override;
    void writeCHR(u16 address, u8 value) override;
    
    void saveState(Buffer* buf) override;
    void loadState(Buffer* buf) override;
    
private:
    u8 m_prgBank;
};

} // namespace nes


#pragma once

#include "../cartridge.h"

namespace nes {

// Mapper 164: Waixing
class Mapper164 : public Mapper {
public:
    Mapper164(Cartridge* cartridge);
    void reset() override;

    u8 cpuRead(u16 address) override;
    void cpuWrite(u16 address, u8 value) override;
    u8 readCHR(u16 address) override;
    void writeCHR(u16 address, u8 value) override;

    void saveState(Buffer* buf) override;
    void loadState(Buffer* buf) override;

private:
    u8 m_prgBank;
    u32 m_prgBankOffset;
    u32 m_chrBankOffset;
};

} // namespace nes
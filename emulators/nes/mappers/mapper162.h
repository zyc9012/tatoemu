#pragma once

#include "../cartridge.h"

namespace nes {

// Mapper 162: Waixing
class Mapper162 : public Mapper {
public:
    Mapper162(Cartridge* cartridge);
    void reset() override;

    u8 cpuRead(u16 address) override;
    void cpuWrite(u16 address, u8 value) override;
    u8 readCHR(u16 address) override;
    void writeCHR(u16 address, u8 value) override;

    void saveState(Buffer* buf) override;
    void loadState(Buffer* buf) override;

private:
    template <typename Visit> void visitState(Visit visit);

    void updateState();

    u8 m_regs[4];
    u32 m_prgBankOffset;
    u32 m_chrBankOffset;
};

} // namespace nes
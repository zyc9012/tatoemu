#pragma once

#include "../cartridge.h"

namespace nes {

// Mapper 178: Waixing
class Mapper178 : public Mapper {
public:
    Mapper178(Cartridge* cartridge);
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
    u8 m_prgLow; // Low 4 bits of PRG bank
    u8 m_prgHigh; // High bits of PRG bank
};

} // namespace nes
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

    MirrorMode getMirrorMode() const override;

    void saveState(std::ofstream& file) const override;
    void loadState(std::ifstream& file) override;

private:
    u8 m_prgBank;
    u8 m_prgLow; // Low 4 bits of PRG bank
    u8 m_prgHigh; // High bits of PRG bank
    MirrorMode m_mirrorMode;
};

} // namespace nes
#pragma once

#include "../cartridge.h"

namespace nes {

// Mapper 0: NROM - No mapper (simple ROM)
class Mapper000 : public Mapper {
public:
    Mapper000(Cartridge* cartridge);
    void reset() override;
    
    u8 cpuRead(u16 address) override;
    void cpuWrite(u16 address, u8 value) override;
    u8 readCHR(u16 address) override;
    void writeCHR(u16 address, u8 value) override;
    
    void saveState(std::ofstream& file) const override;
    void loadState(std::ifstream& file) override;
};

} // namespace nes


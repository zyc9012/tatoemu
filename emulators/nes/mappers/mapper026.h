#pragma once

#include "mapper024.h"

namespace nes {

// Mapper 26: VRC6b (same logic as VRC6a but CPU A0/A1 are swapped)
class Mapper026 : public Mapper024 {
public:
    explicit Mapper026(Cartridge* cartridge) : Mapper024(cartridge) {}

    // Only address decoding differs from mapper 24
    void cpuWrite(u16 address, u8 value) override;
};

} // namespace nes

#include "mapper026.h"

namespace nes {

// Mapper 26 uses the same registers as mapper 24, but CPU A0/A1 are swapped.
// We translate the write address then forward to the VRC6a implementation.
void Mapper026::cpuWrite(u16 address, u8 value) {
    // PRG RAM region behaves the same, let base handle it directly
    if (address >= 0x6000 && address < 0x8000) {
        Mapper024::cpuWrite(address, value);
        return;
    }

    // Swap A0 and A1 while keeping other bits intact
    u16 swapped = (address & 0xFFF0)
                | ((address & 0x0001) << 1)
                | ((address & 0x0002) >> 1)
                | (address & 0x000C);

    Mapper024::cpuWrite(swapped, value);
}

} // namespace nes

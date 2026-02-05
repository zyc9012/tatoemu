// ============================================================================
// Game specific protections
// ============================================================================

#include "memory.h"
#include "cartridge.h"
#include <memory>
#include <vector>
#include <string_view>
#include "../types.h"

namespace neogeo {

std::unique_ptr<MemoryHijacker> initProtections(Memory* memory, Cartridge* cartridge);

// mslugx
class MslugxMemory : public MemoryHijacker {
public:
    MslugxMemory(Memory* memory);

    bool read16(u32 address, u16& ret) override;
    bool write16(u32 address, u16 value) override;

private:
    Memory* m_memory;
    u16 command;
    u16 counter;
};

// kof98
void decryptKof98(std::vector<u8>& rom);

class Kof98Memory : public MemoryHijacker {
public:
    Kof98Memory(Cartridge* cartridge);

    bool write16(u32 address, u16 value) override;

private:
    Cartridge* m_cartridge;
};

} // namespace neogeo

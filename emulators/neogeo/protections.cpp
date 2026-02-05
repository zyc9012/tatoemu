#include "protections.h"

namespace neogeo {

std::unique_ptr<MemoryHijacker> initProtections(Memory* memory, Cartridge* cartridge) {
    auto romSetName = std::string_view(cartridge->getGameInfo()->romSetName);
    if (romSetName == "mslugx") {
        return std::make_unique<MslugxMemory>(memory);
    } else if (romSetName == "kof98") {
        return std::make_unique<Kof98Memory>(cartridge);
    }
    return nullptr;
}

// ============================================================================
// Metal Slug X
// ============================================================================
MslugxMemory::MslugxMemory(Memory* memory) : MemoryHijacker() {
    m_memory = memory;
    command = 0;
    counter = 0;
}

bool MslugxMemory::read16(u32 address, u16& ret) {
    if (address == 0x2FFFE8) {
        ret = 0;
        switch (command) {
            case 0x0001:
                ret = m_memory->read8(0xDEDD2 + ((counter >> 3) & 0xfff)) >> (~counter & 0x07);
                ret &= 1;
                counter++;
                break;
            case 0x0FFF:
                u32 select = m_memory->read16(0x10F00A) - 1;
                ret = m_memory->read8(0xDEDD2 + ((select >> 3) & 0x0FFF)) >> (~select & 0x07);
                ret &= 1;
                break;
        }
        return true;
    }

    return false;
}

bool MslugxMemory::write16(u32 address, u16 value) {
    if ((address & 0xFFFFF0) == 0x2FFFE0) {
        switch (address) {
            case 0x2FFFE0:
                command = 0;
                break;

            case 0x2FFFE2:
            case 0x2FFFE4:
                command |= value;
                break;

            case 0x2FFFE6:
                break;

            case 0x2FFFEA:
                counter = 0;
                break;
        }
        return true;
    }

    return false;
}

// ============================================================================
// The King of Fighters '98
// ============================================================================
void decryptKof98(std::vector<u8>& rom)
{
    std::vector<u8> temp(0x200000);

    for (u32 i = 0; i < 0x100000; i++) {
        u32 j = i;

        if ((i & 0x0000fc) == 0x000000) j ^= 0x000100;
        if ((i & 0x0c0000) != 0x080000) j ^= 0x000100;
        if ((i & 0x0c0008) == 0x080008) j ^= 0x000100;
        if ((i & 0x0c00fe) == 0x080000) j ^= 0x000100;
        if ((i & 0x0c0002) == 0x080002) j ^= 0x000100;
        if ((i & 0x100000) == 0x100000) j ^= 0x000102;
        if ((i & 0x000002) == 0x000002) j ^= 0x100002;
        if ((i & 0x000008) == 0x000008) j ^= 0x100002;
        
        temp[i] = rom[j];
    }

    memmove(rom.data() + 0x000800, temp.data() + 0x000800, 0x200000 - 0x000800);
    memmove(rom.data() + 0x100000, rom.data() + 0x200000, 0x400000);
}

Kof98Memory::Kof98Memory(Cartridge* cartridge) : m_cartridge(cartridge) {}

bool Kof98Memory::write16(u32 address, u16 value) {
    if (address == 0x20AAAA) {
        switch (value) {
            case 0x0090:
                m_cartridge->writeVectorTable8(0x000100, 0x00);
                m_cartridge->writeVectorTable8(0x000101, 0xC2);
                m_cartridge->writeVectorTable8(0x000102, 0x00);
                m_cartridge->writeVectorTable8(0x000103, 0xFD);
                break;
            case 0x00F0:
                m_cartridge->writeVectorTable8(0x000100, 0x4E);
                m_cartridge->writeVectorTable8(0x000101, 0x45);
                m_cartridge->writeVectorTable8(0x000102, 0x4F);
                m_cartridge->writeVectorTable8(0x000103, 0x2D);
                break;
        }
        return true;
    }
    return false;
}

} // namespace neogeo
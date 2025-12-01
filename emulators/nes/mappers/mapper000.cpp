#include "mapper000.h"
#include "../consts.h"

namespace nes {

Mapper000::Mapper000(Cartridge* cartridge) : Mapper(cartridge) {
}

void Mapper000::reset() {
}

u8 Mapper000::cpuRead(u16 address) {
    if (address >= 0x6000 && address < 0x8000) {
        // PRG RAM
        return m_cartridge->getPRGRAM()[address & 0x1FFF];
    } else if (address >= 0x8000) {
        // PRG ROM - mirror if only 16KB
        const auto& prg = m_cartridge->getPRG();
        return prg[(address - 0x8000) % prg.size()];
    }
    return 0;
}

void Mapper000::cpuWrite(u16 address, u8 value) {
    if (address >= 0x6000 && address < 0x8000) {
        // PRG RAM
        m_cartridge->getPRGRAM()[address & 0x1FFF] = value;
    }
    // ROM writes are ignored
}

u8 Mapper000::readCHR(u16 address) {
    return m_cartridge->getCHR()[address & 0x1FFF];
}

void Mapper000::writeCHR(u16 address, u8 value) {
    // CHR RAM (if no CHR ROM)
    if (m_cartridge->getCHR().size() == CHR_ROM_BANK_SIZE) {
        m_cartridge->getCHR()[address & 0x1FFF] = value;
    }
}

void Mapper000::saveState(std::ofstream& file) const {
    (void)file;  // No mapper state
}

void Mapper000::loadState(std::ifstream& file) {
    (void)file;  // No mapper state
}

} // namespace nes


#include "mapper003.h"
#include "../consts.h"

namespace nes {

Mapper003::Mapper003(Cartridge* cartridge)
    : Mapper(cartridge) {
}

void Mapper003::reset() {
    m_chrBank = 0;
}

u8 Mapper003::cpuRead(u16 address) {
    if (address >= 0x6000 && address < 0x8000) {
        return m_cartridge->getPRGRAM()[address & 0x1FFF];
    } else if (address >= 0x8000) {
        const auto& prg = m_cartridge->getPRG();
        return prg[(address - 0x8000) % prg.size()];
    }
    return 0;
}

void Mapper003::cpuWrite(u16 address, u8 value) {
    if (address >= 0x6000 && address < 0x8000) {
        m_cartridge->getPRGRAM()[address & 0x1FFF] = value;
    } else if (address >= 0x8000) {
        m_chrBank = value & 0x03;  // Usually only 2 bits used
    }
}

u8 Mapper003::readCHR(u16 address) {
    const auto& chr = m_cartridge->getCHR();
    u32 offset = (m_chrBank % (chr.size() / 0x2000)) * 0x2000;
    return chr[offset + (address & 0x1FFF)];
}

void Mapper003::writeCHR(u16 address, u8 value) {
    (void)address;
    (void)value;
    // CHR ROM - ignore writes
}

void Mapper003::saveState(std::ofstream& file) const {
    file.write(reinterpret_cast<const char*>(&m_chrBank), sizeof(m_chrBank));
}

void Mapper003::loadState(std::ifstream& file) {
    file.read(reinterpret_cast<char*>(&m_chrBank), sizeof(m_chrBank));
}

} // namespace nes


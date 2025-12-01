#include "mapper002.h"
#include "../consts.h"

namespace nes {

Mapper002::Mapper002(Cartridge* cartridge)
    : Mapper(cartridge)
    , m_prgBank(0) {
}

void Mapper002::reset() {
    m_prgBank = 0;
}

u8 Mapper002::cpuRead(u16 address) {
    if (address >= 0x6000 && address < 0x8000) {
        return m_cartridge->getPRGRAM()[address & 0x1FFF];
    } else if (address >= 0x8000 && address < 0xC000) {
        // Switchable bank
        const auto& prg = m_cartridge->getPRG();
        u32 offset = (m_prgBank % (prg.size() / 0x4000)) * 0x4000;
        return prg[offset + (address & 0x3FFF)];
    } else if (address >= 0xC000) {
        // Fixed to last bank
        const auto& prg = m_cartridge->getPRG();
        return prg[prg.size() - 0x4000 + (address & 0x3FFF)];
    }
    return 0;
}

void Mapper002::cpuWrite(u16 address, u8 value) {
    if (address >= 0x6000 && address < 0x8000) {
        m_cartridge->getPRGRAM()[address & 0x1FFF] = value;
    } else if (address >= 0x8000) {
        m_prgBank = value;
    }
}

u8 Mapper002::readCHR(u16 address) {
    return m_cartridge->getCHR()[address & 0x1FFF];
}

void Mapper002::writeCHR(u16 address, u8 value) {
    m_cartridge->getCHR()[address & 0x1FFF] = value;
}

void Mapper002::saveState(std::ofstream& file) const {
    file.write(reinterpret_cast<const char*>(&m_prgBank), sizeof(m_prgBank));
}

void Mapper002::loadState(std::ifstream& file) {
    file.read(reinterpret_cast<char*>(&m_prgBank), sizeof(m_prgBank));
}

} // namespace nes


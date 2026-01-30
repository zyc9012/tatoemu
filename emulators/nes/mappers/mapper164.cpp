#include "mapper164.h"
#include "../consts.h"

namespace nes {

Mapper164::Mapper164(Cartridge* cartridge)
    : Mapper(cartridge) {
}

void Mapper164::reset() {
    Mapper::reset();
    m_prgBank = 0x0F;
    const auto& prg = m_cartridge->getPRG();
    u32 prgSize = prg.size();
    m_prgBankOffset = (m_prgBank % (prgSize / 0x8000)) * 0x8000;
    m_chrBankOffset = 0;
}

u8 Mapper164::cpuRead(u16 address) {
    if (address >= 0x6000 && address < 0x8000) {
        return m_cartridge->getPRGRAM()[address & 0x1FFF];
    } else if (address >= 0x8000) {
        const auto& prg = m_cartridge->getPRG();
        return prg[m_prgBankOffset + (address & 0x7FFF)];
    }
    return 0;
}

void Mapper164::cpuWrite(u16 address, u8 value) {
    if (address >= 0x6000 && address < 0x8000) {
        m_cartridge->getPRGRAM()[address & 0x1FFF] = value;
    } else if (address >= 0x5000 && address < 0x6000) {
        switch (address & 0x7300) {
            case 0x5000:
                m_prgBank = (m_prgBank & 0xF0) | (value & 0x0F);
                break;
            case 0x5100:
                m_prgBank = (m_prgBank & 0x0F) | ((value & 0x0F) << 4);
                break;
        }

        const auto& prg = m_cartridge->getPRG();
        u32 prgSize = prg.size();
        m_prgBankOffset = (m_prgBank % (prgSize / 0x8000)) * 0x8000;
    }
}

u8 Mapper164::readCHR(u16 address) {
    const auto& chr = m_cartridge->getCHR();
    return chr[m_chrBankOffset + (address & 0x1FFF)];
}

void Mapper164::writeCHR(u16 address, u8 value) {
    // CHR RAM only
    m_cartridge->getCHR()[m_chrBankOffset + (address & 0x1FFF)] = value;
}

void Mapper164::saveState(Buffer* buf) {
    Mapper::saveState(buf);
    buffer_write(buf, &m_prgBank, sizeof(m_prgBank));
    buffer_write(buf, &m_prgBankOffset, sizeof(m_prgBankOffset));
    buffer_write(buf, &m_chrBankOffset, sizeof(m_chrBankOffset));
}

void Mapper164::loadState(Buffer* buf) {
    Mapper::loadState(buf);
    buffer_read(buf, &m_prgBank, sizeof(m_prgBank));
    buffer_read(buf, &m_prgBankOffset, sizeof(m_prgBankOffset));
    buffer_read(buf, &m_chrBankOffset, sizeof(m_chrBankOffset));
}

} // namespace nes
#include "mapper178.h"
#include "../consts.h"

namespace nes {

Mapper178::Mapper178(Cartridge* cartridge)
    : Mapper(cartridge) {
}

void Mapper178::reset() {
    Mapper::reset();
    m_prgBank = 0;
    m_prgLow = 0;
    m_prgHigh = 0;
}

u8 Mapper178::cpuRead(u16 address) {
    if (address >= 0x6000 && address < 0x8000) {
        return m_cartridge->getPRGRAM()[address & 0x1FFF];
    } else if (address >= 0x8000) {
        const auto& prg = m_cartridge->getPRG();
        u32 prgSize = prg.size();
        u32 bank = m_prgBank % (prgSize / 0x8000);
        u32 offset = bank * 0x8000;
        return prg[offset + (address & 0x7FFF)];
    }
    return 0;
}

void Mapper178::cpuWrite(u16 address, u8 value) {
    if (address >= 0x6000 && address < 0x8000) {
        m_cartridge->getPRGRAM()[address & 0x1FFF] = value;
    } else if (address >= 0x4800 && address <= 0x4802) {
        switch (address & 0x03) {
            case 0: // $4800 - Mirroring control
                m_mirrorMode = (value & 0x01) ? MirrorMode::HORIZONTAL : MirrorMode::VERTICAL;
                break;
            case 1: // $4801 - PRG bank low
                m_prgLow = (value >> 1) & 0x0F;
                m_prgBank = m_prgLow + (m_prgHigh << 2);
                break;
            case 2: // $4802 - PRG bank high
                m_prgHigh = value;
                // PRG bank will be updated when $4801 is next written
                break;
        }
    }
}

u8 Mapper178::readCHR(u16 address) {
    return m_cartridge->getCHR()[address & 0x1FFF];
}

void Mapper178::writeCHR(u16 address, u8 value) {
    m_cartridge->getCHR()[address & 0x1FFF] = value;
}

void Mapper178::saveState(Buffer* buf) {
    Mapper::saveState(buf);
    buffer_write(buf, &m_prgBank, sizeof(m_prgBank));
    buffer_write(buf, &m_prgLow, sizeof(m_prgLow));
    buffer_write(buf, &m_prgHigh, sizeof(m_prgHigh));
}

void Mapper178::loadState(Buffer* buf) {
    Mapper::loadState(buf);
    buffer_read(buf, &m_prgBank, sizeof(m_prgBank));
    buffer_read(buf, &m_prgLow, sizeof(m_prgLow));
    buffer_read(buf, &m_prgHigh, sizeof(m_prgHigh));
}

} // namespace nes
#include "mapper162.h"
#include "../consts.h"

namespace nes {

Mapper162::Mapper162(Cartridge* cartridge)
    : Mapper(cartridge) {
}

void Mapper162::reset() {
    Mapper::reset();
    m_prgBankOffset = 0;
    m_chrBankOffset = 0;
    m_regs[0] = 3;
    m_regs[1] = 0;
    m_regs[2] = 0;
    m_regs[3] = 7;
    updateState();
}

void Mapper162::updateState() {
    const auto& prg = m_cartridge->getPRG();

    u32 prgSize = prg.size();

    // PRG banking logic based on regs[3] & 0x5
    u32 prgBank;
    switch (m_regs[3] & 0x5) {
        case 0:
            prgBank = (m_regs[0] & 0x0C) | (m_regs[1] & 0x02) | ((m_regs[2] & 0x0F) << 4);
            break;
        case 1:
            prgBank = (m_regs[0] & 0x0C) | ((m_regs[2] & 0x0F) << 4);
            break;
        case 4:
            prgBank = (m_regs[0] & 0x0E) | ((m_regs[1] >> 1) & 0x01) | ((m_regs[2] & 0x0F) << 4);
            break;
        case 5:
            prgBank = (m_regs[0] & 0x0F) | ((m_regs[2] & 0x0F) << 4);
            break;
        default:
            prgBank = 0;
            break;
    }

    // 32KB PRG banking
    m_prgBankOffset = (prgBank % (prgSize / 0x8000)) * 0x8000;

    // CHR is fixed to bank 0
    m_chrBankOffset = 0;
}

u8 Mapper162::cpuRead(u16 address) {
    if (address >= 0x6000 && address < 0x8000) {
        return m_cartridge->getPRGRAM()[address & 0x1FFF];
    } else if (address >= 0x8000) {
        const auto& prg = m_cartridge->getPRG();
        return prg[m_prgBankOffset + (address & 0x7FFF)];
    }
    return 0;
}

void Mapper162::cpuWrite(u16 address, u8 value) {
    if (address >= 0x6000 && address < 0x8000) {
        m_cartridge->getPRGRAM()[address & 0x1FFF] = value;
    } else if (address >= 0x5000 && address < 0x6000) {
        // Register write
        m_regs[(address >> 8) & 0x03] = value;
        updateState();
    }
}

u8 Mapper162::readCHR(u16 address) {
    const auto& chr = m_cartridge->getCHR();
    return chr[m_chrBankOffset + (address & 0x1FFF)];
}

void Mapper162::writeCHR(u16 address, u8 value) {
    // CHR RAM only
    m_cartridge->getCHR()[m_chrBankOffset + (address & 0x1FFF)] = value;
}

void Mapper162::saveState(Buffer* buf) {
    Mapper::saveState(buf);
    buffer_write(buf, m_regs, sizeof(m_regs));
    buffer_write(buf, &m_prgBankOffset, sizeof(m_prgBankOffset));
    buffer_write(buf, &m_chrBankOffset, sizeof(m_chrBankOffset));
}

void Mapper162::loadState(Buffer* buf) {
    Mapper::loadState(buf);
    buffer_read(buf, m_regs, sizeof(m_regs));
    buffer_read(buf, &m_prgBankOffset, sizeof(m_prgBankOffset));
    buffer_read(buf, &m_chrBankOffset, sizeof(m_chrBankOffset));
    updateState();
}

} // namespace nes
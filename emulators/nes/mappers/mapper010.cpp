#include "mapper010.h"
#include "../consts.h"

namespace nes {

Mapper010::Mapper010(Cartridge* cartridge)
    : Mapper(cartridge) {
}

void Mapper010::reset() {
    Mapper::reset();
    m_prgBank = 0;
    m_chrBank0FD = 0;
    m_chrBank0FE = 0;
    m_chrBank1FD = 0;
    m_chrBank1FE = 0;
    m_latch0 = 0xFE;
    m_latch1 = 0xFE;
}

u8 Mapper010::cpuRead(u16 address) {
    if (address >= 0x6000 && address < 0x8000) {
        return m_cartridge->getPRGRAM()[address & 0x1FFF];
    } else if (address >= 0x8000 && address < 0xC000) {
        const auto& prg = m_cartridge->getPRG();
        u32 offset = (m_prgBank % (prg.size() / 0x4000)) * 0x4000;
        return prg[offset + (address & 0x3FFF)];
    } else if (address >= 0xC000) {
        const auto& prg = m_cartridge->getPRG();
        return prg[prg.size() - 0x4000 + (address & 0x3FFF)];
    }
    return 0;
}

void Mapper010::cpuWrite(u16 address, u8 value) {
    if (address >= 0x6000 && address < 0x8000) {
        m_cartridge->getPRGRAM()[address & 0x1FFF] = value;
    } else if (address >= 0xA000 && address < 0xB000) {
        // PRG bank select
        m_prgBank = value & 0x0F;
    } else if (address >= 0xB000 && address < 0xC000) {
        // CHR bank 0 select ($FD)
        m_chrBank0FD = value;
    } else if (address >= 0xC000 && address < 0xD000) {
        // CHR bank 0 select ($FE)
        m_chrBank0FE = value;
    } else if (address >= 0xD000 && address < 0xE000) {
        // CHR bank 1 select ($FD)
        m_chrBank1FD = value;
    } else if (address >= 0xE000 && address < 0xF000) {
        // CHR bank 1 select ($FE)
        m_chrBank1FE = value;
    } else if (address >= 0xF000) {
        // Mirroring
        m_mirrorMode = (value & 0x01) ? MirrorMode::HORIZONTAL : MirrorMode::VERTICAL;
    }
}

u8 Mapper010::readCHR(u16 address) {
    const auto& chr = m_cartridge->getCHR();
    u32 chrBanks = chr.size() / 0x1000;
    
    if (address < 0x1000) {
        u8 bank = (m_latch0 == 0xFD) ? m_chrBank0FD : m_chrBank0FE;
        u32 offset = (bank % chrBanks) * 0x1000;
        u8 value = chr[offset + (address & 0x0FFF)];
        
        // Update latch based on tile fetched
        if ((address & 0x0FF8) == 0x0FD8) {
            m_latch0 = 0xFD;
        } else if ((address & 0x0FF8) == 0x0FE8) {
            m_latch0 = 0xFE;
        }
        
        return value;
    } else {
        u8 bank = (m_latch1 == 0xFD) ? m_chrBank1FD : m_chrBank1FE;
        u32 offset = (bank % chrBanks) * 0x1000;
        u8 value = chr[offset + (address & 0x0FFF)];
        
        // Update latch based on tile fetched (addresses relative to bank)
        u16 bankAddr = address & 0x0FFF;
        if ((bankAddr & 0x0FF8) == 0x0FD8) {
            m_latch1 = 0xFD;
        } else if ((bankAddr & 0x0FF8) == 0x0FE8) {
            m_latch1 = 0xFE;
        }
        
        return value;
    }
}

void Mapper010::writeCHR(u16 address, u8 value) {
    (void)address;
    (void)value;
    // CHR ROM - ignore writes
}

template <typename Visit>
void Mapper010::visitState(Visit visit) {
    Mapper::visitState(visit);
    visit(m_prgBank);
    visit(m_chrBank0FD);
    visit(m_chrBank0FE);
    visit(m_chrBank1FD);
    visit(m_chrBank1FE);
    visit(m_latch0);
    visit(m_latch1);
}

void Mapper010::saveState(Buffer* buf) {
    visitState(StateWriter{buf});
}

void Mapper010::loadState(Buffer* buf) {
    visitState(StateReader{buf});
}

} // namespace nes


#include "mapper009.h"
#include "../consts.h"

namespace nes {

Mapper009::Mapper009(Cartridge* cartridge)
    : Mapper(cartridge) {
}

void Mapper009::reset() {
    m_prgBank = 0;
    m_leftLatch = 1;
    m_rightLatch = 1;
    m_leftChrPage[0] = 0;
    m_leftChrPage[1] = 0;
    m_rightChrPage[0] = 0;
    m_rightChrPage[1] = 0;
    m_mirrorMode = m_cartridge->getBaseMirrorMode();
    updateBanks();
}

void Mapper009::updateBanks() {
    const auto& prg = m_cartridge->getPRG();
    const auto& chr = m_cartridge->getCHR();
    
    u32 prgBanks = prg.size() / 0x2000;  // 8KB banks
    u32 chrBanks = chr.size() / 0x1000;  // 4KB banks
    
    // PRG: Bank 0 is switchable, banks 1-3 are fixed
    m_prgBankOffset[0] = (m_prgBank % prgBanks) * 0x2000;
    m_prgBankOffset[1] = (prgBanks >= 2) ? (prgBanks - 3) * 0x2000 : 0;
    m_prgBankOffset[2] = (prgBanks >= 2) ? (prgBanks - 2) * 0x2000 : 0;
    m_prgBankOffset[3] = (prgBanks >= 1) ? (prgBanks - 1) * 0x2000 : 0;
    
    // CHR: Update based on current latch states
    if (chrBanks > 0) {
        m_chrBankOffset[0] = (m_leftChrPage[m_leftLatch] % chrBanks) * 0x1000;
        m_chrBankOffset[1] = (m_rightChrPage[m_rightLatch] % chrBanks) * 0x1000;
    }
}

u8 Mapper009::cpuRead(u16 address) {
    if (address >= 0x6000 && address < 0x8000) {
        return m_cartridge->getPRGRAM()[address & 0x1FFF];
    } else if (address >= 0x8000) {
        u8 bank = (address - 0x8000) / 0x2000;
        u16 offset = address & 0x1FFF;
        return m_cartridge->getPRG()[m_prgBankOffset[bank] + offset];
    }
    return 0;
}

void Mapper009::cpuWrite(u16 address, u8 value) {
    if (address >= 0x6000 && address < 0x8000) {
        m_cartridge->getPRGRAM()[address & 0x1FFF] = value;
    } else if (address >= 0xA000 && address < 0xB000) {
        // PRG bank select ($A000-$AFFF)
        m_prgBank = value & 0x0F;
        updateBanks();
    } else if (address >= 0xB000 && address < 0xC000) {
        // Left CHR page 0 ($B000-$BFFF)
        m_leftChrPage[0] = value & 0x1F;
        if (m_leftLatch == 0) {
            updateBanks();
        }
    } else if (address >= 0xC000 && address < 0xD000) {
        // Left CHR page 1 ($C000-$CFFF)
        m_leftChrPage[1] = value & 0x1F;
        if (m_leftLatch == 1) {
            updateBanks();
        }
    } else if (address >= 0xD000 && address < 0xE000) {
        // Right CHR page 0 ($D000-$DFFF)
        m_rightChrPage[0] = value & 0x1F;
        if (m_rightLatch == 0) {
            updateBanks();
        }
    } else if (address >= 0xE000 && address < 0xF000) {
        // Right CHR page 1 ($E000-$EFFF)
        m_rightChrPage[1] = value & 0x1F;
        if (m_rightLatch == 1) {
            updateBanks();
        }
    } else if (address >= 0xF000) {
        // Mirroring ($F000-$FFFF)
        m_mirrorMode = (value & 0x01) ? MirrorMode::HORIZONTAL : MirrorMode::VERTICAL;
    }
}

u8 Mapper009::readCHR(u16 address) {
    const auto& chr = m_cartridge->getCHR();
    
    if (address < 0x1000) {
        // Left bank (4KB at $0000)
        u8 value = chr[m_chrBankOffset[0] + (address & 0x0FFF)];
        
        // Check for latch triggers
        if ((address & 0x0FF8) == 0x0FD8) {
            // Trigger $FD latch (latch = 0)
            if (m_leftLatch != 0) {
                m_leftLatch = 0;
                updateBanks();
            }
        } else if ((address & 0x0FF8) == 0x0FE8) {
            // Trigger $FE latch (latch = 1)
            if (m_leftLatch != 1) {
                m_leftLatch = 1;
                updateBanks();
            }
        }
        
        return value;
    } else {
        // Right bank (4KB at $1000)
        u16 bankAddr = address & 0x0FFF;
        u8 value = chr[m_chrBankOffset[1] + bankAddr];
        
        // Check for latch triggers (addresses relative to $1000, so $1FD8-$1FDF and $1FE8-$1FEF)
        if ((bankAddr & 0x0FF8) == 0x0FD8) {
            // Trigger $FD latch (latch = 0)
            if (m_rightLatch != 0) {
                m_rightLatch = 0;
                updateBanks();
            }
        } else if ((bankAddr & 0x0FF8) == 0x0FE8) {
            // Trigger $FE latch (latch = 1)
            if (m_rightLatch != 1) {
                m_rightLatch = 1;
                updateBanks();
            }
        }
        
        return value;
    }
}

void Mapper009::writeCHR(u16 address, u8 value) {
    (void)address;
    (void)value;
    // CHR ROM - ignore writes
}

MirrorMode Mapper009::getMirrorMode() const {
    return m_mirrorMode;
}

void Mapper009::saveState(std::ofstream& file) const {
    file.write(reinterpret_cast<const char*>(&m_prgBank), sizeof(m_prgBank));
    file.write(reinterpret_cast<const char*>(m_leftChrPage), sizeof(m_leftChrPage));
    file.write(reinterpret_cast<const char*>(m_rightChrPage), sizeof(m_rightChrPage));
    file.write(reinterpret_cast<const char*>(&m_leftLatch), sizeof(m_leftLatch));
    file.write(reinterpret_cast<const char*>(&m_rightLatch), sizeof(m_rightLatch));
    file.write(reinterpret_cast<const char*>(&m_mirrorMode), sizeof(m_mirrorMode));
}

void Mapper009::loadState(std::ifstream& file) {
    file.read(reinterpret_cast<char*>(&m_prgBank), sizeof(m_prgBank));
    file.read(reinterpret_cast<char*>(m_leftChrPage), sizeof(m_leftChrPage));
    file.read(reinterpret_cast<char*>(m_rightChrPage), sizeof(m_rightChrPage));
    file.read(reinterpret_cast<char*>(&m_leftLatch), sizeof(m_leftLatch));
    file.read(reinterpret_cast<char*>(&m_rightLatch), sizeof(m_rightLatch));
    file.read(reinterpret_cast<char*>(&m_mirrorMode), sizeof(m_mirrorMode));
    updateBanks();
}

} // namespace nes

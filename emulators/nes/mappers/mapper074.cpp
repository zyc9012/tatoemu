#include "mapper074.h"
#include "../consts.h"
#include <cstring>

namespace nes {

Mapper074::Mapper074(Cartridge* cartridge)
    : Mapper004(cartridge) {
}

void Mapper074::reset() {
    Mapper004::reset();
    std::memset(m_chrRam, 0, sizeof(m_chrRam));
    std::memset(m_chrRamBank, false, sizeof(m_chrRamBank));
    std::memset(m_chrBankValue, 0, sizeof(m_chrBankValue));
    updateBanks();
}

bool Mapper074::isChrRamBank(u8 bank) const {
    // Banks 0x08-0x09 use CHR RAM
    return bank >= 0x08 && bank <= 0x09;
}

void Mapper074::updateBanks() {
    const auto& prg = m_cartridge->getPRG();
    const auto& chr = m_cartridge->getCHR();
    
    u32 prgBanks = prg.size() / 0x2000;  // 8KB banks
    u32 chrBanks = chr.size() / 0x0400;  // 1KB banks
    
    // PRG bank layout (same as MMC3)
    if (m_bankSelect & 0x40) {
        // $C000 swappable, $8000 fixed to second-to-last bank
        m_prgBankOffset[0] = (prgBanks - 2) * 0x2000;
        m_prgBankOffset[1] = (m_bankData[7] % prgBanks) * 0x2000;
        m_prgBankOffset[2] = (m_bankData[6] % prgBanks) * 0x2000;
        m_prgBankOffset[3] = (prgBanks - 1) * 0x2000;
    } else {
        // $8000 swappable, $C000 fixed to second-to-last bank
        m_prgBankOffset[0] = (m_bankData[6] % prgBanks) * 0x2000;
        m_prgBankOffset[1] = (m_bankData[7] % prgBanks) * 0x2000;
        m_prgBankOffset[2] = (prgBanks - 2) * 0x2000;
        m_prgBankOffset[3] = (prgBanks - 1) * 0x2000;
    }
    
    // CHR bank layout depends on bit 7 of bank select
    if (chrBanks > 0) {
        if (m_bankSelect & 0x80) {
            // R0/R1 at $1000, R2-R5 at $0000
            u8 bank0 = m_bankData[2];
            u8 bank1 = m_bankData[3];
            u8 bank2 = m_bankData[4];
            u8 bank3 = m_bankData[5];
            u8 bank4 = m_bankData[0];
            u8 bank5 = m_bankData[1];
            
            m_chrBankValue[0] = bank0;
            m_chrBankValue[1] = bank1;
            m_chrBankValue[2] = bank2;
            m_chrBankValue[3] = bank3;
            m_chrBankValue[4] = bank4 & 0xFE;
            m_chrBankValue[5] = bank4 | 0x01;
            m_chrBankValue[6] = bank5 & 0xFE;
            m_chrBankValue[7] = bank5 | 0x01;
            
            m_chrRamBank[0] = isChrRamBank(bank0);
            m_chrRamBank[1] = isChrRamBank(bank1);
            m_chrRamBank[2] = isChrRamBank(bank2);
            m_chrRamBank[3] = isChrRamBank(bank3);
            m_chrRamBank[4] = isChrRamBank(bank4 & 0xFE);
            m_chrRamBank[5] = isChrRamBank(bank4 | 0x01);
            m_chrRamBank[6] = isChrRamBank(bank5 & 0xFE);
            m_chrRamBank[7] = isChrRamBank(bank5 | 0x01);
            
            if (!m_chrRamBank[0]) m_chrBankOffset[0] = (bank0 % chrBanks) * 0x0400;
            if (!m_chrRamBank[1]) m_chrBankOffset[1] = (bank1 % chrBanks) * 0x0400;
            if (!m_chrRamBank[2]) m_chrBankOffset[2] = (bank2 % chrBanks) * 0x0400;
            if (!m_chrRamBank[3]) m_chrBankOffset[3] = (bank3 % chrBanks) * 0x0400;
            if (!m_chrRamBank[4]) m_chrBankOffset[4] = ((bank4 & 0xFE) % chrBanks) * 0x0400;
            if (!m_chrRamBank[5]) m_chrBankOffset[5] = ((bank4 | 0x01) % chrBanks) * 0x0400;
            if (!m_chrRamBank[6]) m_chrBankOffset[6] = ((bank5 & 0xFE) % chrBanks) * 0x0400;
            if (!m_chrRamBank[7]) m_chrBankOffset[7] = ((bank5 | 0x01) % chrBanks) * 0x0400;
        } else {
            // R0/R1 at $0000, R2-R5 at $1000
            u8 bank0 = m_bankData[0];
            u8 bank1 = m_bankData[1];
            u8 bank2 = m_bankData[2];
            u8 bank3 = m_bankData[3];
            u8 bank4 = m_bankData[4];
            u8 bank5 = m_bankData[5];
            
            m_chrBankValue[0] = bank0 & 0xFE;
            m_chrBankValue[1] = bank0 | 0x01;
            m_chrBankValue[2] = bank1 & 0xFE;
            m_chrBankValue[3] = bank1 | 0x01;
            m_chrBankValue[4] = bank2;
            m_chrBankValue[5] = bank3;
            m_chrBankValue[6] = bank4;
            m_chrBankValue[7] = bank5;
            
            m_chrRamBank[0] = isChrRamBank(bank0 & 0xFE);
            m_chrRamBank[1] = isChrRamBank(bank0 | 0x01);
            m_chrRamBank[2] = isChrRamBank(bank1 & 0xFE);
            m_chrRamBank[3] = isChrRamBank(bank1 | 0x01);
            m_chrRamBank[4] = isChrRamBank(bank2);
            m_chrRamBank[5] = isChrRamBank(bank3);
            m_chrRamBank[6] = isChrRamBank(bank4);
            m_chrRamBank[7] = isChrRamBank(bank5);
            
            if (!m_chrRamBank[0]) m_chrBankOffset[0] = ((bank0 & 0xFE) % chrBanks) * 0x0400;
            if (!m_chrRamBank[1]) m_chrBankOffset[1] = ((bank0 | 0x01) % chrBanks) * 0x0400;
            if (!m_chrRamBank[2]) m_chrBankOffset[2] = ((bank1 & 0xFE) % chrBanks) * 0x0400;
            if (!m_chrRamBank[3]) m_chrBankOffset[3] = ((bank1 | 0x01) % chrBanks) * 0x0400;
            if (!m_chrRamBank[4]) m_chrBankOffset[4] = (bank2 % chrBanks) * 0x0400;
            if (!m_chrRamBank[5]) m_chrBankOffset[5] = (bank3 % chrBanks) * 0x0400;
            if (!m_chrRamBank[6]) m_chrBankOffset[6] = (bank4 % chrBanks) * 0x0400;
            if (!m_chrRamBank[7]) m_chrBankOffset[7] = (bank5 % chrBanks) * 0x0400;
        }
    } else {
        // No CHR ROM, all banks use CHR RAM
        std::memset(m_chrRamBank, true, sizeof(m_chrRamBank));
        std::memset(m_chrBankValue, 0, sizeof(m_chrBankValue));
    }
}

u8 Mapper074::readCHR(u16 address) {
    u8 bank = address / 0x0400;
    u16 offset = address & 0x03FF;
    
    if (m_chrRamBank[bank]) {
        // Use CHR RAM (banks 0x08-0x09 map to 2KB CHR RAM)
        u8 bankValue = m_chrBankValue[bank];
        if (bankValue >= 0x08 && bankValue <= 0x09) {
            u16 ramOffset = ((bankValue - 0x08) * 0x0400) + offset;
            if (ramOffset < sizeof(m_chrRam)) {
                return m_chrRam[ramOffset];
            }
        }
        return 0;
    } else {
        // Use CHR ROM
        return m_cartridge->getCHR()[m_chrBankOffset[bank] + offset];
    }
}

void Mapper074::writeCHR(u16 address, u8 value) {
    u8 bank = address / 0x0400;
    u16 offset = address & 0x03FF;
    
    if (m_chrRamBank[bank]) {
        // Write to CHR RAM (banks 0x08-0x09 map to 2KB CHR RAM)
        u8 bankValue = m_chrBankValue[bank];
        if (bankValue >= 0x08 && bankValue <= 0x09) {
            u16 ramOffset = ((bankValue - 0x08) * 0x0400) + offset;
            if (ramOffset < sizeof(m_chrRam)) {
                m_chrRam[ramOffset] = value;
            }
        }
    } else {
        // CHR ROM - write to cartridge CHR (if CHR RAM)
        m_cartridge->getCHR()[m_chrBankOffset[bank] + offset] = value;
    }
}

template <typename Visit>
void Mapper074::visitState(Visit visit) {
    Mapper004::visitState(visit);
    // The CHR RAM this mapper adds on top of MMC3
    visit(m_chrRamBank);
    visit(m_chrBankValue);
    visit(m_chrRam);
}

void Mapper074::saveState(Buffer* buf) {
    visitState(StateWriter{buf});
}

void Mapper074::loadState(Buffer* buf) {
    visitState(StateReader{buf});
    updateBanks();
}

} // namespace nes

#include "mapper001.h"
#include "../consts.h"

namespace nes {

Mapper001::Mapper001(Cartridge* cartridge)
    : Mapper(cartridge)
    , m_shiftRegister(0x10)
    , m_shiftCount(0)
    , m_control(0x0C)
    , m_chrBank0(0)
    , m_chrBank1(0)
    , m_prgBank(0) {
}

void Mapper001::reset() {
    m_shiftRegister = 0x10;
    m_shiftCount = 0;
    m_control = 0x0C;  // PRG mode 3, CHR mode 0
    m_chrBank0 = 0;
    m_chrBank1 = 0;
    m_prgBank = 0;
    updateBanks();
}

void Mapper001::updateBanks() {
    const auto& prg = m_cartridge->getPRG();
    const auto& chr = m_cartridge->getCHR();
    
    u32 prgSize = prg.size();
    u32 chrSize = chr.size();
    
    // PRG bank switching
    u8 prgMode = (m_control >> 2) & 0x03;
    
    switch (prgMode) {
        case 0:
        case 1:
            // 32KB mode (ignore low bit of bank number)
            m_prgBankOffset[0] = ((m_prgBank & 0x0E) % (prgSize / 0x4000)) * 0x4000;
            m_prgBankOffset[1] = m_prgBankOffset[0] + 0x4000;
            break;
        case 2:
            // Fix first bank, switch second
            m_prgBankOffset[0] = 0;
            m_prgBankOffset[1] = ((m_prgBank & 0x0F) % (prgSize / 0x4000)) * 0x4000;
            break;
        case 3:
            // Switch first bank, fix last
            m_prgBankOffset[0] = ((m_prgBank & 0x0F) % (prgSize / 0x4000)) * 0x4000;
            m_prgBankOffset[1] = prgSize - 0x4000;
            break;
    }
    
    // CHR bank switching
    bool chrMode = (m_control & 0x10) != 0;
    
    if (chrMode) {
        // 4KB mode
        m_chrBankOffset[0] = (m_chrBank0 % (chrSize / 0x1000)) * 0x1000;
        m_chrBankOffset[1] = (m_chrBank1 % (chrSize / 0x1000)) * 0x1000;
    } else {
        // 8KB mode
        m_chrBankOffset[0] = ((m_chrBank0 & 0x1E) % (chrSize / 0x1000)) * 0x1000;
        m_chrBankOffset[1] = m_chrBankOffset[0] + 0x1000;
    }
}

u8 Mapper001::cpuRead(u16 address) {
    if (address >= 0x6000 && address < 0x8000) {
        return m_cartridge->getPRGRAM()[address & 0x1FFF];
    } else if (address >= 0x8000 && address < 0xC000) {
        return m_cartridge->getPRG()[m_prgBankOffset[0] + (address & 0x3FFF)];
    } else if (address >= 0xC000) {
        return m_cartridge->getPRG()[m_prgBankOffset[1] + (address & 0x3FFF)];
    }
    return 0;
}

void Mapper001::cpuWrite(u16 address, u8 value) {
    if (address >= 0x6000 && address < 0x8000) {
        m_cartridge->getPRGRAM()[address & 0x1FFF] = value;
    } else if (address >= 0x8000) {
        // Register writes
        if (value & 0x80) {
            // Reset shift register
            m_shiftRegister = 0x10;
            m_shiftCount = 0;
            m_control |= 0x0C;
            updateBanks();
        } else {
            // Shift in bit
            m_shiftRegister >>= 1;
            m_shiftRegister |= (value & 1) << 4;
            m_shiftCount++;
            
            if (m_shiftCount == 5) {
                // Write to internal register
                u8 reg = (address >> 13) & 0x03;
                
                switch (reg) {
                    case 0:  // $8000-$9FFF: Control
                        m_control = m_shiftRegister;
                        break;
                    case 1:  // $A000-$BFFF: CHR bank 0
                        m_chrBank0 = m_shiftRegister;
                        break;
                    case 2:  // $C000-$DFFF: CHR bank 1
                        m_chrBank1 = m_shiftRegister;
                        break;
                    case 3:  // $E000-$FFFF: PRG bank
                        m_prgBank = m_shiftRegister;
                        break;
                }
                
                m_shiftRegister = 0x10;
                m_shiftCount = 0;
                updateBanks();
            }
        }
    }
}

u8 Mapper001::readCHR(u16 address) {
    if (address < 0x1000) {
        return m_cartridge->getCHR()[m_chrBankOffset[0] + (address & 0x0FFF)];
    } else {
        return m_cartridge->getCHR()[m_chrBankOffset[1] + (address & 0x0FFF)];
    }
}

void Mapper001::writeCHR(u16 address, u8 value) {
    // CHR RAM only
    if (address < 0x1000) {
        m_cartridge->getCHR()[m_chrBankOffset[0] + (address & 0x0FFF)] = value;
    } else {
        m_cartridge->getCHR()[m_chrBankOffset[1] + (address & 0x0FFF)] = value;
    }
}

MirrorMode Mapper001::getMirrorMode() const {
    switch (m_control & 0x03) {
        case 0: return MirrorMode::SINGLE_SCREEN_A;
        case 1: return MirrorMode::SINGLE_SCREEN_B;
        case 2: return MirrorMode::VERTICAL;
        case 3: return MirrorMode::HORIZONTAL;
        default: return MirrorMode::HORIZONTAL;
    }
}

void Mapper001::saveState(std::ofstream& file) const {
    file.write(reinterpret_cast<const char*>(&m_shiftRegister), sizeof(m_shiftRegister));
    file.write(reinterpret_cast<const char*>(&m_shiftCount), sizeof(m_shiftCount));
    file.write(reinterpret_cast<const char*>(&m_control), sizeof(m_control));
    file.write(reinterpret_cast<const char*>(&m_chrBank0), sizeof(m_chrBank0));
    file.write(reinterpret_cast<const char*>(&m_chrBank1), sizeof(m_chrBank1));
    file.write(reinterpret_cast<const char*>(&m_prgBank), sizeof(m_prgBank));
}

void Mapper001::loadState(std::ifstream& file) {
    file.read(reinterpret_cast<char*>(&m_shiftRegister), sizeof(m_shiftRegister));
    file.read(reinterpret_cast<char*>(&m_shiftCount), sizeof(m_shiftCount));
    file.read(reinterpret_cast<char*>(&m_control), sizeof(m_control));
    file.read(reinterpret_cast<char*>(&m_chrBank0), sizeof(m_chrBank0));
    file.read(reinterpret_cast<char*>(&m_chrBank1), sizeof(m_chrBank1));
    file.read(reinterpret_cast<char*>(&m_prgBank), sizeof(m_prgBank));
    updateBanks();
}

} // namespace nes


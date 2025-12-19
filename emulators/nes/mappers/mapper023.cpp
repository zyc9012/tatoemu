#include "mapper023.h"
#include "../consts.h"
#include <cstring>

namespace nes {

Mapper023::Mapper023(Cartridge* cartridge)
    : Mapper(cartridge) {
}

void Mapper023::reset() {
    std::memset(m_prgBank, 0, sizeof(m_prgBank));
    std::memset(m_chrBank, 0, sizeof(m_chrBank));
    std::memset(m_chrBankHigh, 0, sizeof(m_chrBankHigh));
    std::memset(m_prgBankOffset, 0, sizeof(m_prgBankOffset));
    std::memset(m_chrBankOffset, 0, sizeof(m_chrBankOffset));
    m_prgSwapMode = 0;
    m_mirrorMode = m_cartridge->getBaseMirrorMode();
    m_irqLatch = 0;
    m_irqCounter = 0;
    m_irqPrescaler = 0;
    m_irqPrescalerCounter = 0;
    m_irqEnable = false;
    m_irqEnableOnAck = false;
    m_irqMode = false;
    m_irqActive = false;
    updateBanks();
}

void Mapper023::updateBanks() {
    const auto& prg = m_cartridge->getPRG();
    const auto& chr = m_cartridge->getCHR();
    
    u32 prgBanks8k = prg.size() / 0x2000;
    u32 chrBanks1k = chr.size() / 0x400;
    if (chrBanks1k == 0) chrBanks1k = 8;  // CHR RAM
    
    // PRG banks
    if (m_prgSwapMode & 0x02) {
        // Swap mode: $C000 switchable, $8000 fixed to second-to-last
        m_prgBankOffset[0] = (prgBanks8k - 2) * 0x2000;
        m_prgBankOffset[1] = (m_prgBank[1] % prgBanks8k) * 0x2000;
        m_prgBankOffset[2] = (m_prgBank[0] % prgBanks8k) * 0x2000;
        m_prgBankOffset[3] = (prgBanks8k - 1) * 0x2000;
    } else {
        // Normal mode: $8000 switchable, $C000 fixed to second-to-last
        m_prgBankOffset[0] = (m_prgBank[0] % prgBanks8k) * 0x2000;
        m_prgBankOffset[1] = (m_prgBank[1] % prgBanks8k) * 0x2000;
        m_prgBankOffset[2] = (prgBanks8k - 2) * 0x2000;
        m_prgBankOffset[3] = (prgBanks8k - 1) * 0x2000;
    }
    
    // CHR banks (combine low and high nibbles)
    for (int i = 0; i < 8; i++) {
        u8 bank = m_chrBank[i] | (m_chrBankHigh[i] << 4);
        m_chrBankOffset[i] = (bank % chrBanks1k) * 0x400;
    }
}

u8 Mapper023::cpuRead(u16 address) {
    if (address >= 0x6000 && address < 0x8000) {
        return m_cartridge->getPRGRAM()[address & 0x1FFF];
    } else if (address >= 0x8000) {
        u8 bank = (address - 0x8000) / 0x2000;
        u16 offset = address & 0x1FFF;
        return m_cartridge->getPRG()[m_prgBankOffset[bank] + offset];
    }
    return 0;
}

void Mapper023::cpuWrite(u16 address, u8 value) {
    if (address >= 0x6000 && address < 0x8000) {
        m_cartridge->getPRGRAM()[address & 0x1FFF] = value;
        return;
    }
    
    // VRC2b/VRC4 address line mapping: A0 and A1 swapped
    // Mapper 23 uses A0 and A1 directly
    u16 reg = (address & 0xF000) | ((address & 0x03));
    
    switch (reg) {
        case 0x8000: case 0x8001: case 0x8002: case 0x8003:
            m_prgBank[0] = value & 0x1F;
            updateBanks();
            break;
            
        case 0x9000: case 0x9001:
            switch (value & 0x03) {
                case 0: m_mirrorMode = MirrorMode::VERTICAL; break;
                case 1: m_mirrorMode = MirrorMode::HORIZONTAL; break;
                case 2: m_mirrorMode = MirrorMode::SINGLE_SCREEN_A; break;
                case 3: m_mirrorMode = MirrorMode::SINGLE_SCREEN_B; break;
            }
            break;
            
        case 0x9002: case 0x9003:
            m_prgSwapMode = value;
            updateBanks();
            break;
            
        case 0xA000: case 0xA001: case 0xA002: case 0xA003:
            m_prgBank[1] = value & 0x1F;
            updateBanks();
            break;
            
        case 0xB000:
            m_chrBank[0] = value & 0x0F;
            updateBanks();
            break;
        case 0xB001:
            m_chrBankHigh[0] = value & 0x1F;
            updateBanks();
            break;
        case 0xB002:
            m_chrBank[1] = value & 0x0F;
            updateBanks();
            break;
        case 0xB003:
            m_chrBankHigh[1] = value & 0x1F;
            updateBanks();
            break;
            
        case 0xC000:
            m_chrBank[2] = value & 0x0F;
            updateBanks();
            break;
        case 0xC001:
            m_chrBankHigh[2] = value & 0x1F;
            updateBanks();
            break;
        case 0xC002:
            m_chrBank[3] = value & 0x0F;
            updateBanks();
            break;
        case 0xC003:
            m_chrBankHigh[3] = value & 0x1F;
            updateBanks();
            break;
            
        case 0xD000:
            m_chrBank[4] = value & 0x0F;
            updateBanks();
            break;
        case 0xD001:
            m_chrBankHigh[4] = value & 0x1F;
            updateBanks();
            break;
        case 0xD002:
            m_chrBank[5] = value & 0x0F;
            updateBanks();
            break;
        case 0xD003:
            m_chrBankHigh[5] = value & 0x1F;
            updateBanks();
            break;
            
        case 0xE000:
            m_chrBank[6] = value & 0x0F;
            updateBanks();
            break;
        case 0xE001:
            m_chrBankHigh[6] = value & 0x1F;
            updateBanks();
            break;
        case 0xE002:
            m_chrBank[7] = value & 0x0F;
            updateBanks();
            break;
        case 0xE003:
            m_chrBankHigh[7] = value & 0x1F;
            updateBanks();
            break;
            
        case 0xF000:
            m_irqLatch = (m_irqLatch & 0xF0) | (value & 0x0F);
            break;
        case 0xF001:
            m_irqLatch = (m_irqLatch & 0x0F) | ((value & 0x0F) << 4);
            break;
        case 0xF002:
            m_irqEnableOnAck = (value & 0x01) != 0;
            m_irqEnable = (value & 0x02) != 0;
            m_irqMode = (value & 0x04) != 0;
            if (m_irqEnable) {
                m_irqCounter = m_irqLatch;
                m_irqPrescalerCounter = 341;
            }
            m_irqActive = false;
            break;
        case 0xF003:
            m_irqEnable = m_irqEnableOnAck;
            m_irqActive = false;
            break;
    }
}

u8 Mapper023::readCHR(u16 address) {
    u8 bank = address / 0x400;
    u16 offset = address & 0x3FF;
    return m_cartridge->getCHR()[m_chrBankOffset[bank] + offset];
}

void Mapper023::writeCHR(u16 address, u8 value) {
    u8 bank = address / 0x400;
    u16 offset = address & 0x3FF;
    m_cartridge->getCHR()[m_chrBankOffset[bank] + offset] = value;
}

MirrorMode Mapper023::getMirrorMode() const {
    return m_mirrorMode;
}

void Mapper023::scanlineCounter() {
    if (!m_irqEnable) return;
    
    if (m_irqMode) {
        // Cycle mode
        m_irqPrescalerCounter--;
        if (m_irqPrescalerCounter <= 0) {
            m_irqPrescalerCounter = 341;
            if (m_irqCounter == 0xFF) {
                m_irqCounter = m_irqLatch;
                m_irqActive = true;
            } else {
                m_irqCounter++;
            }
        }
    } else {
        // Scanline mode
        if (m_irqCounter == 0xFF) {
            m_irqCounter = m_irqLatch;
            m_irqActive = true;
        } else {
            m_irqCounter++;
        }
    }
}

void Mapper023::saveState(std::ofstream& file) const {
    file.write(reinterpret_cast<const char*>(m_prgBank), sizeof(m_prgBank));
    file.write(reinterpret_cast<const char*>(m_chrBank), sizeof(m_chrBank));
    file.write(reinterpret_cast<const char*>(m_chrBankHigh), sizeof(m_chrBankHigh));
    file.write(reinterpret_cast<const char*>(&m_prgSwapMode), sizeof(m_prgSwapMode));
    file.write(reinterpret_cast<const char*>(&m_mirrorMode), sizeof(m_mirrorMode));
    file.write(reinterpret_cast<const char*>(&m_irqLatch), sizeof(m_irqLatch));
    file.write(reinterpret_cast<const char*>(&m_irqCounter), sizeof(m_irqCounter));
    file.write(reinterpret_cast<const char*>(&m_irqPrescaler), sizeof(m_irqPrescaler));
    file.write(reinterpret_cast<const char*>(&m_irqPrescalerCounter), sizeof(m_irqPrescalerCounter));
    file.write(reinterpret_cast<const char*>(&m_irqEnable), sizeof(m_irqEnable));
    file.write(reinterpret_cast<const char*>(&m_irqEnableOnAck), sizeof(m_irqEnableOnAck));
    file.write(reinterpret_cast<const char*>(&m_irqMode), sizeof(m_irqMode));
}

void Mapper023::loadState(std::ifstream& file) {
    file.read(reinterpret_cast<char*>(m_prgBank), sizeof(m_prgBank));
    file.read(reinterpret_cast<char*>(m_chrBank), sizeof(m_chrBank));
    file.read(reinterpret_cast<char*>(m_chrBankHigh), sizeof(m_chrBankHigh));
    file.read(reinterpret_cast<char*>(&m_prgSwapMode), sizeof(m_prgSwapMode));
    file.read(reinterpret_cast<char*>(&m_mirrorMode), sizeof(m_mirrorMode));
    file.read(reinterpret_cast<char*>(&m_irqLatch), sizeof(m_irqLatch));
    file.read(reinterpret_cast<char*>(&m_irqCounter), sizeof(m_irqCounter));
    file.read(reinterpret_cast<char*>(&m_irqPrescaler), sizeof(m_irqPrescaler));
    file.read(reinterpret_cast<char*>(&m_irqPrescalerCounter), sizeof(m_irqPrescalerCounter));
    file.read(reinterpret_cast<char*>(&m_irqEnable), sizeof(m_irqEnable));
    file.read(reinterpret_cast<char*>(&m_irqEnableOnAck), sizeof(m_irqEnableOnAck));
    file.read(reinterpret_cast<char*>(&m_irqMode), sizeof(m_irqMode));
    updateBanks();
}

} // namespace nes


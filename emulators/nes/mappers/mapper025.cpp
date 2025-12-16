#include "mapper025.h"
#include "../consts.h"
#include <cstring>

namespace nes {

Mapper025::Mapper025(Cartridge* cartridge)
    : Mapper(cartridge)
    , m_prgSwapMode(0)
    , m_mirrorMode(cartridge->getBaseMirrorMode())
    , m_irqLatch(0)
    , m_irqCounter(0)
    , m_irqPrescaler(0)
    , m_irqPrescalerCounter(0)
    , m_irqEnable(false)
    , m_irqEnableOnAck(false)
    , m_irqMode(false) {
    std::memset(m_prgBank, 0, sizeof(m_prgBank));
    std::memset(m_chrBank, 0, sizeof(m_chrBank));
    std::memset(m_chrBankHigh, 0, sizeof(m_chrBankHigh));
    std::memset(m_prgBankOffset, 0, sizeof(m_prgBankOffset));
    std::memset(m_chrBankOffset, 0, sizeof(m_chrBankOffset));
}

void Mapper025::reset() {
    std::memset(m_prgBank, 0, sizeof(m_prgBank));
    std::memset(m_chrBank, 0, sizeof(m_chrBank));
    std::memset(m_chrBankHigh, 0, sizeof(m_chrBankHigh));
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

void Mapper025::updateBanks() {
    const auto& prg = m_cartridge->getPRG();
    const auto& chr = m_cartridge->getCHR();
    
    u32 prgBanks8k = prg.size() / 0x2000;
    u32 chrBanks1k = chr.size() / 0x400;
    if (chrBanks1k == 0) chrBanks1k = 8;
    
    // PRG banks
    if (m_prgSwapMode & 0x02) {
        m_prgBankOffset[0] = (prgBanks8k - 2) * 0x2000;
        m_prgBankOffset[1] = (m_prgBank[1] % prgBanks8k) * 0x2000;
        m_prgBankOffset[2] = (m_prgBank[0] % prgBanks8k) * 0x2000;
        m_prgBankOffset[3] = (prgBanks8k - 1) * 0x2000;
    } else {
        m_prgBankOffset[0] = (m_prgBank[0] % prgBanks8k) * 0x2000;
        m_prgBankOffset[1] = (m_prgBank[1] % prgBanks8k) * 0x2000;
        m_prgBankOffset[2] = (prgBanks8k - 2) * 0x2000;
        m_prgBankOffset[3] = (prgBanks8k - 1) * 0x2000;
    }
    
    // CHR banks
    for (int i = 0; i < 8; i++) {
        u32 bank = m_chrBank[i] | (m_chrBankHigh[i] << 4);
        m_chrBankOffset[i] = (bank % chrBanks1k) * 0x400;
    }
}

u8 Mapper025::cpuRead(u16 address) {
    if (address >= 0x6000 && address < 0x8000) {
        return m_cartridge->getPRGRAM()[address & 0x1FFF];
    } else if (address >= 0x8000) {
        u8 bank = (address - 0x8000) / 0x2000;
        u16 offset = address & 0x1FFF;
        return m_cartridge->getPRG()[m_prgBankOffset[bank] + offset];
    }
    return 0;
}

void Mapper025::cpuWrite(u16 address, u8 value) {
    if (address >= 0x6000 && address < 0x8000) {
        m_cartridge->getPRGRAM()[address & 0x1FFF] = value;
        return;
    }
    
    // Mapper 25 (VRC4b/VRC4d) - OR both variants together for compatibility
    // VRC4b: A1, A0 -> A0, A1 (swap)
    // VRC4d: A3, A2 -> A0, A1
    // ORing both values makes most games work
    u16 a0 = ((address >> 1) & 0x01) | ((address >> 3) & 0x01);  // VRC4b: A1, VRC4d: A3
    u16 a1 = ((address >> 0) & 0x01) | ((address >> 2) & 0x01);  // VRC4b: A0, VRC4d: A2
    u16 reg = (address & 0xF000) | (a1 << 1) | a0;
    
    if (reg >= 0x8000 && reg <= 0x8006) {
        m_prgBank[0] = value & 0x1F; 
        updateBanks();
    } else if (reg >= 0x9000 && reg <= 0x9001) {
        switch (value & 0x03) {
            case 0: m_mirrorMode = MirrorMode::VERTICAL; break;
            case 1: m_mirrorMode = MirrorMode::HORIZONTAL; break;
            case 2: m_mirrorMode = MirrorMode::SINGLE_SCREEN_A; break;
            case 3: m_mirrorMode = MirrorMode::SINGLE_SCREEN_B; break;
        }
    } else if (reg >= 0x9002 && reg <= 0x9003) {
        m_prgSwapMode = value;
        updateBanks();
    } else if (reg >= 0xA000 && reg <= 0xA006) {
        m_prgBank[1] = value & 0x1F;
        updateBanks();
    } else if (reg >= 0xB000 && reg <= 0xE006) {
        u8 regNumber = ((((reg >> 12) & 0x07) - 3) << 1) + ((reg >> 1) & 0x01);
        bool lowBits = (reg & 0x01) == 0x00;
        if (lowBits) {
            m_chrBank[regNumber] = value & 0x0F;
        } else {
            m_chrBankHigh[regNumber] = value & 0x1F;
        }
        updateBanks();
    } else if (reg == 0xF000) {
        m_irqLatch = (m_irqLatch & 0xF0) | (value & 0x0F);
    } else if (reg == 0xF001) {
        m_irqLatch = (m_irqLatch & 0x0F) | ((value & 0x0F) << 4);
    } else if (reg == 0xF002) {
        m_irqEnableOnAck = (value & 0x01) != 0;
        m_irqEnable = (value & 0x02) != 0;
        m_irqMode = (value & 0x04) != 0;
        if (m_irqEnable) {
            m_irqCounter = m_irqLatch;
            m_irqPrescalerCounter = 341;
        }
        m_irqActive = false;
    } else if (reg == 0xF003) {
        m_irqEnable = m_irqEnableOnAck;
        m_irqActive = false;
    }
}

u8 Mapper025::readCHR(u16 address) {
    u8 bank = address / 0x400;
    u16 offset = address & 0x3FF;
    return m_cartridge->getCHR()[m_chrBankOffset[bank] + offset];
}

void Mapper025::writeCHR(u16 address, u8 value) {
    u8 bank = address / 0x400;
    u16 offset = address & 0x3FF;
    m_cartridge->getCHR()[m_chrBankOffset[bank] + offset] = value;
}

MirrorMode Mapper025::getMirrorMode() const {
    return m_mirrorMode;
}

void Mapper025::scanlineCounter() {
    // VRC4 IRQ is clocked per CPU cycle (see clockAudio).
    // Leave empty to avoid double-clocking when Core calls this at PPU scanline time.
}

void Mapper025::clockAudio() {
    if (!m_irqEnable) return;
    
    if (m_irqMode) {
        // Cycle mode: clock every CPU cycle.
        if (m_irqCounter == 0xFF) {
            m_irqCounter = m_irqLatch;
            m_irqActive = true;
        } else {
            m_irqCounter++;
        }
    } else {
        // Scanline mode: divide CPU cycles down to ~341 PPU cycles.
        m_irqPrescalerCounter -= 3;  // 3 PPU cycles per CPU cycle
        if (m_irqPrescalerCounter <= 0) {
            m_irqPrescalerCounter += 341;
            
            if (m_irqCounter == 0xFF) {
                m_irqCounter = m_irqLatch;
                m_irqActive = true;
            } else {
                m_irqCounter++;
            }
        }
    }
}

void Mapper025::saveState(std::ofstream& file) const {
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

void Mapper025::loadState(std::ifstream& file) {
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


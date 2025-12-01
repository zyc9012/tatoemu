#include "mapper004.h"
#include "../consts.h"
#include <cstring>

namespace nes {

Mapper004::Mapper004(Cartridge* cartridge)
    : Mapper(cartridge)
    , m_bankSelect(0)
    , m_irqLatch(0)
    , m_irqCounter(0)
    , m_irqEnable(false)
    , m_irqReload(false)
    , m_mirrorMode(MirrorMode::HORIZONTAL)
    , m_prgRamEnable(true) {
    std::memset(m_bankData, 0, sizeof(m_bankData));
}

void Mapper004::reset() {
    m_bankSelect = 0;
    std::memset(m_bankData, 0, sizeof(m_bankData));
    m_irqLatch = 0;
    m_irqCounter = 0;
    m_irqEnable = false;
    m_irqReload = false;
    m_irqActive = false;
    m_mirrorMode = MirrorMode::HORIZONTAL;
    m_prgRamEnable = true;
    updateBanks();
}

void Mapper004::updateBanks() {
    const auto& prg = m_cartridge->getPRG();
    const auto& chr = m_cartridge->getCHR();
    
    u32 prgBanks = prg.size() / 0x2000;  // 8KB banks
    u32 chrBanks = chr.size() / 0x0400;  // 1KB banks
    
    // PRG bank layout depends on bit 6 of bank select
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
            m_chrBankOffset[0] = (m_bankData[2] % chrBanks) * 0x0400;
            m_chrBankOffset[1] = (m_bankData[3] % chrBanks) * 0x0400;
            m_chrBankOffset[2] = (m_bankData[4] % chrBanks) * 0x0400;
            m_chrBankOffset[3] = (m_bankData[5] % chrBanks) * 0x0400;
            m_chrBankOffset[4] = ((m_bankData[0] & 0xFE) % chrBanks) * 0x0400;
            m_chrBankOffset[5] = ((m_bankData[0] | 0x01) % chrBanks) * 0x0400;
            m_chrBankOffset[6] = ((m_bankData[1] & 0xFE) % chrBanks) * 0x0400;
            m_chrBankOffset[7] = ((m_bankData[1] | 0x01) % chrBanks) * 0x0400;
        } else {
            // R0/R1 at $0000, R2-R5 at $1000
            m_chrBankOffset[0] = ((m_bankData[0] & 0xFE) % chrBanks) * 0x0400;
            m_chrBankOffset[1] = ((m_bankData[0] | 0x01) % chrBanks) * 0x0400;
            m_chrBankOffset[2] = ((m_bankData[1] & 0xFE) % chrBanks) * 0x0400;
            m_chrBankOffset[3] = ((m_bankData[1] | 0x01) % chrBanks) * 0x0400;
            m_chrBankOffset[4] = (m_bankData[2] % chrBanks) * 0x0400;
            m_chrBankOffset[5] = (m_bankData[3] % chrBanks) * 0x0400;
            m_chrBankOffset[6] = (m_bankData[4] % chrBanks) * 0x0400;
            m_chrBankOffset[7] = (m_bankData[5] % chrBanks) * 0x0400;
        }
    }
}

u8 Mapper004::cpuRead(u16 address) {
    if (address >= 0x6000 && address < 0x8000) {
        if (m_prgRamEnable) {
            return m_cartridge->getPRGRAM()[address & 0x1FFF];
        }
        return 0;
    } else if (address >= 0x8000) {
        u8 bank = (address - 0x8000) / 0x2000;
        u16 offset = address & 0x1FFF;
        return m_cartridge->getPRG()[m_prgBankOffset[bank] + offset];
    }
    return 0;
}

void Mapper004::cpuWrite(u16 address, u8 value) {
    if (address >= 0x6000 && address < 0x8000) {
        if (m_prgRamEnable) {
            m_cartridge->getPRGRAM()[address & 0x1FFF] = value;
        }
    } else if (address >= 0x8000) {
        switch (address & 0xE001) {
            case 0x8000:
                // Bank select
                m_bankSelect = value;
                updateBanks();
                break;
                
            case 0x8001:
                // Bank data
                m_bankData[m_bankSelect & 0x07] = value;
                updateBanks();
                break;
                
            case 0xA000:
                // Mirroring
                if (value & 0x01) {
                    m_mirrorMode = MirrorMode::HORIZONTAL;
                } else {
                    m_mirrorMode = MirrorMode::VERTICAL;
                }
                break;
                
            case 0xA001:
                // PRG RAM protect
                m_prgRamEnable = (value & 0x80) != 0;
                break;
                
            case 0xC000:
                // IRQ latch
                m_irqLatch = value;
                break;
                
            case 0xC001:
                // IRQ reload
                m_irqReload = true;
                m_irqCounter = 0;
                break;
                
            case 0xE000:
                // IRQ disable
                m_irqEnable = false;
                m_irqActive = false;
                break;
                
            case 0xE001:
                // IRQ enable
                m_irqEnable = true;
                break;
        }
    }
}

u8 Mapper004::readCHR(u16 address) {
    u8 bank = address / 0x0400;
    u16 offset = address & 0x03FF;
    return m_cartridge->getCHR()[m_chrBankOffset[bank] + offset];
}

void Mapper004::writeCHR(u16 address, u8 value) {
    // CHR RAM only
    u8 bank = address / 0x0400;
    u16 offset = address & 0x03FF;
    m_cartridge->getCHR()[m_chrBankOffset[bank] + offset] = value;
}

MirrorMode Mapper004::getMirrorMode() const {
    return m_mirrorMode;
}

void Mapper004::scanlineCounter() {
    if (m_irqCounter == 0 || m_irqReload) {
        m_irqCounter = m_irqLatch;
        m_irqReload = false;
    } else {
        m_irqCounter--;
    }
    
    if (m_irqCounter == 0 && m_irqEnable) {
        m_irqActive = true;
    }
}

void Mapper004::saveState(std::ofstream& file) const {
    file.write(reinterpret_cast<const char*>(&m_bankSelect), sizeof(m_bankSelect));
    file.write(reinterpret_cast<const char*>(m_bankData), sizeof(m_bankData));
    file.write(reinterpret_cast<const char*>(&m_irqLatch), sizeof(m_irqLatch));
    file.write(reinterpret_cast<const char*>(&m_irqCounter), sizeof(m_irqCounter));
    file.write(reinterpret_cast<const char*>(&m_irqEnable), sizeof(m_irqEnable));
    file.write(reinterpret_cast<const char*>(&m_irqReload), sizeof(m_irqReload));
    file.write(reinterpret_cast<const char*>(&m_irqActive), sizeof(m_irqActive));
    file.write(reinterpret_cast<const char*>(&m_mirrorMode), sizeof(m_mirrorMode));
    file.write(reinterpret_cast<const char*>(&m_prgRamEnable), sizeof(m_prgRamEnable));
}

void Mapper004::loadState(std::ifstream& file) {
    file.read(reinterpret_cast<char*>(&m_bankSelect), sizeof(m_bankSelect));
    file.read(reinterpret_cast<char*>(m_bankData), sizeof(m_bankData));
    file.read(reinterpret_cast<char*>(&m_irqLatch), sizeof(m_irqLatch));
    file.read(reinterpret_cast<char*>(&m_irqCounter), sizeof(m_irqCounter));
    file.read(reinterpret_cast<char*>(&m_irqEnable), sizeof(m_irqEnable));
    file.read(reinterpret_cast<char*>(&m_irqReload), sizeof(m_irqReload));
    file.read(reinterpret_cast<char*>(&m_irqActive), sizeof(m_irqActive));
    file.read(reinterpret_cast<char*>(&m_mirrorMode), sizeof(m_mirrorMode));
    file.read(reinterpret_cast<char*>(&m_prgRamEnable), sizeof(m_prgRamEnable));
    updateBanks();
}

} // namespace nes


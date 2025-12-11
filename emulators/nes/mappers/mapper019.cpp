#include "mapper019.h"
#include "../consts.h"

namespace nes {

Mapper019::Mapper019(Cartridge* cartridge)
    : Mapper(cartridge)
    , m_prgBank{0, 0, 0}
    , m_chrBank{0}
    , m_chrUseCiram{false}
    , m_irqCounter(0)
    , m_irqEnable(false) {
}

void Mapper019::reset() {
    m_prgBank[0] = 0;
    m_prgBank[1] = 1;
    m_prgBank[2] = 2;
    
    for (u8 i = 0; i < 8; i++) {
        m_chrBank[i] = i;
        m_chrUseCiram[i] = false;
    }
    
    m_irqCounter = 0;
    m_irqEnable = false;
    m_irqActive = false;
}

u8 Mapper019::cpuRead(u16 address) {
    auto& prg = m_cartridge->getPRG();
    
    if (address >= 0x6000 && address < 0x8000) {
        return m_cartridge->getPRGRAM()[address & 0x1FFF];
    }
    
    if (prg.empty()) return 0;
    
    u32 prgBanks8k = prg.size() / 0x2000;
    u32 offset = 0;
    
    if (address >= 0x8000 && address < 0xA000) {
        offset = (m_prgBank[0] % prgBanks8k) * 0x2000;
        return prg[offset + (address & 0x1FFF)];
    } else if (address >= 0xA000 && address < 0xC000) {
        offset = (m_prgBank[1] % prgBanks8k) * 0x2000;
        return prg[offset + (address & 0x1FFF)];
    } else if (address >= 0xC000 && address < 0xE000) {
        offset = (m_prgBank[2] % prgBanks8k) * 0x2000;
        return prg[offset + (address & 0x1FFF)];
    } else if (address >= 0xE000) {
        // Last bank fixed
        offset = (prgBanks8k - 1) * 0x2000;
        return prg[offset + (address & 0x1FFF)];
    }
    
    return 0;
}

void Mapper019::cpuWrite(u16 address, u8 value) {
    if (address >= 0x6000 && address < 0x8000) {
        m_cartridge->getPRGRAM()[address & 0x1FFF] = value;
        return;
    }
    
    switch (address & 0xF800) {
        case 0x4800:
            // Namco 163 audio registers would live here; not implemented.
            break;
            
        case 0x5000:
            // IRQ counter low byte
            m_irqCounter = (m_irqCounter & 0x7F00) | value;
            m_irqActive = false;
            break;
            
        case 0x5800:
            // IRQ counter high bits + enable (bit 7)
            m_irqCounter = (m_irqCounter & 0x00FF) | ((value & 0x7F) << 8);
            m_irqEnable = (value & 0x80) != 0;
            m_irqActive = false;
            break;
            
        case 0x8000: case 0x8800: case 0x9000: case 0x9800:
        case 0xA000: case 0xA800: case 0xB000: case 0xB800: {
            u8 bankIndex = static_cast<u8>((address - 0x8000) >> 11);
            updateChrMapping(bankIndex, value);
            break;
        }
        
        case 0xE000:
            // PRG @ $8000
            m_prgBank[0] = value;
            break;
        case 0xE800:
            // PRG @ $A000
            m_prgBank[1] = value;
            break;
        case 0xF000:
            // PRG @ $C000
            m_prgBank[2] = value;
            break;
            
        case 0xF800:
            // WRAM write-protect in hardware; ignored here.
            break;
    }
}

u8 Mapper019::readCHR(u16 address) {
    u8 bank = address / 0x0400;
    u16 offset = address & 0x03FF;
    
    if (m_chrUseCiram[bank]) {
        // Map to internal CIRAM (nametable). Only two 1KB pages exist; use bit 0.
        u16 ciramAddr = (m_chrBank[bank] & 0x01) * 0x400 + offset;
        return m_cartridge->readCIRAM(ciramAddr);
    }
    
    auto& chr = m_cartridge->getCHR();
    if (chr.empty()) return 0;
    
    u32 chrBanks1k = chr.size() / 0x0400;
    u32 base = (m_chrBank[bank] % chrBanks1k) * 0x0400;
    return chr[base + offset];
}

void Mapper019::writeCHR(u16 address, u8 value) {
    u8 bank = address / 0x0400;
    u16 offset = address & 0x03FF;
    
    if (m_chrUseCiram[bank]) {
        // Ignore writes to CIRAM-mapped CHR
        return;
    }
    
    auto& chr = m_cartridge->getCHR();
    if (chr.empty()) return;
    
    u32 chrBanks1k = chr.size() / 0x0400;
    u32 base = (m_chrBank[bank] % chrBanks1k) * 0x0400;
    chr[base + offset] = value;
}

void Mapper019::clockAudio() {
    // Clocked every CPU cycle from APU::step
    if (!m_irqEnable) {
        return;
    }
    
    if (m_irqCounter != 0x7FFF) {
        m_irqCounter++;
        m_irqCounter &= 0x7FFF;  // keep to 15 bits
        if (m_irqCounter == 0x7FFF) {
            m_irqActive = true;
        }
    }
}

void Mapper019::saveState(std::ofstream& file) const {
    file.write(reinterpret_cast<const char*>(m_prgBank), sizeof(m_prgBank));
    file.write(reinterpret_cast<const char*>(m_chrBank), sizeof(m_chrBank));
    file.write(reinterpret_cast<const char*>(m_chrUseCiram), sizeof(m_chrUseCiram));
    file.write(reinterpret_cast<const char*>(&m_irqCounter), sizeof(m_irqCounter));
    file.write(reinterpret_cast<const char*>(&m_irqEnable), sizeof(m_irqEnable));
}

void Mapper019::loadState(std::ifstream& file) {
    file.read(reinterpret_cast<char*>(m_prgBank), sizeof(m_prgBank));
    file.read(reinterpret_cast<char*>(m_chrBank), sizeof(m_chrBank));
    file.read(reinterpret_cast<char*>(m_chrUseCiram), sizeof(m_chrUseCiram));
    file.read(reinterpret_cast<char*>(&m_irqCounter), sizeof(m_irqCounter));
    file.read(reinterpret_cast<char*>(&m_irqEnable), sizeof(m_irqEnable));
}

void Mapper019::updateChrMapping(u8 bankIndex, u8 value) {
    if (bankIndex >= 8) return;
    m_chrBank[bankIndex] = value;
    m_chrUseCiram[bankIndex] = (value >= 0xE0);
}

} // namespace nes



#include "mapper073.h"
#include "../consts.h"

namespace nes {

Mapper073::Mapper073(Cartridge* cartridge)
    : Mapper(cartridge) {
}

void Mapper073::reset() {
    Mapper::reset();
    m_prgBank = 0;
    m_irqLatch = 0;
    m_irqCounter = 0;
    m_irqEnable = false;
    m_irqEnableOnAck = false;
    m_irqMode = false;
    m_irqActive = false;
}

u8 Mapper073::cpuRead(u16 address) {
    const auto& prg = m_cartridge->getPRG();
    
    if (address >= 0x6000 && address < 0x8000) {
        return m_cartridge->getPRGRAM()[address & 0x1FFF];
    } else if (address >= 0x8000 && address < 0xC000) {
        // Switchable 16KB bank
        u32 prgBanks16k = prg.size() / 0x4000;
        u32 offset = (m_prgBank % prgBanks16k) * 0x4000;
        return prg[offset + (address & 0x3FFF)];
    } else if (address >= 0xC000) {
        // Fixed to last 16KB bank
        return prg[prg.size() - 0x4000 + (address & 0x3FFF)];
    }
    return 0;
}

void Mapper073::cpuWrite(u16 address, u8 value) {
    if (address >= 0x6000 && address < 0x8000) {
        m_cartridge->getPRGRAM()[address & 0x1FFF] = value;
        return;
    }
    
    switch (address & 0xF000) {
        case 0x8000:
            // IRQ latch bits 0-3
            m_irqLatch = (m_irqLatch & 0xFFF0) | (value & 0x0F);
            break;
        case 0x9000:
            // IRQ latch bits 4-7
            m_irqLatch = (m_irqLatch & 0xFF0F) | ((value & 0x0F) << 4);
            break;
        case 0xA000:
            // IRQ latch bits 8-11
            m_irqLatch = (m_irqLatch & 0xF0FF) | ((value & 0x0F) << 8);
            break;
        case 0xB000:
            // IRQ latch bits 12-15
            m_irqLatch = (m_irqLatch & 0x0FFF) | ((value & 0x0F) << 12);
            break;
        case 0xC000:
            // IRQ control
            m_irqEnableOnAck = (value & 0x01) != 0;
            m_irqEnable = (value & 0x02) != 0;
            m_irqMode = (value & 0x04) != 0;  // 0 = 16-bit, 1 = 8-bit (low byte only)
            if (m_irqEnable) {
                m_irqCounter = m_irqLatch;
            }
            m_irqActive = false;
            break;
        case 0xD000:
            // IRQ acknowledge
            m_irqEnable = m_irqEnableOnAck;
            m_irqActive = false;
            break;
        case 0xF000:
            // PRG bank select
            m_prgBank = value & 0x0F;
            break;
    }
}

u8 Mapper073::readCHR(u16 address) {
    return m_cartridge->getCHR()[address & 0x1FFF];
}

void Mapper073::writeCHR(u16 address, u8 value) {
    m_cartridge->getCHR()[address & 0x1FFF] = value;
}

void Mapper073::scanlineCounter() {
    // VRC3 IRQ is clocked by CPU cycles, not by scanlines/A12 edges.
    // Actual ticking happens in clockAudio() which is run every CPU cycle.
}

void Mapper073::clockAudio() {
    if (!m_irqEnable) {
        return;
    }
    
    // Clock the IRQ counter once per CPU cycle (called from APU::step).
    if (m_irqMode) {
        // 8-bit mode: increment the low byte only
        u8 low = static_cast<u8>(m_irqCounter & 0x00FF);
        low++;
        if (low == 0x00) {
            // Overflow: reload low byte from latch and assert IRQ
            m_irqCounter = (m_irqCounter & 0xFF00) | (m_irqLatch & 0x00FF);
            m_irqActive = true;
        } else {
            m_irqCounter = (m_irqCounter & 0xFF00) | low;
        }
    } else {
        // 16-bit mode: increment full counter
        m_irqCounter++;
        if (m_irqCounter == 0x0000) {
            m_irqCounter = m_irqLatch;
            m_irqActive = true;
        }
    }
}

void Mapper073::saveState(std::ofstream& file) const {
    Mapper::saveState(file);
    file.write(reinterpret_cast<const char*>(&m_prgBank), sizeof(m_prgBank));
    file.write(reinterpret_cast<const char*>(&m_irqLatch), sizeof(m_irqLatch));
    file.write(reinterpret_cast<const char*>(&m_irqCounter), sizeof(m_irqCounter));
    file.write(reinterpret_cast<const char*>(&m_irqEnable), sizeof(m_irqEnable));
    file.write(reinterpret_cast<const char*>(&m_irqEnableOnAck), sizeof(m_irqEnableOnAck));
    file.write(reinterpret_cast<const char*>(&m_irqMode), sizeof(m_irqMode));
}

void Mapper073::loadState(std::ifstream& file) {
    Mapper::loadState(file);
    file.read(reinterpret_cast<char*>(&m_prgBank), sizeof(m_prgBank));
    file.read(reinterpret_cast<char*>(&m_irqLatch), sizeof(m_irqLatch));
    file.read(reinterpret_cast<char*>(&m_irqCounter), sizeof(m_irqCounter));
    file.read(reinterpret_cast<char*>(&m_irqEnable), sizeof(m_irqEnable));
    file.read(reinterpret_cast<char*>(&m_irqEnableOnAck), sizeof(m_irqEnableOnAck));
    file.read(reinterpret_cast<char*>(&m_irqMode), sizeof(m_irqMode));
}

} // namespace nes


#include "mapper069.h"
#include "../consts.h"
#include <cstring>

namespace nes {

Mapper069::Mapper069(Cartridge* cartridge)
    : Mapper(cartridge)
    , m_workRam(0x8000, 0) {  // 32KB work RAM
}

void Mapper069::reset() {
    Mapper::reset();
    m_command = 0;
    m_workRamValue = 0;
    m_irqEnabled = false;
    m_irqCounterEnabled = false;
    m_irqCounter = 0;
    m_irqActive = false;
    std::memset(m_chrBanks, 0, sizeof(m_chrBanks));
    std::memset(m_prgBanks, 0, sizeof(m_prgBanks));
    std::fill(m_workRam.begin(), m_workRam.end(), 0);
    updateBanks();
    updateWorkRam();
}

void Mapper069::updateBanks() {
    const auto& prg = m_cartridge->getPRG();
    const auto& chr = m_cartridge->getCHR();
    
    u32 prgBanks = prg.size() / 0x2000;  // 8KB banks
    u32 chrBanks = chr.size() / 0x400;   // 1KB banks
    
    // PRG: 3 switchable banks at $8000, $A000, $C000, last bank fixed at $E000
    if (prgBanks > 0) {
        m_prgBankOffset[0] = (m_prgBanks[0] % prgBanks) * 0x2000;
        m_prgBankOffset[1] = (m_prgBanks[1] % prgBanks) * 0x2000;
        m_prgBankOffset[2] = (m_prgBanks[2] % prgBanks) * 0x2000;
        m_prgBankOffset[3] = (prgBanks - 1) * 0x2000;
    } else {
        m_prgBankOffset[0] = 0;
        m_prgBankOffset[1] = 0;
        m_prgBankOffset[2] = 0;
        m_prgBankOffset[3] = 0;
    }
    
    // CHR: 8 banks of 1KB
    if (chrBanks > 0) {
        for (int i = 0; i < 8; i++) {
            m_chrBankOffset[i] = (m_chrBanks[i] % chrBanks) * 0x400;
        }
    } else {
        for (int i = 0; i < 8; i++) {
            m_chrBankOffset[i] = 0;
        }
    }
}

void Mapper069::updateWorkRam() {
    // Work RAM mapping: $6000-$7FFF
    // Bit 6: Enable work RAM (1 = work RAM, 0 = PRG ROM)
    // Bit 7: When work RAM enabled, access type (1 = ReadWrite, 0 = ReadOnly)
    // Bits 0-5: Page number (0x3F = 6 bits, wraps since we have 32KB = 4 pages of 8KB)
    // Note: updateWorkRam() is called to update state, actual mapping handled in cpuRead/cpuWrite
    (void)m_workRamValue;
}

u8 Mapper069::cpuRead(u16 address) {
    if (address >= 0x6000 && address < 0x8000) {
        // Work RAM area
        if (m_workRamValue & 0x40) {
            // Work RAM enabled
            // Use bits 0-5 for page selection (wraps to 4 pages of 8KB each)
            u8 page = (m_workRamValue & 0x3F) % 4;  // 32KB total = 4 pages
            u16 offset = address & 0x1FFF;
            return m_workRam[page * 0x2000 + offset];
        } else {
            // PRG ROM mapped here
            const auto& prg = m_cartridge->getPRG();
            u32 prgBanks = prg.size() / 0x2000;
            if (prgBanks > 0) {
                u8 page = (m_workRamValue & 0x3F) % prgBanks;
                return prg[page * 0x2000 + (address & 0x1FFF)];
            }
            return 0;
        }
    } else if (address >= 0x8000) {
        u8 bank = (address - 0x8000) / 0x2000;
        u16 offset = address & 0x1FFF;
        return m_cartridge->getPRG()[m_prgBankOffset[bank] + offset];
    }
    return 0;
}

void Mapper069::cpuWrite(u16 address, u8 value) {
    if (address >= 0x6000 && address < 0x8000) {
        // Work RAM area
        if ((m_workRamValue & 0x40) && (m_workRamValue & 0x80)) {
            // Work RAM enabled AND bit 7 set = ReadWrite access
            // Use bits 0-5 for page selection (wraps to 4 pages of 8KB each)
            u8 page = (m_workRamValue & 0x3F) % 4;  // 32KB total = 4 pages
            u16 offset = address & 0x1FFF;
            m_workRam[page * 0x2000 + offset] = value;
        }
        // If bit 7 is clear, it's read-only (NoAccess), so don't write
    } else if (address >= 0x8000 && address < 0xA000) {
        // Command register ($8000-$9FFF)
        m_command = value & 0x0F;
    } else if (address >= 0xA000 && address < 0xC000) {
        // Data register ($A000-$BFFF)
        switch (m_command) {
            case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 7:
                // CHR bank select (commands 0-7)
                m_chrBanks[m_command] = value;
                updateBanks();
                break;
                
            case 8:
                // Work RAM register
                m_workRamValue = value;
                updateWorkRam();
                break;
                
            case 9: case 0xA: case 0xB:
                // PRG bank select (commands 9-A-B)
                m_prgBanks[m_command - 9] = value & 0x3F;
                updateBanks();
                break;
                
            case 0xC:
                // Mirroring
                switch (value & 0x03) {
                    case 0: m_mirrorMode = MirrorMode::VERTICAL; break;
                    case 1: m_mirrorMode = MirrorMode::HORIZONTAL; break;
                    case 2: m_mirrorMode = MirrorMode::SINGLE_SCREEN_A; break;
                    case 3: m_mirrorMode = MirrorMode::SINGLE_SCREEN_B; break;
                }
                break;
                
            case 0xD:
                // IRQ control
                m_irqEnabled = (value & 0x01) != 0;
                m_irqCounterEnabled = (value & 0x80) != 0;
                m_irqActive = false;
                break;
                
            case 0xE:
                // IRQ counter low byte
                m_irqCounter = (m_irqCounter & 0xFF00) | value;
                break;
                
            case 0xF:
                // IRQ counter high byte
                m_irqCounter = (m_irqCounter & 0x00FF) | (static_cast<u16>(value) << 8);
                break;
        }
    } else if (address >= 0xC000 && address < 0xE000) {
        // Audio register ($C000-$DFFF) - not implemented
        (void)value;
    } else if (address >= 0xE000) {
        // Audio register ($E000-$FFFF) - not implemented
        (void)value;
    }
}

u8 Mapper069::readCHR(u16 address) {
    u8 bank = address / 0x400;
    u16 offset = address & 0x03FF;
    return m_cartridge->getCHR()[m_chrBankOffset[bank] + offset];
}

void Mapper069::writeCHR(u16 address, u8 value) {
    // CHR ROM - ignore writes (or handle CHR RAM if present)
    (void)address;
    (void)value;
}

void Mapper069::clockAudio() {
    // FME-7 IRQ counter decrements on CPU cycles
    // This is called every CPU cycle from APU::step()
    if (m_irqCounterEnabled) {
        m_irqCounter--;
        // When counter wraps from 0 to 0xFFFF, trigger IRQ if enabled
        if (m_irqCounter == 0xFFFF) {
            if (m_irqEnabled) {
                m_irqActive = true;
            }
        }
    }
}

void Mapper069::saveState(Buffer* buf) {
    Mapper::saveState(buf);
    buffer_write(buf, &m_command, sizeof(m_command));
    buffer_write(buf, m_chrBanks, sizeof(m_chrBanks));
    buffer_write(buf, m_prgBanks, sizeof(m_prgBanks));
    buffer_write(buf, &m_workRamValue, sizeof(m_workRamValue));
    buffer_write(buf, &m_irqEnabled, sizeof(m_irqEnabled));
    buffer_write(buf, &m_irqCounterEnabled, sizeof(m_irqCounterEnabled));
    buffer_write(buf, &m_irqCounter, sizeof(m_irqCounter));
    
    // Save work RAM
    u32 workRamSize = static_cast<u32>(m_workRam.size());
    buffer_write(buf, &workRamSize, sizeof(workRamSize));
    buffer_write(buf, m_workRam.data(), workRamSize);
}

void Mapper069::loadState(Buffer* buf) {
    Mapper::loadState(buf);
    buffer_read(buf, &m_command, sizeof(m_command));
    buffer_read(buf, m_chrBanks, sizeof(m_chrBanks));
    buffer_read(buf, m_prgBanks, sizeof(m_prgBanks));
    buffer_read(buf, &m_workRamValue, sizeof(m_workRamValue));
    buffer_read(buf, &m_irqEnabled, sizeof(m_irqEnabled));
    buffer_read(buf, &m_irqCounterEnabled, sizeof(m_irqCounterEnabled));
    buffer_read(buf, &m_irqCounter, sizeof(m_irqCounter));
    
    // Load work RAM
    u32 workRamSize;
    buffer_read(buf, &workRamSize, sizeof(workRamSize));
    if (workRamSize != m_workRam.size()) {
        m_workRam.resize(workRamSize);
    }
    buffer_read(buf, m_workRam.data(), workRamSize);
    
    updateBanks();
    updateWorkRam();
}

} // namespace nes

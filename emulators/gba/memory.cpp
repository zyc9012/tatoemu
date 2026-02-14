#include "memory.h"
#include "cartridge.h"
#include "ppu.h"
#include "joypad.h"
#include "timer.h"
#include "dma.h"
#include "apu.h"
#include "cpu.h"
#include <cstring>

namespace gba {

Memory::Memory() {
    reset();
}

Memory::~Memory() {}

void Memory::reset() {
    std::memset(m_ewram, 0, sizeof(m_ewram));
    std::memset(m_iwram, 0, sizeof(m_iwram));
    std::memset(m_io, 0, sizeof(m_io));
    std::memset(m_palette, 0, sizeof(m_palette));
    std::memset(m_vram, 0, sizeof(m_vram));
    std::memset(m_oam, 0, sizeof(m_oam));
    m_halted = false;
    m_openBus = 0;
    if (!m_hasBIOS) {
        initEmbeddedBIOS();
    }
}

bool Memory::loadBIOS(const u8* data, u32 size) {
    if (size != BIOS_SIZE) return false;
    std::memcpy(m_bios, data, BIOS_SIZE);
    m_hasBIOS = true;
    return true;
}

void Memory::initEmbeddedBIOS() {
    // Minimal embedded BIOS - just enough to skip the intro
    // Set up a simple jump table at the SWI vector locations
    // Real BIOS emulation would be much more complex
    m_hasBIOS = false;
}

u8 Memory::read8(u32 address) {
    u32 region = (address >> 24) & 0xF;
    u32 offset = address & 0xFFFFFF;

    switch (region) {
        case REGION_BIOS:
            if (offset < BIOS_SIZE) return m_bios[offset];
            return m_openBus >> ((address & 3) * 8);
        case REGION_EWRAM:
            return m_ewram[offset & 0x3FFFF];
        case REGION_IWRAM:
            return m_iwram[offset & 0x7FFF];
        case REGION_IO:
            return readIO(offset & 0x3FF);
        case REGION_PALETTE:
            return m_palette[offset & 0x3FF];
        case REGION_VRAM:
            offset &= 0x1FFFF;
            if (offset >= 0x18000) offset &= 0x17FFF; // Mirror
            return m_vram[offset];
        case REGION_OAM:
            return m_oam[offset & 0x3FF];
        case REGION_ROM0:
        case REGION_ROM0H:
        case REGION_ROM1:
        case REGION_ROM1H:
        case REGION_ROM2:
        case REGION_ROM2H:
            if (m_cartridge && offset < m_cartridge->getROMSize()) {
                return m_cartridge->getROM()[offset & (m_cartridge->getROMSize() - 1)];
            }
            return 0xFF;
        case REGION_SRAM:
            if (m_cartridge) return m_cartridge->readSave(offset);
            return 0xFF;
        default:
            return m_openBus >> ((address & 3) * 8);
    }
}

u16 Memory::read16(u32 address) {
    address &= ~1;
    u32 region = (address >> 24) & 0xF;
    u32 offset = address & 0xFFFFFF;

    switch (region) {
        case REGION_BIOS:
            if (offset < BIOS_SIZE - 1) {
                return *reinterpret_cast<u16*>(&m_bios[offset]);
            }
            return m_openBus & 0xFFFF;
        case REGION_EWRAM:
            return *reinterpret_cast<u16*>(&m_ewram[offset & 0x3FFFF]);
        case REGION_IWRAM:
            return *reinterpret_cast<u16*>(&m_iwram[offset & 0x7FFF]);
        case REGION_IO:
            return readIO16(offset & 0x3FF);
        case REGION_PALETTE:
            return *reinterpret_cast<u16*>(&m_palette[offset & 0x3FF]);
        case REGION_VRAM:
            offset &= 0x1FFFF;
            if (offset >= 0x18000) offset &= 0x17FFF;
            return *reinterpret_cast<u16*>(&m_vram[offset]);
        case REGION_OAM:
            return *reinterpret_cast<u16*>(&m_oam[offset & 0x3FF]);
        case REGION_ROM0:
        case REGION_ROM0H:
        case REGION_ROM1:
        case REGION_ROM1H:
        case REGION_ROM2:
        case REGION_ROM2H:
            // Check for EEPROM access in ROM2_EX region  (0x0D000000)
            if (region == REGION_ROM2H && m_cartridge->hasEEPROM()) {
                u16 data = m_cartridge->readEEPROM();
                return data | (data << 8); // Replicate to full 16-bit
            }
            if (offset < m_cartridge->getROMSize()) {
                offset &= (m_cartridge->getROMSize() - 1);
                return *reinterpret_cast<u16*>(&m_cartridge->getROM()[offset]);
            }
            return 0xFFFF;
        case REGION_SRAM:
            return (m_cartridge ? m_cartridge->readSave(offset) : 0xFF) * 0x0101;
        default:
            return m_openBus & 0xFFFF;
    }
}

u32 Memory::read32(u32 address) {
    address &= ~3;
    u32 region = (address >> 24) & 0xF;
    u32 offset = address & 0xFFFFFF;

    switch (region) {
        case REGION_BIOS:
            if (offset < BIOS_SIZE - 3) {
                return *reinterpret_cast<u32*>(&m_bios[offset]);
            }
            return m_openBus;
        case REGION_EWRAM:
            return *reinterpret_cast<u32*>(&m_ewram[offset & 0x3FFFF]);
        case REGION_IWRAM:
            return *reinterpret_cast<u32*>(&m_iwram[offset & 0x7FFF]);
        case REGION_IO:
            return readIO32(offset & 0x3FF);
        case REGION_PALETTE:
            return *reinterpret_cast<u32*>(&m_palette[offset & 0x3FF]);
        case REGION_VRAM:
            offset &= 0x1FFFF;
            if (offset >= 0x18000) offset &= 0x17FFF;
            return *reinterpret_cast<u32*>(&m_vram[offset]);
        case REGION_OAM:
            return *reinterpret_cast<u32*>(&m_oam[offset & 0x3FF]);
        case REGION_ROM0:
        case REGION_ROM0H:
        case REGION_ROM1:
        case REGION_ROM1H:
        case REGION_ROM2:
        case REGION_ROM2H:
            if (m_cartridge && offset < m_cartridge->getROMSize()) {
                offset &= (m_cartridge->getROMSize() - 1);
                return *reinterpret_cast<u32*>(&m_cartridge->getROM()[offset]);
            }
            return 0xFFFFFFFF;
        case REGION_SRAM:
            if (m_cartridge) {
                u8 val = m_cartridge->readSave(offset);
                return val * 0x01010101;
            }
            return 0xFFFFFFFF;
        default:
            return m_openBus;
    }
}

void Memory::write8(u32 address, u8 value) {
    u32 region = (address >> 24) & 0xF;
    u32 offset = address & 0xFFFFFF;

    switch (region) {
        case REGION_EWRAM:
            m_ewram[offset & 0x3FFFF] = value;
            break;
        case REGION_IWRAM:
            m_iwram[offset & 0x7FFF] = value;
            break;
        case REGION_IO:
            writeIO(offset & 0x3FF, value);
            break;
        case REGION_PALETTE:
            m_palette[offset & 0x3FF] = value;
            break;
        case REGION_VRAM:
            offset &= 0x1FFFF;
            if (offset >= 0x18000) offset &= 0x17FFF;
            if (offset < 0x14000 || (offset >= 0x14000 && (offset & 1) == 0)) {
                m_vram[offset] = value;
            }
            break;
        case REGION_SRAM:
            if (m_cartridge) m_cartridge->writeSave(offset, value);
            break;
        default:
            break;
    }
}

void Memory::write16(u32 address, u16 value) {
    address &= ~1;
    u32 region = (address >> 24) & 0xF;
    u32 offset = address & 0xFFFFFF;

    switch (region) {
        case REGION_EWRAM:
            *reinterpret_cast<u16*>(&m_ewram[offset & 0x3FFFF]) = value;
            break;
        case REGION_IWRAM:
            *reinterpret_cast<u16*>(&m_iwram[offset & 0x7FFF]) = value;
            break;
        case REGION_IO:
            writeIO16(offset & 0x3FF, value);
            break;
        case REGION_PALETTE:
            *reinterpret_cast<u16*>(&m_palette[offset & 0x3FF]) = value;
            break;
        case REGION_VRAM:
            offset &= 0x1FFFF;
            if (offset >= 0x18000) offset &= 0x17FFF;
            *reinterpret_cast<u16*>(&m_vram[offset]) = value;
            break;
        case REGION_OAM:
            *reinterpret_cast<u16*>(&m_oam[offset & 0x3FF]) = value;
            break;
        case REGION_SRAM:
            if (m_cartridge) {
                m_cartridge->writeSave(offset, value & 0xFF);
            }
            break;
        default:
            break;
    }
}

void Memory::write32(u32 address, u32 value) {
    address &= ~3;
    u32 region = (address >> 24) & 0xF;
    u32 offset = address & 0xFFFFFF;

    switch (region) {
        case REGION_EWRAM:
            *reinterpret_cast<u32*>(&m_ewram[offset & 0x3FFFF]) = value;
            break;
        case REGION_IWRAM:
            *reinterpret_cast<u32*>(&m_iwram[offset & 0x7FFF]) = value;
            break;
        case REGION_IO:
            writeIO16(offset & 0x3FF, value & 0xFFFF);
            writeIO16((offset + 2) & 0x3FF, value >> 16);
            break;
        case REGION_PALETTE:
            *reinterpret_cast<u32*>(&m_palette[offset & 0x3FF]) = value;
            break;
        case REGION_VRAM:
            offset &= 0x1FFFF;
            if (offset >= 0x18000) offset &= 0x17FFF;
            *reinterpret_cast<u32*>(&m_vram[offset]) = value;
            break;
        case REGION_OAM:
            *reinterpret_cast<u32*>(&m_oam[offset & 0x3FF]) = value;
            break;
        case REGION_SRAM:
            if (m_cartridge) {
                m_cartridge->writeSave(offset, value & 0xFF);
            }
            break;
        default:
            break;
    }
}

u16 Memory::fetch16(u32 address) {
    return read16(address);
}

u32 Memory::fetch32(u32 address) {
    return read32(address);
}

u8 Memory::readIO(u32 address) {
    if (address >= IO_SIZE) return 0;
    
    // Sound registers — route to APU
    if (m_apu && address >= IO::SOUND1CNT_L && address <= IO::WAVE_RAM + 0xF) {
        return m_apu->readRegister(address);
    }

    // Handle special read-only registers
    switch (address) {
        case IO::KEYINPUT:
        case IO::KEYINPUT + 1:
            if (m_joypad) return m_joypad->read() >> ((address & 1) * 8);
            break;
        case IO::VCOUNT:
        case IO::VCOUNT + 1:
            if (m_ppu) return m_ppu->getVCount() >> ((address & 1) * 8);
            break;
        case IO::TM0CNT_L: case IO::TM0CNT_L + 1:
            if (m_timer) return m_timer->readCounter(0) >> ((address & 1) * 8);
            break;
        case IO::TM1CNT_L: case IO::TM1CNT_L + 1:
            if (m_timer) return m_timer->readCounter(1) >> ((address & 1) * 8);
            break;
        case IO::TM2CNT_L: case IO::TM2CNT_L + 1:
            if (m_timer) return m_timer->readCounter(2) >> ((address & 1) * 8);
            break;
        case IO::TM3CNT_L: case IO::TM3CNT_L + 1:
            if (m_timer) return m_timer->readCounter(3) >> ((address & 1) * 8);
            break;
    }
    
    return m_io[address];
}

void Memory::writeIO(u32 address, u8 value) {
    if (address >= IO_SIZE) return;
    
    // Some registers are read-only or have special behavior
    switch (address) {
        case IO::VCOUNT:
        case IO::VCOUNT + 1:
        case IO::KEYINPUT:
        case IO::KEYINPUT + 1:
            return; // Read-only
        case IO::IF:
        case IO::IF + 1: {
            // Write-1-to-clear for IF (same behavior as 16-bit write)
            m_io[address] &= ~value;
            return;
        }
        case IO::HALTCNT:
            m_halted = (value & 0x80) == 0; // 0 = halt, 0x80 = stop
            return;
    }
    
    m_io[address] = value;
}

u16 Memory::readIO16(u32 offset) const {
    if (offset >= IO_SIZE - 1) return 0;
    
    // Handle special read-only hardware registers
    switch (offset) {
        case IO::VCOUNT:
            if (m_ppu) return m_ppu->getVCount();
            break;
        case IO::KEYINPUT:
            if (m_joypad) return m_joypad->read();
            break;
        case IO::TM0CNT_L:
            if (m_timer) return m_timer->readCounter(0);
            break;
        case IO::TM1CNT_L:
            if (m_timer) return m_timer->readCounter(1);
            break;
        case IO::TM2CNT_L:
            if (m_timer) return m_timer->readCounter(2);
            break;
        case IO::TM3CNT_L:
            if (m_timer) return m_timer->readCounter(3);
            break;
    }
    
    return *reinterpret_cast<const u16*>(&m_io[offset]);
}

void Memory::writeIO16(u32 offset, u16 value) {
    if (offset >= IO_SIZE - 1) return;
    
    // Forward to subsystems
    if (m_apu && offset >= IO::SOUND1CNT_L && offset <= IO::FIFO_B + 3) {
        m_apu->writeRegister(offset, value);
        // Also store to IO array for reads that bypass APU
        *reinterpret_cast<u16*>(&m_io[offset]) = value;
        return;
    } else if (m_ppu && offset >= IO::DISPCNT && offset < IO::SOUND1CNT_L) {
        m_ppu->writeRegister(offset, value);
    } else if (m_timer && offset >= IO::TM0CNT_L && offset <= IO::TM3CNT_H) {
        m_timer->writeRegister(offset, value);
    } else if (m_dma && offset >= IO::DMA0SAD && offset <= IO::DMA3CNT_H + 1) {
        // Write to IO array FIRST, then let DMA subsystem process
        // DMA::run() may clear the enable bit in the IO array when completing immediately
        *reinterpret_cast<u16*>(&m_io[offset]) = value;
        m_dma->writeRegister(offset, value);
        return; // Don't overwrite IO array again — DMA may have modified it
    }
    
    // Special handling
    switch (offset) {
        case IO::IF:
            // Writing 1 to IF bits clears them
            *reinterpret_cast<u16*>(&m_io[offset]) &= ~value;
            return;
        case IO::HALTCNT:
            m_halted = (value & 0x80) == 0;
            return;
    }
    
    *reinterpret_cast<u16*>(&m_io[offset]) = value;
}

u32 Memory::readIO32(u32 offset) const {
    if (offset >= IO_SIZE - 3) return 0;
    
    // Handle registers that need special read handling
    switch (offset) {
        case IO::VCOUNT:
            // DISPSTAT (low 16) | VCOUNT (high 16)
            return *reinterpret_cast<const u16*>(&m_io[IO::DISPSTAT]) |
                   (m_ppu ? (static_cast<u32>(m_ppu->getVCount()) << 16) : 0);
        case IO::KEYINPUT:
            return (m_joypad ? m_joypad->read() : 0xFFFF) |
                   (static_cast<u32>(*reinterpret_cast<const u16*>(&m_io[IO::KEYCNT])) << 16);
        case IO::TM0CNT_L:
            return (m_timer ? m_timer->readCounter(0) : 0) |
                   (static_cast<u32>(*reinterpret_cast<const u16*>(&m_io[IO::TM0CNT_H])) << 16);
        case IO::TM1CNT_L:
            return (m_timer ? m_timer->readCounter(1) : 0) |
                   (static_cast<u32>(*reinterpret_cast<const u16*>(&m_io[IO::TM1CNT_H])) << 16);
        case IO::TM2CNT_L:
            return (m_timer ? m_timer->readCounter(2) : 0) |
                   (static_cast<u32>(*reinterpret_cast<const u16*>(&m_io[IO::TM2CNT_H])) << 16);
        case IO::TM3CNT_L:
            return (m_timer ? m_timer->readCounter(3) : 0) |
                   (static_cast<u32>(*reinterpret_cast<const u16*>(&m_io[IO::TM3CNT_H])) << 16);
    }
    
    return *reinterpret_cast<const u32*>(&m_io[offset]);
}

void Memory::requestIRQ(u16 irqBit) {
    // Write directly to m_io array, bypassing writeIO16's write-1-to-clear logic for IF
    u16 IF = *reinterpret_cast<u16*>(&m_io[IO::IF]);
    IF |= irqBit;
    *reinterpret_cast<u16*>(&m_io[IO::IF]) = IF;
    
    u16 IE = readIO16(IO::IE);
    u16 IME = readIO16(IO::IME);
    
    // Wake from halt if this IRQ is enabled
    if ((IE & irqBit) && (IME & 1)) {
        m_halted = false;
    }
}

void Memory::saveState(Buffer* buf) {
    buffer_write(buf, m_ewram, sizeof(m_ewram));
    buffer_write(buf, m_iwram, sizeof(m_iwram));
    buffer_write(buf, m_io, sizeof(m_io));
    buffer_write(buf, m_palette, sizeof(m_palette));
    buffer_write(buf, m_vram, sizeof(m_vram));
    buffer_write(buf, m_oam, sizeof(m_oam));
    buffer_write(buf, &m_halted, sizeof(m_halted));
    buffer_write(buf, &m_openBus, sizeof(m_openBus));
}

void Memory::loadState(Buffer* buf) {
    buffer_read(buf, m_ewram, sizeof(m_ewram));
    buffer_read(buf, m_iwram, sizeof(m_iwram));
    buffer_read(buf, m_io, sizeof(m_io));
    buffer_read(buf, m_palette, sizeof(m_palette));
    buffer_read(buf, m_vram, sizeof(m_vram));
    buffer_read(buf, m_oam, sizeof(m_oam));
    buffer_read(buf, &m_halted, sizeof(m_halted));
    buffer_read(buf, &m_openBus, sizeof(m_openBus));
}

} // namespace gba

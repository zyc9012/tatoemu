#include "memory.h"
#include "cartridge.h"
#include "ppu.h"
#include "joypad.h"
#include "timer.h"
#include "dma.h"
#include "apu.h"
#include "gpio.h"
#include "cpu.h"
#include <cstring>

namespace gba {

Memory::Memory() {
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
    m_waitCycles = 0;
    m_exWaitcnt = 0x0D000020; // Default EXWAITCNT
    m_prefetchEnabled = false;
    m_prefetchCount = 0;
    m_prefetchHeadAddr = 0;
    m_fetchRegion = 0;
    m_lastFetchAddr = ~0u;
    m_isFetch = false;
    updateWaitstates(0); // Default WAITCNT = 0
}

// WAITCNT wait state lookup tables
static const int ROM_WS_NONSEQ[] = { 4, 3, 2, 8 };   // SRAM & ROM non-sequential first access
static const int ROM_WS_SEQ[]    = { 2, 1, 4, 1, 8, 1 }; // ROM sequential (WS0:idx0-1, WS1:idx2-3, WS2:idx4-5)

void Memory::updateWaitstates(u16 waitcnt) {
    int sram   = waitcnt & 0x3;
    int ws0    = (waitcnt >> 2) & 0x3;
    int ws0seq = (waitcnt >> 4) & 0x1;
    int ws1    = (waitcnt >> 5) & 0x3;
    int ws1seq = (waitcnt >> 7) & 0x1;
    int ws2    = (waitcnt >> 8) & 0x3;
    int ws2seq = (waitcnt >> 10) & 0x1;

    // Base wait states for non-ROM/SRAM regions (16-bit / 32-bit)
    // BIOS=0, EWRAM=2/5, IWRAM=0, IO=0, Palette=0/1, VRAM=0/1, OAM=0
    static const int BASE_WS16[16] = { 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    static const int BASE_WS32[16] = { 0, 0, 5, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

    for (int i = 0; i < 16; i++) {
        m_wsNonseq16[i] = BASE_WS16[i];
        m_wsNonseq32[i] = BASE_WS32[i];
        m_wsSeq16[i] = BASE_WS16[i];
        m_wsSeq32[i] = BASE_WS32[i];
    }

    // SRAM
    m_wsNonseq16[REGION_SRAM] = m_wsNonseq16[REGION_SRAM_MIRROR] = ROM_WS_NONSEQ[sram];
    m_wsSeq16[REGION_SRAM]    = m_wsSeq16[REGION_SRAM_MIRROR]    = ROM_WS_NONSEQ[sram];
    m_wsNonseq32[REGION_SRAM] = m_wsNonseq32[REGION_SRAM_MIRROR] = 2 * ROM_WS_NONSEQ[sram] + 1;
    m_wsSeq32[REGION_SRAM]    = m_wsSeq32[REGION_SRAM_MIRROR]    = 2 * ROM_WS_NONSEQ[sram] + 1;

    // ROM Wait State 0
    m_wsNonseq16[REGION_ROM0] = m_wsNonseq16[REGION_ROM0H] = ROM_WS_NONSEQ[ws0];
    m_wsSeq16[REGION_ROM0]    = m_wsSeq16[REGION_ROM0H]    = ROM_WS_SEQ[ws0seq];

    // ROM Wait State 1
    m_wsNonseq16[REGION_ROM1] = m_wsNonseq16[REGION_ROM1H] = ROM_WS_NONSEQ[ws1];
    m_wsSeq16[REGION_ROM1]    = m_wsSeq16[REGION_ROM1H]    = ROM_WS_SEQ[ws1seq + 2];

    // ROM Wait State 2
    m_wsNonseq16[REGION_ROM2] = m_wsNonseq16[REGION_ROM2H] = ROM_WS_NONSEQ[ws2];
    m_wsSeq16[REGION_ROM2]    = m_wsSeq16[REGION_ROM2H]    = ROM_WS_SEQ[ws2seq + 4];

    // 32-bit ROM accesses = first-access + 1 + sequential-access (two 16-bit reads)
    m_wsNonseq32[REGION_ROM0] = m_wsNonseq32[REGION_ROM0H] = m_wsNonseq16[REGION_ROM0] + 1 + m_wsSeq16[REGION_ROM0];
    m_wsNonseq32[REGION_ROM1] = m_wsNonseq32[REGION_ROM1H] = m_wsNonseq16[REGION_ROM1] + 1 + m_wsSeq16[REGION_ROM1];
    m_wsNonseq32[REGION_ROM2] = m_wsNonseq32[REGION_ROM2H] = m_wsNonseq16[REGION_ROM2] + 1 + m_wsSeq16[REGION_ROM2];

    m_wsSeq32[REGION_ROM0] = m_wsSeq32[REGION_ROM0H] = 2 * m_wsSeq16[REGION_ROM0] + 1;
    m_wsSeq32[REGION_ROM1] = m_wsSeq32[REGION_ROM1H] = 2 * m_wsSeq16[REGION_ROM1] + 1;
    m_wsSeq32[REGION_ROM2] = m_wsSeq32[REGION_ROM2H] = 2 * m_wsSeq16[REGION_ROM2] + 1;

    m_prefetchEnabled = (waitcnt & 0x4000) != 0;
    if (!m_prefetchEnabled) {
        m_prefetchCount = 0;
    }
}

void Memory::updateEWRAMWaitstates(u16 value) {
    int wait = 15 - ((value >> 8) & 0xF);
    if (wait > 0) {
        m_wsNonseq16[REGION_EWRAM] = wait;
        m_wsSeq16[REGION_EWRAM]    = wait;
        m_wsNonseq32[REGION_EWRAM] = 2 * wait + 1;
        m_wsSeq32[REGION_EWRAM]    = 2 * wait + 1;
    }
}

void Memory::fillPrefetch(int availableCycles) {
    if (!m_prefetchEnabled || m_fetchRegion < REGION_ROM0 || m_fetchRegion > REGION_ROM2H)
        return;
    int seqCost = m_wsSeq16[m_fetchRegion] + 1;
    if (seqCost <= 0) return;
    while (availableCycles >= seqCost && m_prefetchCount < 8) {
        availableCycles -= seqCost;
        m_prefetchCount++;
    }
}

void Memory::prefetchStep(u32 region, int accessWait) {
    if (m_isFetch) return;
    if (region < REGION_ROM0) {
        // Non-ROM access: Game Pak bus is free, fill prefetch buffer
        fillPrefetch(accessWait + 1);
    } else {
        // ROM/SRAM access: Game Pak bus occupied, flush prefetch buffer
        m_prefetchCount = 0;
    }
}

bool Memory::loadBIOS(const u8* data, u32 size) {
    if (size != BIOS_SIZE) return false;
    std::memcpy(m_bios, data, BIOS_SIZE);
    m_hasBIOS = true;
    return true;
}

u8 Memory::read8(u32 address) {
    u32 region = (address >> 24) & 0xF;
    u32 offset = address & 0xFFFFFF;
    u8 value;
    m_waitCycles += m_wsNonseq16[region];

    switch (region) {
        case REGION_BIOS:
            if (offset < BIOS_SIZE) {
                value = m_bios[offset];
            } else {
                return m_openBus >> ((address & 3) * 8);
            }
            break;
        case REGION_EWRAM:
            value = m_ewram[offset & 0x3FFFF];
            break;
        case REGION_IWRAM:
            value = m_iwram[offset & 0x7FFF];
            break;
        case REGION_IO:
            if (offset >= 0x800 && offset <= 0x803) {
                value = (m_exWaitcnt >> ((offset & 3) * 8)) & 0xFF;
            } else {
                value = readIO(offset & 0x3FF);
            }
            break;
        case REGION_PALETTE:
            value = m_palette[offset & 0x3FF];
            break;
        case REGION_VRAM:
            offset &= 0x1FFFF;
            if (offset >= 0x18000) offset &= 0x17FFF; // Mirror
            value = m_vram[offset];
            break;
        case REGION_OAM:
            value = m_oam[offset & 0x3FF];
            break;
        case REGION_ROM0:
        case REGION_ROM0H:
        case REGION_ROM1:
        case REGION_ROM1H:
        case REGION_ROM2:
        case REGION_ROM2H:
            if (m_cartridge && offset < m_cartridge->getROMSize()) {
                value = m_cartridge->getROM()[offset & (m_cartridge->getROMSize() - 1)];
            } else {
                return m_openBus >> ((address & 3) * 8);
            }
            break;
        case REGION_SRAM:
        case REGION_SRAM_MIRROR:
            if (m_cartridge) {
                value = m_cartridge->readSave(offset);
            } else {
                return m_openBus >> ((address & 3) * 8);
            }
            break;
        default:
            return m_openBus >> ((address & 3) * 8);
    }

    return value;
}

u16 Memory::read16(u32 address) {
    address &= ~1;
    u32 region = (address >> 24) & 0xF;
    u32 offset = address & 0xFFFFFF;
    u16 value;
    m_waitCycles += m_wsNonseq16[region];
    prefetchStep(region, m_wsNonseq16[region]);

    switch (region) {
        case REGION_BIOS:
            if (offset < BIOS_SIZE - 1) {
                value = *reinterpret_cast<u16*>(&m_bios[offset]);
            } else {
                return m_openBus & 0xFFFF;
            }
            break;
        case REGION_EWRAM:
            value = *reinterpret_cast<u16*>(&m_ewram[offset & 0x3FFFF]);
            break;
        case REGION_IWRAM:
            value = *reinterpret_cast<u16*>(&m_iwram[offset & 0x7FFF]);
            break;
        case REGION_IO:
            if (offset == 0x800) {
                value = m_exWaitcnt & 0xFFFF;
            } else if (offset == 0x802) {
                value = (m_exWaitcnt >> 16) & 0xFFFF;
            } else {
                value = readIO16(offset & 0x3FF);
            }
            break;
        case REGION_PALETTE:
            value = *reinterpret_cast<u16*>(&m_palette[offset & 0x3FF]);
            break;
        case REGION_VRAM:
            offset &= 0x1FFFF;
            if (offset >= 0x18000) offset &= 0x17FFF;
            value = *reinterpret_cast<u16*>(&m_vram[offset]);
            break;
        case REGION_OAM:
            value = *reinterpret_cast<u16*>(&m_oam[offset & 0x3FF]);
            break;
        case REGION_ROM0:
        case REGION_ROM0H:
        case REGION_ROM1:
        case REGION_ROM1H:
        case REGION_ROM2:
        case REGION_ROM2H:
            // Check for EEPROM access in ROM2_EX region  (0x0D000000)
            if (region == REGION_ROM2H && m_cartridge->hasEEPROM()) {
                u16 data = m_cartridge->readEEPROM();
                value = data | (data << 8); // Replicate to full 16-bit
            } else if (offset < m_cartridge->getROMSize()) {
                offset &= (m_cartridge->getROMSize() - 1);
                value = *reinterpret_cast<u16*>(&m_cartridge->getROM()[offset]);
            } else {
                return (m_openBus >> ((address & 2) * 8)) & 0xFFFF;
            }
            break;
        case REGION_SRAM:
        case REGION_SRAM_MIRROR:
            value = (m_cartridge ? m_cartridge->readSave(offset) : 0xFF) * 0x0101;
            break;
        default:
            return (m_openBus >> ((address & 2) * 8)) & 0xFFFF;
    }

    return value;
}

u32 Memory::read32(u32 address) {
    address &= ~3;
    u32 region = (address >> 24) & 0xF;
    u32 offset = address & 0xFFFFFF;
    u32 value;
    m_waitCycles += m_wsNonseq32[region];
    prefetchStep(region, m_wsNonseq32[region]);

    switch (region) {
        case REGION_BIOS:
            if (offset < BIOS_SIZE - 3) {
                value = *reinterpret_cast<u32*>(&m_bios[offset]);
            } else {
                return m_openBus;
            }
            break;
        case REGION_EWRAM:
            value = *reinterpret_cast<u32*>(&m_ewram[offset & 0x3FFFF]);
            break;
        case REGION_IWRAM:
            value = *reinterpret_cast<u32*>(&m_iwram[offset & 0x7FFF]);
            break;
        case REGION_IO:
            if (offset == 0x800) {
                value = m_exWaitcnt;
            } else {
                value = readIO32(offset & 0x3FF);
            }
            break;
        case REGION_PALETTE:
            value = *reinterpret_cast<u32*>(&m_palette[offset & 0x3FF]);
            break;
        case REGION_VRAM:
            offset &= 0x1FFFF;
            if (offset >= 0x18000) offset &= 0x17FFF;
            value = *reinterpret_cast<u32*>(&m_vram[offset]);
            break;
        case REGION_OAM:
            value = *reinterpret_cast<u32*>(&m_oam[offset & 0x3FF]);
            break;
        case REGION_ROM0:
        case REGION_ROM0H:
        case REGION_ROM1:
        case REGION_ROM1H:
        case REGION_ROM2:
        case REGION_ROM2H:
            if (m_cartridge && offset < m_cartridge->getROMSize()) {
                offset &= (m_cartridge->getROMSize() - 1);
                value = *reinterpret_cast<u32*>(&m_cartridge->getROM()[offset]);
            } else {
                return m_openBus;
            }
            break;
        case REGION_SRAM:
        case REGION_SRAM_MIRROR:
            if (m_cartridge) {
                u8 val = m_cartridge->readSave(offset);
                value = val * 0x01010101;
            } else {
                return m_openBus;
            }
            break;
        default:
            return m_openBus;
    }

    return value;
}

void Memory::write8(u32 address, u8 value) {
    u32 region = (address >> 24) & 0xF;
    u32 offset = address & 0xFFFFFF;
    m_waitCycles += m_wsNonseq16[region];
    prefetchStep(region, m_wsNonseq16[region]);

    switch (region) {
        case REGION_EWRAM:
            m_ewram[offset & 0x3FFFF] = value;
            break;
        case REGION_IWRAM:
            m_iwram[offset & 0x7FFF] = value;
            break;
        case REGION_IO:
            if (offset >= 0x800 && offset <= 0x803) {
                int shift = (offset & 3) * 8;
                m_exWaitcnt = (m_exWaitcnt & ~(0xFFu << shift)) | (static_cast<u32>(value) << shift);
                if (offset >= 0x802) {
                    updateEWRAMWaitstates((m_exWaitcnt >> 16) & 0xFFFF);
                }
            } else {
                writeIO(offset & 0x3FF, value);
            }
            break;
        case REGION_PALETTE: {
            // Byte writes to palette RAM expand to both bytes of the halfword
            u32 palOffset = offset & 0x3FE;
            m_palette[palOffset] = value;
            m_palette[palOffset + 1] = value;
            break;
        }
        case REGION_VRAM: {
            offset &= 0x1FFFF;
            if (offset >= 0x18000) offset &= 0x17FFF;
            // BG/OBJ VRAM boundary depends on video mode
            u16 dispcnt = *reinterpret_cast<u16*>(&m_io[0]);
            int mode = dispcnt & 7;
            u32 objBoundary = (mode >= 3) ? 0x14000 : 0x10000;
            if (offset < objBoundary) {
                // BG VRAM: byte writes expand to both bytes of the halfword
                offset &= ~1;
                m_vram[offset] = value;
                m_vram[offset + 1] = value;
            }
            // OBJ VRAM: byte writes are ignored
            break;
        }
        case REGION_SRAM:
        case REGION_SRAM_MIRROR:
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
    m_waitCycles += m_wsNonseq16[region];
    prefetchStep(region, m_wsNonseq16[region]);

    switch (region) {
        case REGION_EWRAM:
            *reinterpret_cast<u16*>(&m_ewram[offset & 0x3FFFF]) = value;
            break;
        case REGION_IWRAM:
            *reinterpret_cast<u16*>(&m_iwram[offset & 0x7FFF]) = value;
            break;
        case REGION_IO:
            if (offset == 0x800) {
                m_exWaitcnt = (m_exWaitcnt & 0xFFFF0000) | value;
            } else if (offset == 0x802) {
                u16 masked = value & 0xFF00;
                m_exWaitcnt = (m_exWaitcnt & 0x0000FFFF) | (static_cast<u32>(masked) << 16);
                updateEWRAMWaitstates(masked);
            } else {
                writeIO16(offset & 0x3FF, value);
            }
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
        case REGION_ROM0:
        case REGION_ROM0H:
            // GPIO register writes (0x080000C4, 0x080000C6, 0x080000C8)
            if (m_gpio && GPIO::isGPIOAddress(offset)) {
                m_gpio->write(offset, value);
            }
            break;
        case REGION_ROM2H:
            if (m_cartridge->hasEEPROM()) {
                m_cartridge->writeEEPROM(value & 1, 1);
            }
            break;
        case REGION_SRAM:
        case REGION_SRAM_MIRROR:
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
    m_waitCycles += m_wsNonseq32[region];
    prefetchStep(region, m_wsNonseq32[region]);

    switch (region) {
        case REGION_EWRAM:
            *reinterpret_cast<u32*>(&m_ewram[offset & 0x3FFFF]) = value;
            break;
        case REGION_IWRAM:
            *reinterpret_cast<u32*>(&m_iwram[offset & 0x7FFF]) = value;
            break;
        case REGION_IO:
            if (offset == 0x800) {
                u16 lo = value & 0xFFFF;
                u16 hi = (value >> 16) & 0xFF00;
                m_exWaitcnt = lo | (static_cast<u32>(hi) << 16);
                updateEWRAMWaitstates(hi);
            } else {
                writeIO16(offset & 0x3FF, value & 0xFFFF);
                writeIO16((offset + 2) & 0x3FF, value >> 16);
            }
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
        case REGION_ROM0:
        case REGION_ROM0H:
            // GPIO register writes
            if (m_gpio) {
                if (GPIO::isGPIOAddress(offset)) {
                    m_gpio->write(offset, value & 0xFFFF);
                }
                if (GPIO::isGPIOAddress(offset + 2)) {
                    m_gpio->write(offset + 2, value >> 16);
                }
            }
            break;
        case REGION_SRAM:
        case REGION_SRAM_MIRROR:
            if (m_cartridge) {
                m_cartridge->writeSave(offset, value & 0xFF);
            }
            break;
        default:
            break;
    }
}

u16 Memory::fetch16(u32 address) {
    u16 value = read16(address);  // Adds m_wsNonseq16[region] to m_waitCycles
    u32 region = (address >> 24) & 0xF;
    bool isROM = (region >= REGION_ROM0 && region <= REGION_ROM2H);

    if (isROM) {
        if (m_prefetchEnabled && m_prefetchCount > 0 && address == m_prefetchHeadAddr) {
            // Prefetch buffer hit — remove ROM wait, costs 1 internal cycle
            m_waitCycles -= m_wsNonseq16[region];
            m_prefetchHeadAddr += 2;
            m_prefetchCount--;
        } else {
            // Buffer miss — convert N to S for sequential fetches
            bool isSeq = (address == m_lastFetchAddr + 2);
            if (isSeq) {
                m_waitCycles -= m_wsNonseq16[region];
                m_waitCycles += m_wsSeq16[region];
            }
            // Reset prefetch buffer
            m_prefetchCount = 0;
            m_prefetchHeadAddr = address + 2;
        }
    }

    m_fetchRegion = region;
    m_lastFetchAddr = address;
    m_openBus = static_cast<u32>(value) | (static_cast<u32>(value) << 16);
    return value;
}

u32 Memory::fetch32(u32 address) {
    u32 value = read32(address);  // Adds m_wsNonseq32[region] to m_waitCycles
    u32 region = (address >> 24) & 0xF;
    bool isROM = (region >= REGION_ROM0 && region <= REGION_ROM2H);

    if (isROM) {
        if (m_prefetchEnabled && m_prefetchCount >= 2 && address == m_prefetchHeadAddr) {
            // Both halfwords in prefetch buffer — remove ROM wait
            m_waitCycles -= m_wsNonseq32[region];
            m_prefetchHeadAddr += 4;
            m_prefetchCount -= 2;
        } else if (m_prefetchEnabled && m_prefetchCount >= 1 && address == m_prefetchHeadAddr) {
            // First halfword in buffer, second needs S-cycle
            m_waitCycles -= m_wsNonseq32[region];
            m_waitCycles += m_wsSeq16[region];  // one S-cycle for second half
            m_prefetchHeadAddr += 4;
            m_prefetchCount = 0;
        } else {
            // Buffer miss — convert N+S to S+S for sequential fetches
            bool isSeq = (address == m_lastFetchAddr + 4);
            if (isSeq) {
                m_waitCycles -= m_wsNonseq32[region];
                m_waitCycles += m_wsSeq32[region];
            }
            // Reset prefetch buffer
            m_prefetchCount = 0;
            m_prefetchHeadAddr = address + 4;
        }
    }

    m_fetchRegion = region;
    m_lastFetchAddr = address;
    m_openBus = value;
    return value;
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
            // Bit 7: 0 = Halt, 1 = Stop (deeper halt). Both halt the CPU.
            m_halted = true;
            break;
    }
    
    m_io[address] = value;

    // Update wait states when either byte of WAITCNT is written
    if (address == IO::WAITCNT || address == IO::WAITCNT + 1) {
        updateWaitstates(*reinterpret_cast<u16*>(&m_io[IO::WAITCNT]));
    }
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
        // Write to IO array FIRST so writeRegister can read the updated value
        *reinterpret_cast<u16*>(&m_io[offset]) = value;
        m_ppu->writeRegister(offset, value);
        return;
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
            // Bit 7: 0 = Halt, 1 = Stop (deeper halt). Both halt the CPU.
            m_halted = true;
            break;
        case IO::WAITCNT:
            *reinterpret_cast<u16*>(&m_io[offset]) = value;
            updateWaitstates(value);
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
    
    // Wake from halt if this IRQ is enabled in IE,
    if (IE & irqBit) {
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
    buffer_write(buf, &m_waitCycles, sizeof(m_waitCycles));
    buffer_write(buf, m_wsNonseq16, sizeof(m_wsNonseq16));
    buffer_write(buf, m_wsNonseq32, sizeof(m_wsNonseq32));
    buffer_write(buf, m_wsSeq16, sizeof(m_wsSeq16));
    buffer_write(buf, m_wsSeq32, sizeof(m_wsSeq32));
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
    buffer_read(buf, &m_waitCycles, sizeof(m_waitCycles));
    buffer_read(buf, m_wsNonseq16, sizeof(m_wsNonseq16));
    buffer_read(buf, m_wsNonseq32, sizeof(m_wsNonseq32));
    buffer_read(buf, m_wsSeq16, sizeof(m_wsSeq16));
    buffer_read(buf, m_wsSeq32, sizeof(m_wsSeq32));
}

} // namespace gba

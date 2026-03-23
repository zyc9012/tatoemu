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
    m_exWaitcnt = 0x0D000020;
    m_prefetchEnabled = false;
    m_prefetchCount = 0;
    m_prefetchHeadAddr = 0;
    m_fetchRegion = 0;
    m_lastFetchAddr = ~0u;
    *reinterpret_cast<u16*>(&m_io[IO::DISPCNT]) = 0x0080;
    *reinterpret_cast<u16*>(&m_io[IO::RCNT]) = -0x8000;
    *reinterpret_cast<u16*>(&m_io[IO::KEYINPUT]) = 0x3FF;
    *reinterpret_cast<u16*>(&m_io[IO::SOUNDBIAS]) = 0x200;
    *reinterpret_cast<u16*>(&m_io[IO::BG2PA]) = 0x100;
    *reinterpret_cast<u16*>(&m_io[IO::BG2PD]) = 0x100;
    *reinterpret_cast<u16*>(&m_io[IO::BG3PA]) = 0x100;
    *reinterpret_cast<u16*>(&m_io[IO::BG3PD]) = 0x100;
    *reinterpret_cast<u16*>(&m_io[IO::POSTFLG]) = 1;
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
    u32 offset = address & 0x1FFFFFF;
    u8 value;
    m_waitCycles += m_wsNonseq16[region];
    prefetchStep(region, m_wsNonseq16[region]);

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
                value = readIO8(offset & 0x3FF);
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
            if (offset < m_cartridge->getROMSize()) {
                value = m_cartridge->getROM()[offset & (m_cartridge->getROMSize() - 1)];
            } else {
                return m_openBus >> ((address & 3) * 8);
            }
            break;
        case REGION_SRAM:
        case REGION_SRAM_MIRROR:
            value = m_cartridge->readSave(offset);
            break;
        default:
            return m_openBus >> ((address & 3) * 8);
    }

    return value;
}

u16 Memory::read16(u32 address, bool isFetch) {
    address &= ~1;
    u32 region = (address >> 24) & 0xF;
    u32 offset = address & 0x1FFFFFF;
    u16 value;
    m_waitCycles += m_wsNonseq16[region];
    if (!isFetch) prefetchStep(region, m_wsNonseq16[region]);

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

u32 Memory::read32(u32 address, bool isFetch) {
    address &= ~3;
    u32 region = (address >> 24) & 0xF;
    u32 offset = address & 0x1FFFFFF;
    u32 value;
    m_waitCycles += m_wsNonseq32[region];
    if (!isFetch) prefetchStep(region, m_wsNonseq32[region]);

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
            if (offset < m_cartridge->getROMSize()) {
                offset &= (m_cartridge->getROMSize() - 1);
                value = *reinterpret_cast<u32*>(&m_cartridge->getROM()[offset]);
            } else {
                return m_openBus;
            }
            break;
        case REGION_SRAM:
        case REGION_SRAM_MIRROR:
            {
                u8 val = m_cartridge->readSave(offset);
                value = val * 0x01010101;
            }
            break;
        default:
            return m_openBus;
    }

    return value;
}

void Memory::write8(u32 address, u8 value) {
    u32 region = (address >> 24) & 0xF;
    u32 offset = address & 0x1FFFFFF;
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
                writeIO8(offset & 0x3FF, value);
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
            m_cartridge->writeSave(offset, value);
            break;
        default:
            break;
    }
}

void Memory::write16(u32 address, u16 value) {
    address &= ~1;
    u32 region = (address >> 24) & 0xF;
    u32 offset = address & 0x1FFFFFF;
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
            m_cartridge->writeSave(offset, value & 0xFF);
            break;
        default:
            break;
    }
}

void Memory::write32(u32 address, u32 value) {
    address &= ~3;
    u32 region = (address >> 24) & 0xF;
    u32 offset = address & 0x1FFFFFF;
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
            m_cartridge->writeSave(offset, value & 0xFF);
            break;
        default:
            break;
    }
}

u16 Memory::fetch16(u32 address) {
    u16 value = read16(address, true);  // Adds m_wsNonseq16[region] to m_waitCycles
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
    u32 value = read32(address, true);  // Adds m_wsNonseq32[region] to m_waitCycles
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

u8 Memory::readIO8(u32 offset) {
    return (readIO16(offset & 0xFFFE) >> ((offset & 0x0001) << 3)) & 0xFF;
}

void Memory::writeIO8(u32 address, u8 value) {
    if (address > IO_SIZE - 1) return;

    if (address == IO::HALTCNT) {
        m_io[IO::HALTCNT] = value;
        m_halted = true;
        return;
    }

    u32 aligned = address & ~1u;
    int shift = (address & 1) * 8;
    u16 existing = *reinterpret_cast<u16*>(&m_io[aligned]);
    u16 value16 = (existing & ~(0xFF << shift)) | (static_cast<u16>(value) << shift);
    writeIO16(aligned, value16);
}

u16 Memory::readIO16(u32 offset) const {
    offset &= 0xFFFFFFFE;

    switch (offset) {
        case IO::VCOUNT:
            return m_ppu->getVCount();
        case IO::SOUND1CNT_L:
        case IO::SOUND1CNT_H:
        case IO::SOUND1CNT_X:
        case IO::SOUND2CNT_L:
        case IO::SOUND2CNT_H:
        case IO::SOUND3CNT_L:
        case IO::SOUND3CNT_H:
        case IO::SOUND3CNT_X:
        case IO::SOUND4CNT_L:
        case IO::SOUND4CNT_H:
        case IO::SOUNDCNT_L:
        case IO::SOUNDCNT_H:
        case IO::SOUNDCNT_X:
        case IO::SOUNDBIAS:
        case IO::WAVE_RAM:
        case IO::WAVE_RAM + 2:
        case IO::WAVE_RAM + 4:
        case IO::WAVE_RAM + 6:
        case IO::WAVE_RAM + 8:
        case IO::WAVE_RAM + 0xA:
        case IO::WAVE_RAM + 0xC:
        case IO::WAVE_RAM + 0xE: {
            u8 lo = m_apu->readRegister(offset);
            u8 hi = m_apu->readRegister(offset + 1);
            return lo | (static_cast<u16>(hi) << 8);
        }
        case IO::TM0CNT_L:
        case IO::TM0CNT_H:
            return m_timer->readCounter(0);
        case IO::TM1CNT_L:
        case IO::TM1CNT_H:
            return m_timer->readCounter(1);
        case IO::TM2CNT_L:
        case IO::TM2CNT_H:
            return m_timer->readCounter(2);
        case IO::TM3CNT_L:
        case IO::TM3CNT_H:
            return m_timer->readCounter(3);
        case IO::KEYINPUT:
            return m_joypad->read();
        default:
            return *reinterpret_cast<const u16*>(&m_io[offset]);
    }
}

void Memory::writeIO16(u32 offset, u16 value) {
    if (offset < IO::SOUND1CNT_L && (offset > IO::VCOUNT || offset < IO::DISPSTAT)) {
        *reinterpret_cast<u16*>(&m_io[offset]) = value;
        m_ppu->writeRegister(offset, value);
        return;
    }

    switch (offset) {
        case IO::DISPSTAT: {
            // Bits 0-2 (VBlank/HBlank/VCounter flags) are hardware-set; CPU writes cannot change them
            u16 hw = *reinterpret_cast<const u16*>(&m_io[IO::DISPSTAT]);
            value = (value & ~0x0007u) | (hw & 0x0007u);
            break;
        }
        case IO::VCOUNT:
            return;
        case IO::SOUND1CNT_L:
        case IO::SOUND1CNT_H:
        case IO::SOUND1CNT_X:
        case IO::SOUND2CNT_L:
        case IO::SOUND2CNT_H:
        case IO::SOUND3CNT_L:
        case IO::SOUND3CNT_H:
        case IO::SOUND3CNT_X:
        case IO::SOUND4CNT_L:
        case IO::SOUND4CNT_H:
        case IO::SOUNDCNT_L:
        case IO::SOUNDCNT_H:
        case IO::SOUNDCNT_X:
        case IO::SOUNDBIAS:
        case IO::WAVE_RAM:
        case IO::WAVE_RAM + 2:
        case IO::WAVE_RAM + 4:
        case IO::WAVE_RAM + 6:
        case IO::WAVE_RAM + 8:
        case IO::WAVE_RAM + 0xA:
        case IO::WAVE_RAM + 0xC:
        case IO::WAVE_RAM + 0xE:
            m_apu->writeRegister(offset, value);
            break;
        case IO::FIFO_A:
        case IO::FIFO_A + 2:
        case IO::FIFO_B:
        case IO::FIFO_B + 2:
            m_apu->writeRegister(offset, value);
            return; // FIFO — don't store to io
        case IO::DMA0SAD:
        case IO::DMA0SAD + 2:
        case IO::DMA0DAD:
        case IO::DMA0DAD + 2:
        case IO::DMA0CNT_L:
        case IO::DMA0CNT_H:
        case IO::DMA1SAD:
        case IO::DMA1SAD + 2:
        case IO::DMA1DAD:
        case IO::DMA1DAD + 2:
        case IO::DMA1CNT_L:
        case IO::DMA1CNT_H:
        case IO::DMA2SAD:
        case IO::DMA2SAD + 2:
        case IO::DMA2DAD:
        case IO::DMA2DAD + 2:
        case IO::DMA2CNT_L:
        case IO::DMA2CNT_H:
        case IO::DMA3SAD:
        case IO::DMA3SAD + 2:
        case IO::DMA3DAD:
        case IO::DMA3DAD + 2:
        case IO::DMA3CNT_L:
        case IO::DMA3CNT_H:
            // Write to IO array FIRST, then let DMA subsystem process
            *reinterpret_cast<u16*>(&m_io[offset]) = value;
            m_dma->writeRegister(offset, value);
            return;
        case IO::TM0CNT_L:
        case IO::TM0CNT_H:
        case IO::TM1CNT_L:
        case IO::TM1CNT_H:
        case IO::TM2CNT_L:
        case IO::TM2CNT_H:
        case IO::TM3CNT_L:
        case IO::TM3CNT_H:
            m_timer->writeRegister(offset, value);
            break;
        case IO::SIOCNT:
            // SIO disabled — just store the value for now
            *reinterpret_cast<u16*>(&m_io[IO::SIOCNT]) = value;
            return;
        case IO::RCNT:
            *reinterpret_cast<u16*>(&m_io[IO::RCNT]) = value & 0xC1FF;
            return;
        case IO::IF:
            // Write-1-to-clear
            *reinterpret_cast<u16*>(&m_io[IO::IF]) &= ~value;
            return;
        case IO::WAITCNT:
            value &= 0x5FFF;
            updateWaitstates(value);
            break;
        case IO::IME:
            *reinterpret_cast<u16*>(&m_io[offset]) = value & 1;
            return;
        case IO::POSTFLG:
            if (m_io[IO::POSTFLG]) {
                m_halted = true;
            }
            break;
        case IO::HALTCNT:
            m_halted = true;
            break;
    }

    *reinterpret_cast<u16*>(&m_io[offset]) = value;
}

u32 Memory::readIO32(u32 offset) const {
    return readIO16(offset) | (static_cast<u32>(readIO16(offset + 2)) << 16);
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

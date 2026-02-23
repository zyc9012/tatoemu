#include "dma.h"
#include "memory.h"
#include "cartridge.h"

namespace gba {

DMA::DMA() {
    reset();
}

DMA::~DMA() {}

void DMA::reset() {
    for (int i = 0; i < 4; i++) {
        m_channels[i] = {};
    }
}

void DMA::writeRegister(u32 offset, u16 value) {
    int channel = -1;
    int reg = 0;
    
    if (offset >= IO::DMA0SAD && offset <= IO::DMA3CNT_H + 1) {
        channel = (offset - IO::DMA0SAD) / 12;
        reg = (offset - IO::DMA0SAD) % 12;
    }
    
    if (channel < 0 || channel > 3) return;
    
    switch (reg) {
        case 0: // SAD low
            m_channels[channel].source = (m_channels[channel].source & 0xFFFF0000) | value;
            break;
        case 2: // SAD high
            m_channels[channel].source = (m_channels[channel].source & 0x0000FFFF) | (static_cast<u32>(value) << 16);
            // DMA0: 27-bit source, DMA1-3: 28-bit source
            m_channels[channel].source &= (channel == 0) ? 0x07FFFFFE : 0x0FFFFFFE;
            break;
        case 4: // DAD low
            m_channels[channel].dest = (m_channels[channel].dest & 0xFFFF0000) | value;
            break;
        case 6: // DAD high
            m_channels[channel].dest = (m_channels[channel].dest & 0x0000FFFF) | (static_cast<u32>(value) << 16);
            // DMA0-2: 27-bit dest, DMA3: 28-bit dest
            m_channels[channel].dest &= (channel == 3) ? 0x0FFFFFFE : 0x07FFFFFE;
            break;
        case 8: // CNT_L (count)
            m_channels[channel].count = value;
            break;
        case 10: { // CNT_H (control)
            bool wasEnabled = (m_channels[channel].control & (1 << 15)) != 0;
            m_channels[channel].control = value;
            bool nowEnabled = (value & (1 << 15)) != 0;
            
            if (!wasEnabled && nowEnabled) {
                trigger(channel);
            } else if (!nowEnabled) {
                m_channels[channel].active = false;
            }
            break;
        }
    }
}

void DMA::trigger(int channel) {
    bool is32bit = (m_channels[channel].control & (1 << 10)) != 0;
    int width = is32bit ? 4 : 2;

    // Align source and dest to transfer width
    m_channels[channel].internalSource = m_channels[channel].source & ~(width - 1);
    m_channels[channel].internalDest = m_channels[channel].dest & ~(width - 1);
    m_channels[channel].internalCount = m_channels[channel].count;
    
    if (m_channels[channel].internalCount == 0) {
        m_channels[channel].internalCount = (channel == 3) ? 0x10000 : 0x4000;
    }
    
    m_channels[channel].active = true;
    m_channels[channel].repeat = (m_channels[channel].control & (1 << 9)) != 0;
    
    int timing = (m_channels[channel].control >> 12) & 3;

    if (timing == DMA_TIMING::IMMEDIATE) {
        run(channel);
    }
}

void DMA::run(int channel) {
    if (!m_channels[channel].active) return;
    
    int srcCtrl = (m_channels[channel].control >> 7) & 3;
    int dstCtrl = (m_channels[channel].control >> 5) & 3;
    bool is32bit = (m_channels[channel].control & (1 << 10)) != 0;
    int width = is32bit ? 4 : 2;
    
    u32 src = m_channels[channel].internalSource;
    u32 dst = m_channels[channel].internalDest;
    u32 count = m_channels[channel].internalCount;
    
    // Source offset: ROM sources are forced to increment
    static constexpr int DMA_OFFSET[] = {1, -1, 0, 1}; // INC, DEC, FIXED, INC(reload only for dest)
    int srcOffset;
    if (src >= 0x08000000 && src < 0x0E000000) {
        srcOffset = width; // ROM source: forced increment
    } else {
        srcOffset = DMA_OFFSET[srcCtrl] * width;
    }
    int dstOffset = DMA_OFFSET[dstCtrl] * width;
    
    // Check for EEPROM access (ROM2_EX region = 0x0D000000)
    u32 srcRegion = (src >> 24) & 0xF;
    u32 dstRegion = (dst >> 24) & 0xF;
    Cartridge* cart = m_memory ? m_memory->getCartridge() : nullptr;
    
    for (u32 i = 0; i < count; i++) {
        if (is32bit) {
            // Only refresh latch for non-BIOS sources; BIOS reads return stale latch
            if (srcRegion >= REGION_EWRAM) {
                m_channels[channel].latch = m_memory->read32(src);
            }
            m_memory->write32(dst, m_channels[channel].latch);
        } else {
            u16 remainingCount = count - i;
            u16 data;

            // Only refresh latch for non-BIOS sources; BIOS reads return stale latch
            if (srcRegion == REGION_ROM2H && cart && cart->hasEEPROM()) {
                data = cart->readEEPROM();
                m_channels[channel].latch = data | (static_cast<u32>(data) << 16);
            } else if (srcRegion >= REGION_EWRAM) {
                data = m_memory->read16(src);
                m_channels[channel].latch = data | (static_cast<u32>(data) << 16);
            } else {
                // srcRegion < REGION_EWRAM (BIOS) — latch is not refreshed, stale value is written
                data = static_cast<u16>(m_channels[channel].latch >> (8 * (dst & 2)));
            }

            // EEPROM write through DMA to ROM2_EX region (0x0D000000)
            if (dstRegion == REGION_ROM2H && cart) {
                cart->writeEEPROM(data, remainingCount);
            } else {
                m_memory->write16(dst, data);
            }
        }

        src += srcOffset;
        dst += dstOffset;
    }
    
    // Update internal registers
    m_channels[channel].internalSource = src;
    if (dstCtrl != 3) {
        m_channels[channel].internalDest = dst;
    } else {
        m_channels[channel].internalDest = m_channels[channel].dest; // Reload
    }
    
    // Handle completion
    bool irq = (m_channels[channel].control & (1 << 14)) != 0;
    if (irq && m_memory) {
        m_memory->requestIRQ(IRQ::DMA0 << channel);
    }
    
    int timing = (m_channels[channel].control >> 12) & 3;
    // Immediate DMAs never repeat, even if repeat bit is set
    // DMA3 video capture auto-stops at vcount == 161
    bool noRepeat = !m_channels[channel].repeat
                 || (timing == DMA_TIMING::IMMEDIATE)
                 || (channel == 3 && timing == DMA_TIMING::SPECIAL && m_videoCaptureLine >= VISIBLE_LINES + 1);
    
    if (noRepeat) {
        m_channels[channel].active = false;
        m_channels[channel].control &= ~(1 << 15); // Disable
        // Update the IO register to reflect DMA completion
        // Games poll DMA CNT_H bit 15 to wait for transfer to finish
        if (m_memory) {
            u32 cntHOffset = IO::DMA0CNT_H + channel * 12;
            u16* reg = reinterpret_cast<u16*>(&m_memory->getIO()[cntHOffset]);
            *reg &= ~(1 << 15);
        }
    }
}

void DMA::runHBlank() {
    for (int i = 0; i < 4; i++) {
        if (m_channels[i].active) {
            int timing = (m_channels[i].control >> 12) & 3;
            if (timing == DMA_TIMING::HBLANK) {
                // Reload count for repeat DMAs
                m_channels[i].internalCount = m_channels[i].count;
                if (m_channels[i].internalCount == 0) {
                    m_channels[i].internalCount = (i == 3) ? 0x10000 : 0x4000;
                }
                run(i);
            }
        }
    }
}

void DMA::runDisplayStart(u16 vcount) {
    // DMA3 video capture: SPECIAL timing, fires during HBlank at vcounts 2-161
    // Matches mgba's GBADMARunDisplayStart (triggered at vcount >= 2 && < VISIBLE_LINES+2)
    if (m_channels[3].active) {
        int timing = (m_channels[3].control >> 12) & 3;
        if (timing == DMA_TIMING::SPECIAL) {
            m_videoCaptureLine = vcount;
            m_channels[3].internalCount = m_channels[3].count;
            if (m_channels[3].internalCount == 0) {
                m_channels[3].internalCount = 0x10000;
            }
            run(3);
        }
    }
}

void DMA::runVBlank() {
    for (int i = 0; i < 4; i++) {
        if (m_channels[i].active) {
            int timing = (m_channels[i].control >> 12) & 3;
            if (timing == DMA_TIMING::VBLANK) {
                // Reload count for repeat DMAs
                m_channels[i].internalCount = m_channels[i].count;
                if (m_channels[i].internalCount == 0) {
                    m_channels[i].internalCount = (i == 3) ? 0x10000 : 0x4000;
                }
                run(i);
            }
        }
    }
}

void DMA::runFIFO(int fifoIndex) {
    // FIFO A uses DMA 1 or 2, FIFO B uses DMA 1 or 2
    // SPECIAL timing DMA channels 1 and 2 can feed FIFOs
    // fifoIndex 0 = FIFO A (dest 0x040000A0), 1 = FIFO B (dest 0x040000A4)
    constexpr u32 FIFO_ADDR[2] = { 0x040000A0, 0x040000A4 };
    u32 targetAddr = FIFO_ADDR[fifoIndex];

    for (int i = 1; i <= 2; i++) {
        if (!m_channels[i].active) continue;
        int timing = (m_channels[i].control >> 12) & 3;
        if (timing != DMA_TIMING::SPECIAL) continue;
        if (m_channels[i].dest != targetAddr) continue;

        // FIFO DMA: always transfers 4 words (16 bytes), 32-bit, fixed dest
        u32 src = m_channels[i].internalSource;
        for (int j = 0; j < 4; j++) {
            u32 data = m_memory->read32(src);
            m_memory->write32(targetAddr, data);
            src += 4;
        }
        m_channels[i].internalSource = src;

        // Handle completion
        bool irq = (m_channels[i].control & (1 << 14)) != 0;
        if (irq && m_memory) {
            m_memory->requestIRQ(IRQ::DMA0 << i);
        }
        // FIFO DMA always repeats until disabled
        break;
    }
}

bool DMA::isActive() const {
    for (int i = 0; i < 4; i++) {
        if (m_channels[i].active) return true;
    }
    return false;
}

void DMA::saveState(Buffer* buf) {
    for (int i = 0; i < 4; i++) {
        buffer_write(buf, &m_channels[i], sizeof(DMAChannel));
    }
}

void DMA::loadState(Buffer* buf) {
    for (int i = 0; i < 4; i++) {
        buffer_read(buf, &m_channels[i], sizeof(DMAChannel));
    }
}

} // namespace gba

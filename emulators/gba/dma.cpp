#include "dma.h"
#include "memory.h"

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
            m_channels[channel].source &= 0x0FFFFFFF; // 28-bit
            break;
        case 4: // DAD low
            m_channels[channel].dest = (m_channels[channel].dest & 0xFFFF0000) | value;
            break;
        case 6: // DAD high
            m_channels[channel].dest = (m_channels[channel].dest & 0x0000FFFF) | (static_cast<u32>(value) << 16);
            m_channels[channel].dest &= 0x0FFFFFFF; // 28-bit
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
    m_channels[channel].internalSource = m_channels[channel].source;
    m_channels[channel].internalDest = m_channels[channel].dest;
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
    
    u32 src = m_channels[channel].internalSource;
    u32 dst = m_channels[channel].internalDest;
    u16 count = m_channels[channel].internalCount;
    
    for (u16 i = 0; i < count; i++) {
        if (is32bit) {
            u32 data = m_memory->read32(src);
            m_memory->write32(dst, data);
            
            // Update source
            if (srcCtrl == 0) src += 4;       // Increment
            else if (srcCtrl == 1) src -= 4;  // Decrement
            // srcCtrl == 2: Fixed
            
            // Update dest
            if (dstCtrl == 0) dst += 4;
            else if (dstCtrl == 1) dst -= 4;
            else if (dstCtrl == 3) dst += 4; // Increment+reload
        } else {
            u16 data = m_memory->read16(src);
            m_memory->write16(dst, data);
            
            if (srcCtrl == 0) src += 2;
            else if (srcCtrl == 1) src -= 2;
            
            if (dstCtrl == 0) dst += 2;
            else if (dstCtrl == 1) dst -= 2;
            else if (dstCtrl == 3) dst += 2;
        }
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
    
    if (!m_channels[channel].repeat) {
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

void DMA::runImmediate() {
    for (int i = 0; i < 4; i++) {
        if (m_channels[i].active) {
            int timing = (m_channels[i].control >> 12) & 3;
            if (timing == DMA_TIMING::IMMEDIATE) {
                run(i);
            }
        }
    }
}

void DMA::runHBlank() {
    for (int i = 0; i < 4; i++) {
        if (m_channels[i].active) {
            int timing = (m_channels[i].control >> 12) & 3;
            if (timing == DMA_TIMING::HBLANK) {
                run(i);
            }
        }
    }
}

void DMA::runVBlank() {
    for (int i = 0; i < 4; i++) {
        if (m_channels[i].active) {
            int timing = (m_channels[i].control >> 12) & 3;
            if (timing == DMA_TIMING::VBLANK) {
                run(i);
            }
        }
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

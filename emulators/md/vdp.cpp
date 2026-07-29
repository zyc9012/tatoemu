#include "vdp.h"
#include "cpu.h"
#include "memory.h"
#include <algorithm>
#include <cstring>

namespace md {

namespace {

// 3-bit colour component to 8-bit, at the three intensity levels the VDP can
// output when shadow/highlight is enabled.
constexpr u8 SHADE_NORMAL[8]    = {   0,  36,  73, 109, 146, 182, 219, 255 };
constexpr u8 SHADE_SHADOW[8]    = {   0,  18,  36,  54,  73,  91, 109, 127 };
constexpr u8 SHADE_HIGHLIGHT[8] = { 146, 164, 182, 200, 219, 237, 255, 255 };

// Plane dimensions selected by register 16.  Entry 2 is undefined on hardware
// and behaves as 32 cells.
constexpr u32 PLANE_CELLS[4] = { 32, 64, 32, 128 };

constexpr u8 PIXEL_PRIORITY = 0x40;
constexpr u8 PIXEL_COLOR    = 0x3F;

// Sprite operator colours in palette 3.
constexpr u8 OP_HIGHLIGHT = 0x3E;
constexpr u8 OP_SHADOW    = 0x3F;

} // namespace

VDP::VDP() {
    reset();
}

void VDP::reset() {
    m_vram.fill(0);
    m_cram.fill(0);
    m_vsram.fill(0);
    m_regs.fill(0);

    // Power-on defaults that let a ROM run before it programs the VDP.
    m_regs[1]  = 0x04;  // Mode 5
    m_regs[10] = 0xFF;
    m_regs[15] = 0x02;

    m_addr = 0;
    m_code = 0;
    m_pending = false;
    m_readBuffer = 0;
    m_dmaFillPending = false;

    // Bit 9 is the FIFO-empty flag: writes complete instantly here, so the
    // FIFO is always reported empty and never full.  Bits 10-13 read high.
    m_status = 0x3600;
    m_line = 0;
    m_hintCounter = 0;
    m_vintPending = false;
    m_hintPending = false;
    m_oddFrame = false;

    m_framebuffer.fill(0xFF000000);

    for (u32 i = 0; i < CRAM_ENTRIES; i++) updatePaletteEntry(i);
}

void VDP::setPAL(bool pal) {
    m_pal = pal;
    if (pal) m_status |= 0x0001;
    else     m_status &= ~0x0001;
}

// ---------------------------------------------------------------------------
// Palette
// ---------------------------------------------------------------------------

void VDP::updatePaletteEntry(u32 index) {
    u16 value = m_cram[index];
    u32 b = (value >> 9) & 7;
    u32 g = (value >> 5) & 7;
    u32 r = (value >> 1) & 7;

    m_paletteNormal[index] = 0xFF000000u |
        (static_cast<u32>(SHADE_NORMAL[r]) << 16) |
        (static_cast<u32>(SHADE_NORMAL[g]) << 8)  |
         static_cast<u32>(SHADE_NORMAL[b]);

    m_paletteShadow[index] = 0xFF000000u |
        (static_cast<u32>(SHADE_SHADOW[r]) << 16) |
        (static_cast<u32>(SHADE_SHADOW[g]) << 8)  |
         static_cast<u32>(SHADE_SHADOW[b]);

    m_paletteHighlight[index] = 0xFF000000u |
        (static_cast<u32>(SHADE_HIGHLIGHT[r]) << 16) |
        (static_cast<u32>(SHADE_HIGHLIGHT[g]) << 8)  |
         static_cast<u32>(SHADE_HIGHLIGHT[b]);
}

// ---------------------------------------------------------------------------
// Control / data ports
// ---------------------------------------------------------------------------

void VDP::bumpAddress() {
    m_addr = (m_addr + m_regs[15]) & 0xFFFF;
}

u32 VDP::dmaLength() const {
    u32 length = (static_cast<u32>(m_regs[20]) << 8) | m_regs[19];
    return length ? length : 0x10000;
}

void VDP::writeRegister(u8 index, u8 value) {
    if (index >= 24) return;

    u8 previous = m_regs[index];
    m_regs[index] = value;

    if (index == 0 && ((previous ^ value) & 0x10)) {
        // Masking the horizontal interrupt releases the line immediately;
        // unmasking it while a request is latched asserts it.
        updateIRQ();
    }

    // Enabling the vertical interrupt while one is already pending fires it
    // immediately, which some games rely on.
    if (index == 1 && ((previous ^ value) & 0x20)) {
        updateIRQ();
    }
}

// The VDP holds both interrupt lines itself: it asserts the highest enabled
// pending level and releases the line once nothing is left to service.
void VDP::updateIRQ() {
    if (!m_cpu) return;

    if ((m_regs[1] & 0x20) && m_vintPending)      m_cpu->setIRQLevel(IRQ_VBLANK);
    else if ((m_regs[0] & 0x10) && m_hintPending) m_cpu->setIRQLevel(IRQ_HBLANK);
    else                                          m_cpu->setIRQLevel(0);
}

void VDP::writeControl(u16 value) {
    if (!m_pending) {
        if ((value & 0xC000) == 0x8000) {
            writeRegister((value >> 8) & 0x1F, static_cast<u8>(value & 0xFF));
            // A register write also clears the pending CD5 DMA trigger.
            m_code &= 0x03;
            return;
        }

        m_addr = (m_addr & 0xC000) | (value & 0x3FFF);
        m_code = static_cast<u8>((m_code & 0x3C) | ((value >> 14) & 0x03));
        m_pending = true;
        return;
    }

    m_addr = (m_addr & 0x3FFF) | ((static_cast<u32>(value) & 0x03) << 14);
    m_code = static_cast<u8>((m_code & 0x03) | ((value >> 2) & 0x3C));
    m_pending = false;

    // CD5 set and DMA enabled in register 1 starts a transfer.
    if ((m_code & 0x20) && (m_regs[1] & 0x10)) {
        startDMA();
    }
}

u16 VDP::readControl() {
    m_pending = false;

    u16 status = m_status;

    // While the display is disabled the VDP reports a permanent vertical
    // blank, because it never leaves blanking.
    if (!(m_regs[1] & 0x40)) status |= 0x0008;

    // Only the sticky sprite flags are cleared by a status read.  The vertical
    // interrupt is *not* acknowledged here: the flag and the interrupt line
    // survive until the 68000 runs its acknowledge cycle.
    m_status &= ~0x0060;

    return status;
}

void VDP::acknowledgeIRQ(u8 level) {
    // The vertical interrupt has priority when both are asserted; the other
    // request stays latched and is re-asserted by updateIRQ().
    if (level == IRQ_VBLANK && (m_regs[1] & 0x20) && m_vintPending) {
        m_vintPending = false;
        m_status &= ~0x0080;
    } else {
        m_hintPending = false;
    }

    updateIRQ();
}

void VDP::writeData(u16 value) {
    m_pending = false;

    if (m_dmaFillPending) {
        m_dmaFillPending = false;
        writeDataInternal(value);
        dmaVramFill(value);
        return;
    }

    writeDataInternal(value);
}

void VDP::writeDataInternal(u16 value) {
    switch (m_code & 0x0F) {
        case 0x01: {  // VRAM
            // An odd address swaps the two bytes on the way in.
            u16 data = (m_addr & 1) ? static_cast<u16>((value >> 8) | (value << 8)) : value;
            u32 a = m_addr & 0xFFFE;
            m_vram[a]     = static_cast<u8>(data >> 8);
            m_vram[a + 1] = static_cast<u8>(data & 0xFF);
            break;
        }
        case 0x03: {  // CRAM
            u32 index = (m_addr >> 1) & 0x3F;
            m_cram[index] = value & 0x0EEE;
            updatePaletteEntry(index);
            break;
        }
        case 0x05: {  // VSRAM
            u32 index = (m_addr >> 1) & 0x3F;
            if (index < VSRAM_ENTRIES) m_vsram[index] = value & 0x07FF;
            break;
        }
        default:
            break;
    }

    bumpAddress();
}

u16 VDP::readData() {
    m_pending = false;

    u16 result = m_readBuffer;

    switch (m_code & 0x0F) {
        case 0x00: {  // VRAM
            u32 a = m_addr & 0xFFFE;
            result = static_cast<u16>((m_vram[a] << 8) | m_vram[a + 1]);
            break;
        }
        case 0x08: {  // CRAM
            result = m_cram[(m_addr >> 1) & 0x3F];
            break;
        }
        case 0x04: {  // VSRAM
            u32 index = (m_addr >> 1) & 0x3F;
            result = (index < VSRAM_ENTRIES) ? m_vsram[index] : 0;
            break;
        }
        default:
            break;
    }

    m_readBuffer = result;
    bumpAddress();
    return result;
}

u16 VDP::readHVCounter() const {
    // Vertical counter, including the mid-frame jump.
    u32 line = m_line;
    u32 v;
    if (m_pal) {
        v = (line <= 0x102) ? line : (line + 0x1CA - 0x103);
    } else {
        v = (line <= 0x0EA) ? line : (line + 0x1E5 - 0x0EB);
    }

    // Horizontal counter.  It ticks once per two pixels and is derived from
    // how far the 68000 has advanced into the current scanline, so busy-wait
    // loops on a given value actually terminate.
    u32 elapsed = 0;
    if (m_cpu) {
        const u32 now = m_cpu->frameCycles();
        if (now > m_lineStartCycle) elapsed = now - m_lineStartCycle;
        if (elapsed > m_lineCycles) elapsed = m_lineCycles;
    }

    // Counting is not contiguous: the hardware skips a block of values partway
    // through horizontal blanking.
    const u32 total = isH40() ? 211u : 171u;
    const u32 lastBeforeJump = isH40() ? 0xB6u : 0x93u;
    const u32 firstAfterJump = isH40() ? 0xE4u : 0xE9u;

    u32 index = (elapsed * total) / m_lineCycles;
    if (index >= total) index = total - 1;

    const u32 h = (index <= lastBeforeJump) ? index
                                            : (firstAfterJump + (index - lastBeforeJump - 1));

    return static_cast<u16>(((v & 0xFF) << 8) | (h & 0xFF));
}

// ---------------------------------------------------------------------------
// DMA
// ---------------------------------------------------------------------------

void VDP::startDMA() {
    u32 mode = (m_regs[23] >> 6) & 0x03;

    m_status |= 0x0002;  // DMA busy

    switch (mode) {
        case 0:
        case 1:
            dmaMemoryToVdp();
            break;
        case 2:
            // VRAM fill waits for the next data port write to supply the value.
            m_dmaFillPending = true;
            return;
        case 3:
            dmaVramCopy();
            break;
    }

    m_status &= ~0x0002;
}

void VDP::dmaMemoryToVdp() {
    u32 length = dmaLength();
    u32 source = ((static_cast<u32>(m_regs[23]) & 0x7F) << 17) |
                 (static_cast<u32>(m_regs[22]) << 9) |
                 (static_cast<u32>(m_regs[21]) << 1);

    for (u32 i = 0; i < length; i++) {
        u16 data = m_memory ? m_memory->readDMA16(source) : 0;
        writeDataInternal(data);
        // The source address wraps inside its 128 KB bank.
        source = (source & 0xFE0000) | ((source + 2) & 0x01FFFF);
    }

    m_regs[19] = 0;
    m_regs[20] = 0;
    m_regs[21] = static_cast<u8>((source >> 1) & 0xFF);
    m_regs[22] = static_cast<u8>((source >> 9) & 0xFF);

    // Approximate the bus time the transfer steals from the 68000.
    if (m_cpu) m_cpu->stall((length * 12) / 5);
}

void VDP::dmaVramFill(u16 value) {
    u32 length = dmaLength();
    u8 fill = static_cast<u8>(value >> 8);

    for (u32 i = 0; i < length; i++) {
        m_vram[(m_addr ^ 1) & 0xFFFF] = fill;
        bumpAddress();
    }

    m_regs[19] = 0;
    m_regs[20] = 0;
    m_status &= ~0x0002;

    if (m_cpu) m_cpu->stall(length);
}

void VDP::dmaVramCopy() {
    u32 length = dmaLength();
    u32 source = (static_cast<u32>(m_regs[22]) << 8) | m_regs[21];

    for (u32 i = 0; i < length; i++) {
        m_vram[m_addr & 0xFFFF] = m_vram[source & 0xFFFF];
        source = (source + 1) & 0xFFFF;
        bumpAddress();
    }

    m_regs[19] = 0;
    m_regs[20] = 0;
    m_regs[21] = static_cast<u8>(source & 0xFF);
    m_regs[22] = static_cast<u8>((source >> 8) & 0xFF);

    if (m_cpu) m_cpu->stall(length * 2);
}

// ---------------------------------------------------------------------------
// Frame sequencing
// ---------------------------------------------------------------------------

void VDP::beginFrame() {
    m_hintCounter = m_regs[10];
    m_oddFrame = !m_oddFrame;
    if (m_oddFrame) m_status |= 0x0010;
    else            m_status &= ~0x0010;
    m_status &= ~0x0008;
}

void VDP::beginLine(u32 line) {
    m_line = line;
    m_status &= ~0x0004;

    // The horizontal interrupt request is *not* released here: the VDP holds
    // the level on the interrupt lines until the 68000 acknowledges it.  Games
    // that spend more than a scanline inside an interrupt handler rely on a
    // request raised meanwhile still being serviced afterwards.
}

void VDP::endActiveDisplay(u32 line) {
    const u32 active = activeScanlines();

    if (line < active && line < SCREEN_HEIGHT) {
        renderLine(line);
    }

    m_status |= 0x0004;  // horizontal blanking

    // The H interrupt counter runs across the active display plus the first
    // blanking line, and is reloaded continuously outside that window.
    if (line <= active) {
        if (--m_hintCounter < 0) {
            m_hintCounter = m_regs[10];
            m_hintPending = true;
            updateIRQ();
        }
    } else {
        m_hintCounter = m_regs[10];
    }

    if (line == active) {
        m_status |= 0x0008;  // vertical blanking
        m_status |= 0x0080;  // vertical interrupt pending
        m_vintPending = true;
        m_vintEvent = true;
        updateIRQ();
    }
}

void VDP::endFrame() {
    if (m_videoDevice) {
        m_videoDevice->render(m_framebuffer.data());
    }
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------

void VDP::renderLine(u32 line) {
    const u32 width = isH40() ? H40_WIDTH : H32_WIDTH;

    if (!(m_regs[1] & 0x40)) {
        // Display disabled: the whole line shows the backdrop colour.
        std::fill(m_planeA.begin(), m_planeA.begin() + width, 0);
        std::fill(m_planeB.begin(), m_planeB.begin() + width, 0);
        std::fill(m_sprites.begin(), m_sprites.begin() + width, 0);
        composite(line);
        return;
    }

    // Work out which part of the line the window plane covers.
    u32 winStart = 0;
    u32 winEnd = 0;
    const u32 windowRow = static_cast<u32>(m_regs[18] & 0x1F) * 8;
    const bool rowInWindow = (m_regs[18] & 0x80) ? (line >= windowRow) : (line < windowRow);

    if (rowInWindow) {
        winStart = 0;
        winEnd = width;
    } else {
        u32 windowCol = static_cast<u32>(m_regs[17] & 0x1F) * 16;
        if (windowCol > width) windowCol = width;
        if (m_regs[17] & 0x80) {
            winStart = windowCol;
            winEnd = width;
        } else {
            winStart = 0;
            winEnd = windowCol;
        }
    }

    // Plane A occupies whatever the window does not.
    if (winStart > 0)     renderPlane(false, line, m_planeA.data(), 0, winStart);
    if (winEnd < width)   renderPlane(false, line, m_planeA.data(), winEnd, width);
    if (winStart < winEnd) renderWindow(line, m_planeA.data(), winStart, winEnd);

    renderPlane(true, line, m_planeB.data(), 0, width);
    renderSprites(line);
    composite(line);
}

void VDP::renderPlane(bool planeB, u32 line, u8* dest, u32 xStart, u32 xEnd) {
    if (xStart >= xEnd) return;

    const u32 nameBase = planeB ? ((static_cast<u32>(m_regs[4]) & 0x07) << 13)
                                : ((static_cast<u32>(m_regs[2]) & 0x38) << 10);

    const u32 hCells = PLANE_CELLS[m_regs[16] & 0x03];
    const u32 vCells = PLANE_CELLS[(m_regs[16] >> 4) & 0x03];
    const u32 hMask = hCells * 8 - 1;
    const u32 vMask = vCells * 8 - 1;

    // Horizontal scroll table lookup depends on the scroll mode in register 11.
    const u32 hsBase = (static_cast<u32>(m_regs[13]) & 0x3F) << 10;
    u32 hsOffset;
    switch (m_regs[11] & 0x03) {
        case 0:  hsOffset = 0; break;
        case 1:  hsOffset = (line & 7) * 4; break;
        case 2:  hsOffset = (line & ~7u) * 4; break;
        default: hsOffset = line * 4; break;
    }
    const u32 hScroll = readVRAM16(hsBase + hsOffset + (planeB ? 2 : 0)) & 0x03FF;

    const bool twoCellVScroll = (m_regs[11] & 0x04) != 0;
    const u32 vsBase = planeB ? 1u : 0u;
    const u32 fullVScroll = m_vsram[vsBase];

    u32 cachedTile = 0xFFFFFFFF;
    u16 entry = 0;
    u32 tileIndex = 0;
    bool hflip = false;
    bool vflip = false;
    u8 attrBits = 0;

    for (u32 x = xStart; x < xEnd; x++) {
        u32 vScroll;
        if (twoCellVScroll) {
            u32 index = ((x >> 4) << 1) + vsBase;
            vScroll = (index < VSRAM_ENTRIES) ? m_vsram[index] : 0;
        } else {
            vScroll = fullVScroll;
        }

        const u32 yy = (line + vScroll) & vMask;
        const u32 xx = (x - hScroll) & hMask;
        const u32 tileX = xx >> 3;
        const u32 tileY = yy >> 3;
        const u32 tileKey = (tileY << 8) | tileX;

        if (tileKey != cachedTile) {
            cachedTile = tileKey;
            entry = readVRAM16(nameBase + ((tileY * hCells + tileX) << 1));
            tileIndex = entry & 0x07FF;
            hflip = (entry & 0x0800) != 0;
            vflip = (entry & 0x1000) != 0;
            attrBits = static_cast<u8>((((entry >> 13) & 3) << 4) |
                                       ((entry & 0x8000) ? PIXEL_PRIORITY : 0));
        }

        const u32 fy = vflip ? (7 - (yy & 7)) : (yy & 7);
        const u32 fx = hflip ? (7 - (xx & 7)) : (xx & 7);
        const u32 addr = ((tileIndex << 5) + (fy << 2) + (fx >> 1)) & 0xFFFF;
        const u8 byte = m_vram[addr];
        const u8 idx = (fx & 1) ? (byte & 0x0F) : static_cast<u8>(byte >> 4);

        dest[x] = idx ? static_cast<u8>(attrBits | idx) : 0;
    }
}

void VDP::renderWindow(u32 line, u8* dest, u32 xStart, u32 xEnd) {
    if (xStart >= xEnd) return;

    const bool h40 = isH40();
    // In H40 mode the low bit of the window base address is ignored.
    const u32 nameBase = (static_cast<u32>(m_regs[3]) & (h40 ? 0x3Cu : 0x3Eu)) << 10;
    const u32 windowCells = h40 ? 64u : 32u;

    const u32 tileY = line >> 3;
    const u32 fyBase = line & 7;

    u32 cachedTileX = 0xFFFFFFFF;
    u32 tileIndex = 0;
    bool hflip = false;
    bool vflip = false;
    u8 attrBits = 0;

    for (u32 x = xStart; x < xEnd; x++) {
        const u32 tileX = x >> 3;
        if (tileX != cachedTileX) {
            cachedTileX = tileX;
            u16 entry = readVRAM16(nameBase + ((tileY * windowCells + tileX) << 1));
            tileIndex = entry & 0x07FF;
            hflip = (entry & 0x0800) != 0;
            vflip = (entry & 0x1000) != 0;
            attrBits = static_cast<u8>((((entry >> 13) & 3) << 4) |
                                       ((entry & 0x8000) ? PIXEL_PRIORITY : 0));
        }

        const u32 fy = vflip ? (7 - fyBase) : fyBase;
        const u32 fx = hflip ? (7 - (x & 7)) : (x & 7);
        const u32 addr = ((tileIndex << 5) + (fy << 2) + (fx >> 1)) & 0xFFFF;
        const u8 byte = m_vram[addr];
        const u8 idx = (fx & 1) ? (byte & 0x0F) : static_cast<u8>(byte >> 4);

        dest[x] = idx ? static_cast<u8>(attrBits | idx) : 0;
    }
}

void VDP::renderSprites(u32 line) {
    const bool h40 = isH40();
    const u32 width = h40 ? H40_WIDTH : H32_WIDTH;

    std::fill(m_sprites.begin(), m_sprites.begin() + width, 0);

    const u32 maxSprites = h40 ? 80u : 64u;
    const u32 maxPerLine = h40 ? 20u : 16u;
    const u32 maxPixels  = width;

    const u32 satBase = (static_cast<u32>(m_regs[5]) & (h40 ? 0x7Eu : 0x7Fu)) << 9;

    u32 index = 0;
    u32 onLine = 0;
    u32 pixels = 0;
    bool anyDrawn = false;

    for (u32 i = 0; i < maxSprites; i++) {
        const u32 addr = (satBase + index * 8) & 0xFFFF;

        const s32 spriteY = static_cast<s32>(readVRAM16(addr) & 0x01FF) - 128;
        const u8 sizeByte = m_vram[(addr + 2) & 0xFFFF];
        const u32 vCells = static_cast<u32>(sizeByte & 0x03) + 1;
        const u32 hCells = static_cast<u32>((sizeByte >> 2) & 0x03) + 1;
        const u32 link = m_vram[(addr + 3) & 0xFFFF] & 0x7F;
        const u16 attr = readVRAM16(addr + 4);
        const s32 spriteX = static_cast<s32>(readVRAM16(addr + 6) & 0x01FF) - 128;

        const s32 height = static_cast<s32>(vCells * 8);
        const s32 currentLine = static_cast<s32>(line);

        if (currentLine >= spriteY && currentLine < spriteY + height) {
            if (++onLine > maxPerLine) {
                m_status |= 0x0040;  // sprite overflow
                break;
            }

            // A sprite positioned at raw X = 0 masks every lower-priority
            // sprite on the line, provided something was already drawn.
            if (spriteX == -128) {
                if (anyDrawn) break;
            } else {
                const u32 tileBase = attr & 0x07FF;
                const bool hflip = (attr & 0x0800) != 0;
                const bool vflip = (attr & 0x1000) != 0;
                const u8 attrBits = static_cast<u8>((((attr >> 13) & 3) << 4) |
                                                    ((attr & 0x8000) ? PIXEL_PRIORITY : 0));

                u32 row = static_cast<u32>(currentLine - spriteY);
                if (vflip) row = vCells * 8 - 1 - row;

                for (u32 cell = 0; cell < hCells; cell++) {
                    const u32 cellX = hflip ? (hCells - 1 - cell) : cell;
                    // Sprite tiles are laid out column by column.
                    const u32 tile = tileBase + cellX * vCells + (row >> 3);
                    const u32 rowAddr = ((tile << 5) + ((row & 7) << 2)) & 0xFFFF;

                    for (u32 px = 0; px < 8; px++) {
                        const s32 sx = spriteX + static_cast<s32>(cell * 8 + px);
                        if (sx < 0 || sx >= static_cast<s32>(width)) continue;

                        const u32 fx = hflip ? (7 - px) : px;
                        const u8 byte = m_vram[(rowAddr + (fx >> 1)) & 0xFFFF];
                        const u8 idx = (fx & 1) ? (byte & 0x0F) : static_cast<u8>(byte >> 4);
                        if (!idx) continue;

                        if (m_sprites[static_cast<u32>(sx)]) {
                            m_status |= 0x0020;  // sprite collision
                            continue;
                        }
                        m_sprites[static_cast<u32>(sx)] = static_cast<u8>(attrBits | idx);
                    }
                }

                anyDrawn = true;
            }

            pixels += hCells * 8;
            if (pixels >= maxPixels) {
                m_status |= 0x0040;
                break;
            }
        }

        index = link;
        if (index == 0 || index >= maxSprites) break;
    }
}

void VDP::composite(u32 line) {
    const u32 width = isH40() ? H40_WIDTH : H32_WIDTH;
    const bool ste = (m_regs[12] & 0x08) != 0;
    const bool blankLeft = (m_regs[0] & 0x20) != 0;
    const u8 backdrop = static_cast<u8>(m_regs[7] & 0x3F);

    u32* out = &m_framebuffer[line * SCREEN_WIDTH];

    for (u32 sx = 0; sx < SCREEN_WIDTH; sx++) {
        // H32 lines are stretched to the H40 framebuffer width, matching how
        // both modes filled the same area of a CRT.
        const u32 x = (width == SCREEN_WIDTH) ? sx : (sx * width) / SCREEN_WIDTH;

        u8 a = m_planeA[x];
        u8 b = m_planeB[x];
        u8 s = m_sprites[x];

        if (blankLeft && x < 8) {
            a = b = s = 0;
        }

        u8 aCol = a & PIXEL_COLOR;
        u8 bCol = b & PIXEL_COLOR;
        u8 sCol = s & PIXEL_COLOR;
        const bool aPrio = (a & PIXEL_PRIORITY) != 0;
        const bool bPrio = (b & PIXEL_PRIORITY) != 0;
        const bool sPrio = (s & PIXEL_PRIORITY) != 0;

        bool opShadow = false;
        bool opHighlight = false;
        bool areaShadow = false;

        if (ste) {
            areaShadow = !aPrio && !bPrio;
            if (sCol == OP_HIGHLIGHT) {
                opHighlight = true;
                sCol = 0;
            } else if (sCol == OP_SHADOW) {
                opShadow = true;
                sCol = 0;
            }
        }

        u8 color = backdrop;
        bool fromSprite = false;

        if (bCol && !bPrio) { color = bCol; fromSprite = false; }
        if (aCol && !aPrio) { color = aCol; fromSprite = false; }
        if (sCol && !sPrio) { color = sCol; fromSprite = true; }
        if (bCol && bPrio)  { color = bCol; fromSprite = false; }
        if (aCol && aPrio)  { color = aCol; fromSprite = false; }
        if (sCol && sPrio)  { color = sCol; fromSprite = true; }

        const u32* palette = m_paletteNormal.data();
        if (ste) {
            if (opShadow) {
                palette = m_paletteShadow.data();
            } else if (opHighlight) {
                palette = areaShadow ? m_paletteNormal.data() : m_paletteHighlight.data();
            } else if (areaShadow && !fromSprite) {
                palette = m_paletteShadow.data();
            }
        }

        out[sx] = palette[color];
    }
}

// ---------------------------------------------------------------------------
// Save states
// ---------------------------------------------------------------------------

template <typename Visit>
void VDP::visitState(Visit visit) {
    visit(m_vram);
    visit(m_cram);
    visit(m_vsram);
    visit(m_regs);

    visit(m_addr);
    visit(m_code);
    visit(m_pending);
    visit(m_readBuffer);
    visit(m_dmaFillPending);
    visit(m_status);
    visit(m_line);
    visit(m_hintCounter);
    visit(m_vintPending);
    visit(m_hintPending);
    visit(m_oddFrame);
}

void VDP::saveState(Buffer* buf) {
    visitState(StateWriter{buf});
}

void VDP::loadState(Buffer* buf) {
    visitState(StateReader{buf});

    for (u32 i = 0; i < CRAM_ENTRIES; i++) updatePaletteEntry(i);
}

} // namespace md

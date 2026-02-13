#include "ppu.h"
#include "memory.h"
#include "dma.h"
#include <cstring>

namespace gba {

PPU::PPU() {
    reset();
}

PPU::~PPU() {}

void PPU::reset() {
    std::memset(m_framebuffer, 0, sizeof(m_framebuffer));
    m_vcount = 0;
    m_cycles = 0;
    m_inHBlank = false;
    m_inVBlank = false;
}

void PPU::step(int cycles) {
    if (!m_memory) return;
    
    m_cycles += cycles;
    
    if (m_vcount < VISIBLE_LINES) {
        // Visible scanline
        if (!m_inHBlank && m_cycles >= HDRAW_CYCLES) {
            enterHBlank();
        }
        
        if (m_cycles >= SCANLINE_CYCLES) {
            m_cycles -= SCANLINE_CYCLES;
            m_vcount++;
            m_inHBlank = false;
            
            if (m_vcount == VISIBLE_LINES) {
                enterVBlank();
            }
            
            // Update DISPSTAT
            u16 dispstat = m_memory->readIO16(IO::DISPSTAT);
            dispstat &= ~(DISPSTAT::HBLANK_FLAG | DISPSTAT::VBLANK_FLAG | DISPSTAT::VCOUNTER_FLAG);
            if (m_inVBlank) dispstat |= DISPSTAT::VBLANK_FLAG;
            
            u16 vcountSetting = (dispstat >> DISPSTAT::VCOUNT_SETTING_SHIFT) & 0xFF;
            if (m_vcount == vcountSetting) {
                dispstat |= DISPSTAT::VCOUNTER_FLAG;
                if (dispstat & DISPSTAT::VCOUNTER_IRQ) {
                    m_memory->requestIRQ(IRQ::VCOUNTER);
                }
            }
            
            m_memory->writeIO16(IO::DISPSTAT, dispstat);
        }
    } else {
        // VBlank scanlines
        if (m_cycles >= SCANLINE_CYCLES) {
            m_cycles -= SCANLINE_CYCLES;
            m_vcount++;
            
            if (m_vcount >= TOTAL_LINES) {
                m_vcount = 0;
                m_inVBlank = false;
                
                // Render frame
                if (m_videoDevice) {
                    m_videoDevice->render(m_framebuffer);
                }
            }
            
            // Update DISPSTAT
            u16 dispstat = m_memory->readIO16(IO::DISPSTAT);
            dispstat &= ~(DISPSTAT::HBLANK_FLAG | DISPSTAT::VBLANK_FLAG | DISPSTAT::VCOUNTER_FLAG);
            if (m_inVBlank) dispstat |= DISPSTAT::VBLANK_FLAG;
            
            u16 vcountSetting = (dispstat >> DISPSTAT::VCOUNT_SETTING_SHIFT) & 0xFF;
            if (m_vcount == vcountSetting) {
                dispstat |= DISPSTAT::VCOUNTER_FLAG;
                if (dispstat & DISPSTAT::VCOUNTER_IRQ) {
                    m_memory->requestIRQ(IRQ::VCOUNTER);
                }
            }
            
            m_memory->writeIO16(IO::DISPSTAT, dispstat);
        }
    }
}

static inline u32 rgb555ToARGB(u16 color555) {
    u32 r = (color555 & 0x1F) << 3;
    u32 g = ((color555 >> 5) & 0x1F) << 3;
    u32 b = ((color555 >> 10) & 0x1F) << 3;
    return 0xFF000000 | (r << 16) | (g << 8) | b;
}

// Sprite size lookup table: [shape][size] = {width, height}
static const u8 OBJ_SIZES[3][4][2] = {
    // Square
    {{8,8}, {16,16}, {32,32}, {64,64}},
    // Horizontal
    {{16,8}, {32,8}, {32,16}, {64,32}},
    // Vertical
    {{8,16}, {8,32}, {16,32}, {32,64}},
};

void PPU::renderScanline() {
    if (!m_memory || m_vcount >= VISIBLE_LINES) return;
    
    u16 dispcnt = m_memory->readIO16(IO::DISPCNT);
    u8* palette = m_memory->getPalette();
    u8* vram = m_memory->getVRAM();
    u8* oam = m_memory->getOAM();
    u32* line = &m_framebuffer[m_vcount * SCREEN_WIDTH];
    int y = m_vcount;
    
    // Forced blank: white screen
    if (dispcnt & (1 << 7)) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            line[x] = 0xFFFFFFFF;
        }
        return;
    }
    
    int mode = dispcnt & 0x7;
    
    // Initialize scanline with backdrop color
    u16 backdrop = *reinterpret_cast<u16*>(&palette[0]);
    u32 backdropColor = rgb555ToARGB(backdrop);
    
    // Priority buffer: stores priority (0-3 for BG, 4 for backdrop, 5 for none)
    // Lower = higher priority
    u8 priorityBuf[SCREEN_WIDTH];
    for (int x = 0; x < SCREEN_WIDTH; x++) {
        line[x] = backdropColor;
        priorityBuf[x] = 4; // backdrop priority
    }
    
    // Render backgrounds from lowest priority to highest
    // Within same priority, BG3 < BG2 < BG1 < BG0 (BG0 wins)
    
    switch (mode) {
        case 0:
            // Mode 0: 4 text BGs
            for (int bg = 3; bg >= 0; bg--) {
                if (!(dispcnt & (1 << (8 + bg)))) continue;
                renderTextBG(bg, y, dispcnt, palette, vram, line, priorityBuf);
            }
            break;
        case 1:
            // Mode 1: BG0,BG1 text, BG2 affine
            if (dispcnt & (1 << 10)) renderAffineBG(2, y, dispcnt, palette, vram, line, priorityBuf);
            if (dispcnt & (1 << 9)) renderTextBG(1, y, dispcnt, palette, vram, line, priorityBuf);
            if (dispcnt & (1 << 8)) renderTextBG(0, y, dispcnt, palette, vram, line, priorityBuf);
            break;
        case 2:
            // Mode 2: BG2, BG3 affine
            if (dispcnt & (1 << 11)) renderAffineBG(3, y, dispcnt, palette, vram, line, priorityBuf);
            if (dispcnt & (1 << 10)) renderAffineBG(2, y, dispcnt, palette, vram, line, priorityBuf);
            break;
        case 3:
            // Mode 3: BG2 bitmap, 240x160 16bpp direct color
            if (dispcnt & (1 << 10)) renderBitmapMode3(y, vram, line, priorityBuf);
            break;
        case 4:
            // Mode 4: BG2 bitmap, 240x160 8bpp paletted
            if (dispcnt & (1 << 10)) renderBitmapMode4(y, dispcnt, palette, vram, line, priorityBuf);
            break;
        case 5:
            // Mode 5: BG2 bitmap, 160x128 16bpp direct color
            if (dispcnt & (1 << 10)) renderBitmapMode5(y, dispcnt, vram, line, priorityBuf);
            break;
    }
    
    // Render sprites (OBJ)
    if (dispcnt & (1 << 12)) {
        renderSprites(y, dispcnt, palette, vram, oam, line, priorityBuf);
    }
}

void PPU::renderTextBG(int bg, int y, u16 dispcnt, u8* palette, u8* vram, u32* line, u8* priorityBuf) {
    u16 bgcnt = m_memory->readIO16(IO::BG0CNT + bg * 2);
    int priority = bgcnt & 3;
    u32 charBase = ((bgcnt >> 2) & 3) * 0x4000;
    bool is8bpp = (bgcnt & (1 << 7)) != 0;
    u32 screenBase = ((bgcnt >> 8) & 0x1F) * 0x800;
    int bgSize = (bgcnt >> 14) & 3;
    
    // BG scroll offsets
    int scrollX = m_memory->readIO16(IO::BG0HOFS + bg * 4) & 0x1FF;
    int scrollY = m_memory->readIO16(IO::BG0VOFS + bg * 4) & 0x1FF;
    
    // Map dimensions in tiles
    int mapWidth = (bgSize & 1) ? 64 : 32;
    int mapHeight = (bgSize & 2) ? 64 : 32;
    
    int srcY = (y + scrollY) & ((mapHeight * 8) - 1);
    int tileRow = srcY / 8;
    int tileYOffset = srcY & 7;
    
    for (int x = 0; x < SCREEN_WIDTH; x++) {
        int srcX = (x + scrollX) & ((mapWidth * 8) - 1);
        int tileCol = srcX / 8;
        int tileXOffset = srcX & 7;
        
        // Calculate screen block offset
        // For 64-wide maps, second half uses next screen block
        u32 mapOffset = screenBase;
        int localCol = tileCol;
        int localRow = tileRow;
        
        if (mapWidth == 64 && tileCol >= 32) {
            mapOffset += 0x800;
            localCol -= 32;
        }
        if (mapHeight == 64 && tileRow >= 32) {
            mapOffset += (mapWidth == 64) ? 0x1000 : 0x800;
            localRow -= 32;
        }
        
        u32 mapAddr = mapOffset + (localRow * 32 + localCol) * 2;
        u16 mapEntry = *reinterpret_cast<u16*>(&vram[mapAddr & 0x1FFFF]);
        
        int tileIndex = mapEntry & 0x3FF;
        bool hFlip = (mapEntry & (1 << 10)) != 0;
        bool vFlip = (mapEntry & (1 << 11)) != 0;
        int palIndex = (mapEntry >> 12) & 0xF;
        
        int pixelX = hFlip ? (7 - tileXOffset) : tileXOffset;
        int pixelY = vFlip ? (7 - tileYOffset) : tileYOffset;
        
        u8 colorIndex;
        if (is8bpp) {
            // 8bpp: 64 bytes per tile
            u32 tileAddr = charBase + tileIndex * 64 + pixelY * 8 + pixelX;
            colorIndex = vram[tileAddr & 0x1FFFF];
        } else {
            // 4bpp: 32 bytes per tile
            u32 tileAddr = charBase + tileIndex * 32 + pixelY * 4 + pixelX / 2;
            u8 byte = vram[tileAddr & 0x1FFFF];
            colorIndex = (pixelX & 1) ? (byte >> 4) : (byte & 0xF);
            if (colorIndex != 0) colorIndex += palIndex * 16;
        }
        
        if (colorIndex == 0) continue; // Transparent
        
        // Only draw if this BG has equal or better priority
        if (priority <= priorityBuf[x]) {
            u16 color555 = *reinterpret_cast<u16*>(&palette[colorIndex * 2]);
            line[x] = rgb555ToARGB(color555);
            priorityBuf[x] = priority;
        }
    }
}

void PPU::renderAffineBG(int bg, int y, u16 dispcnt, u8* palette, u8* vram, u32* line, u8* priorityBuf) {
    u16 bgcnt = m_memory->readIO16(IO::BG0CNT + bg * 2);
    int priority = bgcnt & 3;
    u32 charBase = ((bgcnt >> 2) & 3) * 0x4000;
    u32 screenBase = ((bgcnt >> 8) & 0x1F) * 0x800;
    int bgSize = (bgcnt >> 14) & 3;
    bool overflow = (bgcnt & (1 << 13)) != 0; // Wraparound
    
    // Affine BG sizes: 128, 256, 512, 1024 pixels
    int size = 128 << bgSize;
    int mapSize = size / 8; // tiles per dimension
    
    // Get affine parameters
    u32 baseIO = (bg == 2) ? IO::BG2PA : IO::BG3PA;
    s16 pa = (s16)m_memory->readIO16(baseIO);
    s16 pb = (s16)m_memory->readIO16(baseIO + 2);
    s16 pc = (s16)m_memory->readIO16(baseIO + 4);
    s16 pd = (s16)m_memory->readIO16(baseIO + 6);
    
    // Reference point (28.8 fixed-point stored across two 16-bit regs)
    u32 baseRef = (bg == 2) ? IO::BG2X : IO::BG3X;
    s32 refX = (s32)m_memory->readIO32(baseRef);
    s32 refY = (s32)m_memory->readIO32(baseRef + 4);
    // Sign extend from 28 bits
    refX = (refX << 4) >> 4;
    refY = (refY << 4) >> 4;
    
    // Calculate starting position for this scanline
    s32 cx = refX + pb * y;
    s32 cy = refY + pd * y;
    
    for (int x = 0; x < SCREEN_WIDTH; x++) {
        s32 texX = (cx + pa * x) >> 8;
        s32 texY = (cy + pc * x) >> 8;
        
        if (overflow) {
            texX &= (size - 1);
            texY &= (size - 1);
        } else {
            if (texX < 0 || texX >= size || texY < 0 || texY >= size) continue;
        }
        
        int tileX = texX / 8;
        int tileY = texY / 8;
        int pixelX = texX & 7;
        int pixelY = texY & 7;
        
        // Affine BGs use 8-bit map entries (just tile index)
        u32 mapAddr = screenBase + tileY * mapSize + tileX;
        u8 tileIndex = vram[mapAddr & 0x1FFFF];
        
        // Always 8bpp
        u32 tileAddr = charBase + tileIndex * 64 + pixelY * 8 + pixelX;
        u8 colorIndex = vram[tileAddr & 0x1FFFF];
        
        if (colorIndex == 0) continue;
        
        if (priority <= priorityBuf[x]) {
            u16 color555 = *reinterpret_cast<u16*>(&palette[colorIndex * 2]);
            line[x] = rgb555ToARGB(color555);
            priorityBuf[x] = priority;
        }
    }
}

void PPU::renderBitmapMode3(int y, u8* vram, u32* line, u8* priorityBuf) {
    u16 bgcnt = m_memory->readIO16(IO::BG2CNT);
    int priority = bgcnt & 3;
    
    for (int x = 0; x < SCREEN_WIDTH; x++) {
        u32 addr = (y * SCREEN_WIDTH + x) * 2;
        u16 color555 = *reinterpret_cast<u16*>(&vram[addr]);
        if (priority <= priorityBuf[x]) {
            line[x] = rgb555ToARGB(color555);
            priorityBuf[x] = priority;
        }
    }
}

void PPU::renderBitmapMode4(int y, u16 dispcnt, u8* palette, u8* vram, u32* line, u8* priorityBuf) {
    u16 bgcnt = m_memory->readIO16(IO::BG2CNT);
    int priority = bgcnt & 3;
    u32 base = (dispcnt & (1 << 4)) ? 0xA000 : 0;
    
    for (int x = 0; x < SCREEN_WIDTH; x++) {
        u8 colorIndex = vram[base + y * SCREEN_WIDTH + x];
        if (colorIndex == 0) continue;
        
        if (priority <= priorityBuf[x]) {
            u16 color555 = *reinterpret_cast<u16*>(&palette[colorIndex * 2]);
            line[x] = rgb555ToARGB(color555);
            priorityBuf[x] = priority;
        }
    }
}

void PPU::renderBitmapMode5(int y, u16 dispcnt, u8* vram, u32* line, u8* priorityBuf) {
    if (y >= 128) return; // Mode 5 is only 160x128
    
    u16 bgcnt = m_memory->readIO16(IO::BG2CNT);
    int priority = bgcnt & 3;
    u32 base = (dispcnt & (1 << 4)) ? 0xA000 : 0;
    
    for (int x = 0; x < 160 && x < SCREEN_WIDTH; x++) {
        u32 addr = base + (y * 160 + x) * 2;
        u16 color555 = *reinterpret_cast<u16*>(&vram[addr]);
        if (priority <= priorityBuf[x]) {
            line[x] = rgb555ToARGB(color555);
            priorityBuf[x] = priority;
        }
    }
}

void PPU::renderSprites(int y, u16 dispcnt, u8* palette, u8* vram, u8* oam, u32* line, u8* priorityBuf) {
    bool objMapping1D = (dispcnt & (1 << 6)) != 0;
    int mode = dispcnt & 7;
    
    // OBJ VRAM starts at 0x10000
    u8* objVram = &vram[0x10000];
    u8* objPalette = &palette[0x200]; // OBJ palette at 0x200
    
    // Iterate sprites in reverse order (lower index = higher priority within same OBJ priority)
    for (int i = 127; i >= 0; i--) {
        u16 attr0 = *reinterpret_cast<u16*>(&oam[i * 8]);
        u16 attr1 = *reinterpret_cast<u16*>(&oam[i * 8 + 2]);
        u16 attr2 = *reinterpret_cast<u16*>(&oam[i * 8 + 4]);
        
        // Check if disabled
        bool isAffine = (attr0 & (1 << 8)) != 0;
        if (!isAffine && (attr0 & (1 << 9))) continue; // Disabled
        
        int shape = (attr0 >> 14) & 3;
        if (shape == 3) continue; // Invalid
        int size = (attr1 >> 14) & 3;
        
        int objWidth = OBJ_SIZES[shape][size][0];
        int objHeight = OBJ_SIZES[shape][size][1];
        
        int objY = attr0 & 0xFF;
        int objX = attr1 & 0x1FF;
        if (objX >= 240) objX -= 512;
        if (objY >= 160) objY -= 256;
        
        int priority = (attr2 >> 10) & 3;
        int tileIndex = attr2 & 0x3FF;
        bool is8bpp = (attr0 & (1 << 13)) != 0;
        int palNum = (attr2 >> 12) & 0xF;
        
        // In bitmap modes, tiles < 512 are inaccessible for OBJ
        if (mode >= 3 && tileIndex < 512) continue;
        
        int renderWidth = objWidth;
        int renderHeight = objHeight;
        
        // Affine double-size
        bool doubleSize = false;
        if (isAffine && (attr0 & (1 << 9))) {
            doubleSize = true;
            renderWidth *= 2;
            renderHeight *= 2;
        }
        
        // Check if this sprite is on the current scanline
        int localY = y - objY;
        if (localY < 0 || localY >= renderHeight) continue;
        
        if (isAffine) {
            // Affine sprite
            int affineIndex = (attr1 >> 9) & 0x1F;
            s16 pa = *reinterpret_cast<s16*>(&oam[affineIndex * 32 + 6]);
            s16 pb = *reinterpret_cast<s16*>(&oam[affineIndex * 32 + 14]);
            s16 pc = *reinterpret_cast<s16*>(&oam[affineIndex * 32 + 22]);
            s16 pd = *reinterpret_cast<s16*>(&oam[affineIndex * 32 + 30]);
            
            int halfW = renderWidth / 2;
            int halfH = renderHeight / 2;
            
            for (int sx = 0; sx < renderWidth; sx++) {
                int screenX = objX + sx;
                if (screenX < 0 || screenX >= SCREEN_WIDTH) continue;
                
                // Transform to texture coordinates
                int dx = sx - halfW;
                int dy = localY - halfH;
                int texX = ((pa * dx + pb * dy) >> 8) + objWidth / 2;
                int texY = ((pc * dx + pd * dy) >> 8) + objHeight / 2;
                
                if (texX < 0 || texX >= objWidth || texY < 0 || texY >= objHeight) continue;
                
                u8 colorIndex = getObjPixel(tileIndex, texX, texY, objWidth, is8bpp, objMapping1D, objVram);
                if (colorIndex == 0) continue;
                
                if (priority <= priorityBuf[screenX]) {
                    u16 color555;
                    if (is8bpp) {
                        color555 = *reinterpret_cast<u16*>(&objPalette[colorIndex * 2]);
                    } else {
                        color555 = *reinterpret_cast<u16*>(&objPalette[(palNum * 16 + colorIndex) * 2]);
                    }
                    line[screenX] = rgb555ToARGB(color555);
                    priorityBuf[screenX] = priority;
                }
            }
        } else {
            // Normal sprite
            bool hFlip = (attr1 & (1 << 12)) != 0;
            bool vFlip = (attr1 & (1 << 13)) != 0;
            
            int texY = vFlip ? (objHeight - 1 - localY) : localY;
            
            for (int sx = 0; sx < objWidth; sx++) {
                int screenX = objX + sx;
                if (screenX < 0 || screenX >= SCREEN_WIDTH) continue;
                
                int texX = hFlip ? (objWidth - 1 - sx) : sx;
                
                u8 colorIndex = getObjPixel(tileIndex, texX, texY, objWidth, is8bpp, objMapping1D, objVram);
                if (colorIndex == 0) continue;
                
                if (priority <= priorityBuf[screenX]) {
                    u16 color555;
                    if (is8bpp) {
                        color555 = *reinterpret_cast<u16*>(&objPalette[colorIndex * 2]);
                    } else {
                        color555 = *reinterpret_cast<u16*>(&objPalette[(palNum * 16 + colorIndex) * 2]);
                    }
                    line[screenX] = rgb555ToARGB(color555);
                    priorityBuf[screenX] = priority;
                }
            }
        }
    }
}

u8 PPU::getObjPixel(int tileIndex, int x, int y, int objWidth, bool is8bpp, bool mapping1D, u8* objVram) {
    int tileX = x / 8;
    int tileY = y / 8;
    int pixelX = x & 7;
    int pixelY = y & 7;
    
    int tile;
    if (mapping1D) {
        // 1D mapping: tiles are linear
        if (is8bpp) {
            tile = tileIndex + tileY * (objWidth / 8) * 2 + tileX * 2;
        } else {
            tile = tileIndex + tileY * (objWidth / 8) + tileX;
        }
    } else {
        // 2D mapping: 32 tiles per row in VRAM
        if (is8bpp) {
            tile = tileIndex + tileY * 32 + tileX * 2;
        } else {
            tile = tileIndex + tileY * 32 + tileX;
        }
    }
    
    if (is8bpp) {
        u32 addr = tile * 32 + pixelY * 8 + pixelX;
        if (addr >= 0x8000) return 0; // OBJ VRAM is 32KB
        return objVram[addr];
    } else {
        u32 addr = tile * 32 + pixelY * 4 + pixelX / 2;
        if (addr >= 0x8000) return 0;
        u8 byte = objVram[addr];
        return (pixelX & 1) ? (byte >> 4) : (byte & 0xF);
    }
}

void PPU::enterHBlank() {
    m_inHBlank = true;
    
    if (!m_memory) return;
    
    renderScanline();
    
    u16 dispstat = m_memory->readIO16(IO::DISPSTAT);
    dispstat |= DISPSTAT::HBLANK_FLAG;
    m_memory->writeIO16(IO::DISPSTAT, dispstat);
    
    if (dispstat & DISPSTAT::HBLANK_IRQ) {
        m_memory->requestIRQ(IRQ::HBLANK);
    }
    
    // Trigger HBlank DMA
    if (m_dma) m_dma->runHBlank();
}

void PPU::enterVBlank() {
    m_inVBlank = true;
    
    if (!m_memory) return;
    
    u16 dispstat = m_memory->readIO16(IO::DISPSTAT);
    dispstat |= DISPSTAT::VBLANK_FLAG;
    m_memory->writeIO16(IO::DISPSTAT, dispstat);
    
    if (dispstat & DISPSTAT::VBLANK_IRQ) {
        m_memory->requestIRQ(IRQ::VBLANK);
    }
    
    // Trigger VBlank DMA
    if (m_dma) m_dma->runVBlank();
}

void PPU::writeRegister(u32 offset, u16 value) {
    // PPU registers are written through memory
    // This function can be used for special register handling if needed
    (void)offset;
    (void)value;
}

void PPU::saveState(Buffer* buf) {
    buffer_write(buf, m_framebuffer, sizeof(m_framebuffer));
    buffer_write(buf, &m_vcount, sizeof(m_vcount));
    buffer_write(buf, &m_cycles, sizeof(m_cycles));
    buffer_write(buf, &m_inHBlank, sizeof(m_inHBlank));
    buffer_write(buf, &m_inVBlank, sizeof(m_inVBlank));
}

void PPU::loadState(Buffer* buf) {
    buffer_read(buf, m_framebuffer, sizeof(m_framebuffer));
    buffer_read(buf, &m_vcount, sizeof(m_vcount));
    buffer_read(buf, &m_cycles, sizeof(m_cycles));
    buffer_read(buf, &m_inHBlank, sizeof(m_inHBlank));
    buffer_read(buf, &m_inVBlank, sizeof(m_inVBlank));
}

} // namespace gba

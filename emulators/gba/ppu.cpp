#include "ppu.h"
#include "memory.h"
#include "dma.h"
#include <cstring>
#include <algorithm>

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
    m_bg2RefX = 0;
    m_bg2RefY = 0;
    m_bg3RefX = 0;
    m_bg3RefY = 0;
}

void PPU::step(int cycles) {
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
            
            updateDispstat();
        }
    } else {
        // VBlank scanlines — HBlank still fires for IRQ
        if (!m_inHBlank && m_cycles >= HDRAW_CYCLES) {
            m_inHBlank = true;
            
            u16 dispstat = m_memory->readDISPSTAT();
            dispstat |= DISPSTAT::HBLANK_FLAG;
            m_memory->writeDISPSTAT(dispstat);
            
            if (dispstat & DISPSTAT::HBLANK_IRQ) {
                m_memory->requestIRQ(IRQ::HBLANK);
            }
            // Trigger video capture DMA for vcounts 160-161 (VISIBLE_LINES to VISIBLE_LINES+1)
            if (m_vcount < VISIBLE_LINES + 2) m_dma->runDisplayStart(m_vcount);
            // No HBlank DMA during VBlank
        }
        
        if (m_cycles >= SCANLINE_CYCLES) {
            m_cycles -= SCANLINE_CYCLES;
            m_vcount++;
            m_inHBlank = false;
            
            // VBlank flag clears at scanline 227 (last VBlank line)
            if (m_vcount == TOTAL_LINES - 1) {
                m_inVBlank = false;
            }
            
            if (m_vcount >= TOTAL_LINES) {
                m_vcount = 0;
                
                // Render frame
                if (m_videoDevice) {
                    m_videoDevice->render(m_framebuffer);
                }
            }
            
            updateDispstat();
        }
    }
}

void PPU::updateDispstat() {
    u16 dispstat = m_memory->readDISPSTAT();
    dispstat &= ~(DISPSTAT::HBLANK_FLAG | DISPSTAT::VBLANK_FLAG | DISPSTAT::VCOUNTER_FLAG);
    if (m_inVBlank) dispstat |= DISPSTAT::VBLANK_FLAG;
    
    u16 vcountSetting = (dispstat >> DISPSTAT::VCOUNT_SETTING_SHIFT) & 0xFF;
    if (m_vcount == vcountSetting) {
        dispstat |= DISPSTAT::VCOUNTER_FLAG;
        if (dispstat & DISPSTAT::VCOUNTER_IRQ) {
            m_memory->requestIRQ(IRQ::VCOUNTER);
        }
    }
    
    m_memory->writeDISPSTAT(dispstat);
}

static inline u32 rgb555ToARGB(u16 color555) {
    u32 r = (color555 & 0x1F) << 3;
    u32 g = ((color555 >> 5) & 0x1F) << 3;
    u32 b = ((color555 >> 10) & 0x1F) << 3;
    return 0xFF000000 | (r << 16) | (g << 8) | b;
}

static inline u16 blendAlpha(u16 ca, u16 cb, int eva, int evb) {
    int r = std::min(31, ((ca & 0x1F) * eva + (cb & 0x1F) * evb) / 16);
    int g = std::min(31, (((ca >> 5) & 0x1F) * eva + ((cb >> 5) & 0x1F) * evb) / 16);
    int b = std::min(31, (((ca >> 10) & 0x1F) * eva + ((cb >> 10) & 0x1F) * evb) / 16);
    return (u16)(r | (g << 5) | (b << 10));
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

void PPU::computeObjWindowMask(int y, u16 dispcnt, u8* oam, u8* vram, bool* mask) {
    // Initialize mask to false
    for (int x = 0; x < SCREEN_WIDTH; x++) mask[x] = false;
    
    bool objMapping1D = (dispcnt & (1 << 6)) != 0;
    u8* objVram = &vram[0x10000];
    
    for (int i = 127; i >= 0; i--) {
        u16 attr0 = *reinterpret_cast<u16*>(&oam[i * 8]);
        u16 attr1 = *reinterpret_cast<u16*>(&oam[i * 8 + 2]);
        u16 attr2 = *reinterpret_cast<u16*>(&oam[i * 8 + 4]);
        
        bool isAffine = (attr0 & (1 << 8)) != 0;
        if (!isAffine && (attr0 & (1 << 9))) continue; // Disabled
        
        int objMode = (attr0 >> 10) & 3;
        if (objMode != 2) continue; // Only OBJ window sprites
        
        int shape = (attr0 >> 14) & 3;
        if (shape == 3) continue;
        int size = (attr1 >> 14) & 3;
        
        int objWidth = OBJ_SIZES[shape][size][0];
        int objHeight = OBJ_SIZES[shape][size][1];
        
        int objY = attr0 & 0xFF;
        int objX = attr1 & 0x1FF;
        if (objX >= 240) objX -= 512;
        if (objY >= 160) objY -= 256;
        
        int tileIndex = attr2 & 0x3FF;
        bool is8bpp = (attr0 & (1 << 13)) != 0;
        
        int renderWidth = objWidth;
        int renderHeight = objHeight;
        if (isAffine && (attr0 & (1 << 9))) {
            renderWidth *= 2;
            renderHeight *= 2;
        }
        
        int localY = y - objY;
        if (localY < 0 || localY >= renderHeight) continue;
        
        if (isAffine) {
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
                int dx = sx - halfW;
                int dy = localY - halfH;
                int texX = ((pa * dx + pb * dy) >> 8) + objWidth / 2;
                int texY = ((pc * dx + pd * dy) >> 8) + objHeight / 2;
                if (texX < 0 || texX >= objWidth || texY < 0 || texY >= objHeight) continue;
                u8 ci = getObjPixel(tileIndex, texX, texY, objWidth, is8bpp, objMapping1D, objVram);
                if (ci != 0) mask[screenX] = true;
            }
        } else {
            bool hFlip = (attr1 & (1 << 12)) != 0;
            bool vFlip = (attr1 & (1 << 13)) != 0;
            int texY = vFlip ? (objHeight - 1 - localY) : localY;
            for (int sx = 0; sx < objWidth; sx++) {
                int screenX = objX + sx;
                if (screenX < 0 || screenX >= SCREEN_WIDTH) continue;
                int texX = hFlip ? (objWidth - 1 - sx) : sx;
                u8 ci = getObjPixel(tileIndex, texX, texY, objWidth, is8bpp, objMapping1D, objVram);
                if (ci != 0) mask[screenX] = true;
            }
        }
    }
}

void PPU::computeWindowFlags(u16 dispcnt, int y, u8* oam, u8* vram, u8* windowFlags) {
    bool win0Enable = (dispcnt & (1 << 13)) != 0;
    bool win1Enable = (dispcnt & (1 << 14)) != 0;
    bool objWinEnable = (dispcnt & (1 << 15)) != 0;
    
    // If no windows are enabled, all layers and SFX are fully enabled
    if (!win0Enable && !win1Enable && !objWinEnable) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            windowFlags[x] = 0x3F; // All bits set (BG0-3, OBJ, SFX)
        }
        return;
    }
    
    // Read window registers
    u16 win0h = m_memory->readIO16(IO::WIN0H);
    u16 win1h = m_memory->readIO16(IO::WIN1H);
    u16 win0v = m_memory->readIO16(IO::WIN0V);
    u16 win1v = m_memory->readIO16(IO::WIN1V);
    u16 winin = m_memory->readIO16(IO::WININ);
    u16 winout = m_memory->readIO16(IO::WINOUT);
    
    // Window boundaries
    int win0Left  = (win0h >> 8) & 0xFF;
    int win0Right = win0h & 0xFF;
    int win0Top   = (win0v >> 8) & 0xFF;
    int win0Bot   = win0v & 0xFF;
    
    int win1Left  = (win1h >> 8) & 0xFF;
    int win1Right = win1h & 0xFF;
    int win1Top   = (win1v >> 8) & 0xFF;
    int win1Bot   = win1v & 0xFF;
    
    // Check vertical range for WIN0 and WIN1
    // Handle wrapping: if top > bottom, window covers [top..227] and [0..bottom)
    bool win0InY;
    if (win0Top <= win0Bot) {
        win0InY = (y >= win0Top && y < win0Bot);
    } else {
        win0InY = (y >= win0Top || y < win0Bot);
    }
    
    bool win1InY;
    if (win1Top <= win1Bot) {
        win1InY = (y >= win1Top && y < win1Bot);
    } else {
        win1InY = (y >= win1Top || y < win1Bot);
    }
    
    // Control bits for each window region (6 bits each)
    u8 win0Flags = winin & 0x3F;
    u8 win1Flags = (winin >> 8) & 0x3F;
    u8 outsideFlags = winout & 0x3F;
    u8 objWinFlags = (winout >> 8) & 0x3F;
    
    // Compute OBJ window mask if needed
    bool objWinMask[SCREEN_WIDTH];
    if (objWinEnable) {
        computeObjWindowMask(y, dispcnt, oam, vram, objWinMask);
    }
    
    for (int x = 0; x < SCREEN_WIDTH; x++) {
        // Priority: WIN0 > WIN1 > OBJ WIN > Outside
        if (win0Enable && win0InY) {
            bool inX;
            if (win0Left <= win0Right) {
                inX = (x >= win0Left && x < win0Right);
            } else {
                inX = (x >= win0Left || x < win0Right);
            }
            if (inX) {
                windowFlags[x] = win0Flags;
                continue;
            }
        }
        
        if (win1Enable && win1InY) {
            bool inX;
            if (win1Left <= win1Right) {
                inX = (x >= win1Left && x < win1Right);
            } else {
                inX = (x >= win1Left || x < win1Right);
            }
            if (inX) {
                windowFlags[x] = win1Flags;
                continue;
            }
        }
        
        if (objWinEnable && objWinMask[x]) {
            windowFlags[x] = objWinFlags;
            continue;
        }
        
        windowFlags[x] = outsideFlags;
    }
}

void PPU::renderScanline() {
    if (m_vcount >= VISIBLE_LINES) return;
    
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
    
    // Two-pixel compositing buffers
    ScanPixel top[SCREEN_WIDTH];
    ScanPixel bot[SCREEN_WIDTH];
    bool objSemiTransparent[SCREEN_WIDTH];
    
    for (int x = 0; x < SCREEN_WIDTH; x++) {
        top[x] = { backdrop, LAYER_BD, 4 };
        bot[x] = { backdrop, LAYER_BD, 4 };
        objSemiTransparent[x] = false;
    }
    
    // Compute per-pixel window flags
    u8 windowFlags[SCREEN_WIDTH];
    computeWindowFlags(dispcnt, y, oam, vram, windowFlags);
    
    // Decode MOSAIC register once for all layers
    u16 mosaicReg = m_memory->readIO16(IO::MOSAIC);
    int bgMosaicH  = (mosaicReg & 0xF) + 1;
    int bgMosaicV  = ((mosaicReg >> 4) & 0xF) + 1;
    int objMosaicH = ((mosaicReg >> 8) & 0xF) + 1;
    int objMosaicV = ((mosaicReg >> 12) & 0xF) + 1;
    
    // Render backgrounds from lowest priority to highest
    // Within same priority, BG3 < BG2 < BG1 < BG0 (BG0 wins)
    
    switch (mode) {
        case 0:
            // Mode 0: 4 text BGs
            for (int bg = 3; bg >= 0; bg--) {
                if (!(dispcnt & (1 << (8 + bg)))) continue;
                renderTextBG(bg, y, dispcnt, palette, vram, top, bot, windowFlags, bgMosaicH, bgMosaicV);
            }
            break;
        case 1:
            // Mode 1: BG0,BG1 text, BG2 affine
            if (dispcnt & (1 << 10)) renderAffineBG(2, y, dispcnt, palette, vram, top, bot, windowFlags, bgMosaicH, bgMosaicV);
            if (dispcnt & (1 << 9)) renderTextBG(1, y, dispcnt, palette, vram, top, bot, windowFlags, bgMosaicH, bgMosaicV);
            if (dispcnt & (1 << 8)) renderTextBG(0, y, dispcnt, palette, vram, top, bot, windowFlags, bgMosaicH, bgMosaicV);
            break;
        case 2:
            // Mode 2: BG2, BG3 affine
            if (dispcnt & (1 << 11)) renderAffineBG(3, y, dispcnt, palette, vram, top, bot, windowFlags, bgMosaicH, bgMosaicV);
            if (dispcnt & (1 << 10)) renderAffineBG(2, y, dispcnt, palette, vram, top, bot, windowFlags, bgMosaicH, bgMosaicV);
            break;
        case 3:
            // Mode 3: BG2 bitmap, 240x160 16bpp direct color
            if (dispcnt & (1 << 10)) renderBitmapMode3(y, vram, top, bot, windowFlags, bgMosaicH, bgMosaicV);
            break;
        case 4:
            // Mode 4: BG2 bitmap, 240x160 8bpp paletted
            if (dispcnt & (1 << 10)) renderBitmapMode4(y, dispcnt, palette, vram, top, bot, windowFlags, bgMosaicH, bgMosaicV);
            break;
        case 5:
            // Mode 5: BG2 bitmap, 160x128 16bpp direct color
            if (dispcnt & (1 << 10)) renderBitmapMode5(y, dispcnt, vram, top, bot, windowFlags, bgMosaicH, bgMosaicV);
            break;
    }
    
    // Render sprites (OBJ)
    if (dispcnt & (1 << 12)) {
        renderSprites(y, dispcnt, palette, vram, oam, top, bot, objSemiTransparent, windowFlags, objMosaicH, objMosaicV);
    }
    
    // Apply color special effects (blending) and write final pixels
    composeScanline(line, top, bot, objSemiTransparent, windowFlags);
}

void PPU::renderTextBG(int bg, int y, [[maybe_unused]] u16 dispcnt, u8* palette, u8* vram,
                       ScanPixel* top, ScanPixel* bot, const u8* windowFlags,
                       int bgMosaicH, int bgMosaicV) {
    u16 bgcnt = m_memory->readIO16(IO::BG0CNT + bg * 2);
    int priority = bgcnt & 3;
    u8 layer = LAYER_BG0 + bg;
    u8 layerBit = 1 << bg; // Window flag bit for this BG
    u32 charBase = ((bgcnt >> 2) & 3) * 0x4000;
    bool is8bpp = (bgcnt & (1 << 7)) != 0;
    u32 screenBase = ((bgcnt >> 8) & 0x1F) * 0x800;
    int bgSize = (bgcnt >> 14) & 3;
    
    // BG scroll offsets
    int scrollX = m_memory->readIO16(IO::BG0HOFS + bg * 4) & 0x1FF;
    int scrollY = m_memory->readIO16(IO::BG0VOFS + bg * 4) & 0x1FF;
    
    // Apply mosaic only if enabled for this BG (BGCNT bit 6)
    bool mosaicEnable = (bgcnt & (1 << 6)) != 0;
    int mH = mosaicEnable ? bgMosaicH : 1;
    int mV = mosaicEnable ? bgMosaicV : 1;
    int effectiveY = y - (y % mV);
    
    // Map dimensions in tiles
    int mapWidth = (bgSize & 1) ? 64 : 32;
    int mapHeight = (bgSize & 2) ? 64 : 32;
    
    int srcY = (effectiveY + scrollY) & ((mapHeight * 8) - 1);
    int tileRow = srcY / 8;
    int tileYOffset = srcY & 7;
    
    for (int x = 0; x < SCREEN_WIDTH; x++) {
        int effectiveX = x - (x % mH);
        int srcX = (effectiveX + scrollX) & ((mapWidth * 8) - 1);
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
        
        // Check window visibility for this BG layer
        if (!(windowFlags[x] & layerBit)) continue;
        
        // Place pixel into compositing buffers
        u16 color555 = *reinterpret_cast<u16*>(&palette[colorIndex * 2]);
        placePixel(top, bot, x, color555, layer, priority);
    }
}

void PPU::renderAffineBG(int bg, [[maybe_unused]] int y, [[maybe_unused]] u16 dispcnt, u8* palette, u8* vram,
                         ScanPixel* top, ScanPixel* bot, const u8* windowFlags,
                         int bgMosaicH, int bgMosaicV) {
    u16 bgcnt = m_memory->readIO16(IO::BG0CNT + bg * 2);
    int priority = bgcnt & 3;
    u8 layer = LAYER_BG0 + bg;
    u8 layerBit = 1 << bg;
    u32 charBase = ((bgcnt >> 2) & 3) * 0x4000;
    u32 screenBase = ((bgcnt >> 8) & 0x1F) * 0x800;
    int bgSize = (bgcnt >> 14) & 3;
    bool overflow = (bgcnt & (1 << 13)) != 0; // Wraparound
    
    // Apply mosaic only if enabled for this BG (BGCNT bit 6)
    bool mosaicEnable = (bgcnt & (1 << 6)) != 0;
    int mH = mosaicEnable ? bgMosaicH : 1;
    (void)bgMosaicV; // Vertical mosaic handled via internal affine ref point tracking
    
    // Affine BG sizes: 128, 256, 512, 1024 pixels
    int size = 128 << bgSize;
    int mapSize = size / 8; // tiles per dimension
    
    // Get affine parameters
    u32 baseIO = (bg == 2) ? IO::BG2PA : IO::BG3PA;
    s16 pa = (s16)m_memory->readIO16(baseIO);
    s16 pc = (s16)m_memory->readIO16(baseIO + 4);
    
    // Reference point — use internal affine tracking
    int bgIdx = bg - 2; // 0 for BG2, 1 for BG3
    s32 refX = (bgIdx == 0) ? m_bg2RefX : m_bg3RefX;
    s32 refY = (bgIdx == 0) ? m_bg2RefY : m_bg3RefY;
    
    // Calculate starting position for this scanline
    s32 cx = refX;
    s32 cy = refY;
    
    for (int x = 0; x < SCREEN_WIDTH; x++) {
        int effectiveX = x - (x % mH);
        s32 texX = (cx + pa * effectiveX) >> 8;
        s32 texY = (cy + pc * effectiveX) >> 8;
        
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
        
        if (!(windowFlags[x] & layerBit)) continue;
        
        u16 color555 = *reinterpret_cast<u16*>(&palette[colorIndex * 2]);
        placePixel(top, bot, x, color555, layer, priority);
    }
}

void PPU::renderBitmapMode3([[maybe_unused]] int y, u8* vram,
                            ScanPixel* top, ScanPixel* bot, const u8* windowFlags,
                            int bgMosaicH, int bgMosaicV) {
    u16 bgcnt = m_memory->readIO16(IO::BG2CNT);
    int priority = bgcnt & 3;
    
    // Apply mosaic only if enabled for this BG (BGCNT bit 6)
    bool mosaicEnable = (bgcnt & (1 << 6)) != 0;
    int mH = mosaicEnable ? bgMosaicH : 1;
    (void)bgMosaicV; // Vertical mosaic handled via internal affine ref point tracking
    
    // Affine parameters: PA (dx per pixel), PC (dy per pixel)
    s16 pa = (s16)m_memory->readIO16(IO::BG2PA);
    s16 pc = (s16)m_memory->readIO16(IO::BG2PA + 4);
    
    // Internal reference points (updated per scanline by PB/PD in enterHBlank)
    s32 refX = m_bg2RefX;
    s32 refY = m_bg2RefY;
    
    for (int x = 0; x < SCREEN_WIDTH; x++) {
        if (!(windowFlags[x] & (1 << 2))) continue; // BG2 window check
        int effectiveX = x - (x % mH);
        s32 texX = (refX + pa * effectiveX) >> 8;
        s32 texY = (refY + pc * effectiveX) >> 8;
        
        // Bitmap modes do not wrap — clip to source bounds
        if (texX < 0 || texX >= 240 || texY < 0 || texY >= 160) continue;
        
        u32 addr = (texY * 240 + texX) * 2;
        u16 color555 = *reinterpret_cast<u16*>(&vram[addr]);
        placePixel(top, bot, x, color555, LAYER_BG2, priority);
    }
}

void PPU::renderBitmapMode4([[maybe_unused]] int y, u16 dispcnt, u8* palette, u8* vram,
                            ScanPixel* top, ScanPixel* bot, const u8* windowFlags,
                            int bgMosaicH, int bgMosaicV) {
    u16 bgcnt = m_memory->readIO16(IO::BG2CNT);
    int priority = bgcnt & 3;
    u32 base = (dispcnt & (1 << 4)) ? 0xA000 : 0;
    
    // Apply mosaic only if enabled for this BG (BGCNT bit 6)
    bool mosaicEnable = (bgcnt & (1 << 6)) != 0;
    int mH = mosaicEnable ? bgMosaicH : 1;
    (void)bgMosaicV; // Vertical mosaic handled via internal affine ref point tracking
    
    // Affine parameters: PA (dx per pixel), PC (dy per pixel)
    s16 pa = (s16)m_memory->readIO16(IO::BG2PA);
    s16 pc = (s16)m_memory->readIO16(IO::BG2PA + 4);
    
    // Internal reference points (updated per scanline by PB/PD in enterHBlank)
    s32 refX = m_bg2RefX;
    s32 refY = m_bg2RefY;
    
    for (int x = 0; x < SCREEN_WIDTH; x++) {
        if (!(windowFlags[x] & (1 << 2))) continue; // BG2 window check
        int effectiveX = x - (x % mH);
        s32 texX = (refX + pa * effectiveX) >> 8;
        s32 texY = (refY + pc * effectiveX) >> 8;
        
        // Bitmap modes do not wrap — clip to source bounds
        if (texX < 0 || texX >= 240 || texY < 0 || texY >= 160) continue;
        
        u8 colorIndex = vram[base + texY * 240 + texX];
        if (colorIndex == 0) continue;
        
        u16 color555 = *reinterpret_cast<u16*>(&palette[colorIndex * 2]);
        placePixel(top, bot, x, color555, LAYER_BG2, priority);
    }
}

void PPU::renderBitmapMode5([[maybe_unused]] int y, u16 dispcnt, u8* vram,
                            ScanPixel* top, ScanPixel* bot, const u8* windowFlags,
                            int bgMosaicH, int bgMosaicV) {
    u16 bgcnt = m_memory->readIO16(IO::BG2CNT);
    int priority = bgcnt & 3;
    u32 base = (dispcnt & (1 << 4)) ? 0xA000 : 0;
    
    // Apply mosaic only if enabled for this BG (BGCNT bit 6)
    bool mosaicEnable = (bgcnt & (1 << 6)) != 0;
    int mH = mosaicEnable ? bgMosaicH : 1;
    (void)bgMosaicV; // Vertical mosaic handled via internal affine ref point tracking
    
    // Affine parameters: PA (dx per pixel), PC (dy per pixel)
    s16 pa = (s16)m_memory->readIO16(IO::BG2PA);
    s16 pc = (s16)m_memory->readIO16(IO::BG2PA + 4);
    
    // Internal reference points (updated per scanline by PB/PD in enterHBlank)
    s32 refX = m_bg2RefX;
    s32 refY = m_bg2RefY;
    
    for (int x = 0; x < SCREEN_WIDTH; x++) {
        if (!(windowFlags[x] & (1 << 2))) continue; // BG2 window check
        int effectiveX = x - (x % mH);
        s32 texX = (refX + pa * effectiveX) >> 8;
        s32 texY = (refY + pc * effectiveX) >> 8;
        
        // Bitmap modes do not wrap — clip to source bounds (160x128)
        if (texX < 0 || texX >= 160 || texY < 0 || texY >= 128) continue;
        
        u32 addr = base + (texY * 160 + texX) * 2;
        u16 color555 = *reinterpret_cast<u16*>(&vram[addr]);
        placePixel(top, bot, x, color555, LAYER_BG2, priority);
    }
}

void PPU::renderSprites(int y, u16 dispcnt, u8* palette, u8* vram, u8* oam,
                        ScanPixel* top, ScanPixel* bot, bool* objSemiTransparent,
                        const u8* windowFlags, int objMosaicH, int objMosaicV) {
    bool objMapping1D = (dispcnt & (1 << 6)) != 0;
    int mode = dispcnt & 7;
    
    // OBJ VRAM starts at 0x10000
    u8* objVram = &vram[0x10000];
    u8* objPalette = &palette[0x200]; // OBJ palette at 0x200
    
    // OBJ mosaic vertical counter (screen-based)
    int mosaicCounterY = y % objMosaicV;
    
    // Iterate sprites in reverse order (lower index = higher priority within same OBJ priority)
    for (int i = 127; i >= 0; i--) {
        u16 attr0 = *reinterpret_cast<u16*>(&oam[i * 8]);
        u16 attr1 = *reinterpret_cast<u16*>(&oam[i * 8 + 2]);
        u16 attr2 = *reinterpret_cast<u16*>(&oam[i * 8 + 4]);
        
        // Check if disabled
        bool isAffine = (attr0 & (1 << 8)) != 0;
        if (!isAffine && (attr0 & (1 << 9))) continue; // Disabled
        
        // OBJ mode: 0=normal, 1=semi-transparent, 2=OBJ window, 3=invalid
        int objMode = (attr0 >> 10) & 3;
        if (objMode == 3) continue; // Invalid mode
        // (handled in computeObjWindowMask)
        if (objMode == 2) continue;
        bool isSemiTransparent = (objMode == 1);
        bool hasMosaic = (attr0 & (1 << 12)) != 0;
        
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
        if (isAffine && (attr0 & (1 << 9))) {
            renderWidth *= 2;
            renderHeight *= 2;
        }
        
        // Check if this sprite is on the current scanline
        int localY = y - objY;
        if (localY < 0 || localY >= renderHeight) continue;
        
        // Apply OBJ V-mosaic: snap localY back by screen-based mosaic counter
        if (hasMosaic && objMosaicV > 1) {
            localY = std::max(0, localY - mosaicCounterY);
        }
        
        // Shared per-pixel OBJ emit: palette lookup, window check, compositing
        auto emitObjPixel = [&](int screenX, u8 colorIndex) {
            if (colorIndex == 0) return;
            u16 color555 = is8bpp
                ? *reinterpret_cast<u16*>(&objPalette[colorIndex * 2])
                : *reinterpret_cast<u16*>(&objPalette[(palNum * 16 + colorIndex) * 2]);
            if (!(windowFlags[screenX] & (1 << 4))) return;
            if (placePixel(top, bot, screenX, color555, LAYER_OBJ, priority)) {
                objSemiTransparent[screenX] = isSemiTransparent;
            }
        };
        
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
                
                // Apply OBJ H-mosaic: snap screen X to mosaic grid
                int lookupSX = sx;
                if (hasMosaic && objMosaicH > 1) {
                    int mosaicCounterX = screenX % objMosaicH;
                    lookupSX = std::max(0, sx - mosaicCounterX);
                }
                
                // Transform to texture coordinates
                int dx = lookupSX - halfW;
                int dy = localY - halfH;
                int texX = ((pa * dx + pb * dy) >> 8) + objWidth / 2;
                int texY = ((pc * dx + pd * dy) >> 8) + objHeight / 2;
                
                if (texX < 0 || texX >= objWidth || texY < 0 || texY >= objHeight) continue;
                
                u8 colorIndex = getObjPixel(tileIndex, texX, texY, objWidth, is8bpp, objMapping1D, objVram);
                emitObjPixel(screenX, colorIndex);
            }
        } else {
            // Normal sprite
            bool hFlip = (attr1 & (1 << 12)) != 0;
            bool vFlip = (attr1 & (1 << 13)) != 0;
            
            int texY = vFlip ? (objHeight - 1 - localY) : localY;
            
            for (int sx = 0; sx < objWidth; sx++) {
                int screenX = objX + sx;
                if (screenX < 0 || screenX >= SCREEN_WIDTH) continue;
                
                // Apply OBJ H-mosaic: snap screen X to mosaic grid
                int lookupSX = sx;
                if (hasMosaic && objMosaicH > 1) {
                    int mosaicCounterX = screenX % objMosaicH;
                    lookupSX = std::max(0, sx - mosaicCounterX);
                }
                
                int texX = hFlip ? (objWidth - 1 - lookupSX) : lookupSX;
                
                u8 colorIndex = getObjPixel(tileIndex, texX, texY, objWidth, is8bpp, objMapping1D, objVram);
                emitObjPixel(screenX, colorIndex);
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

void PPU::composeScanline(u32* line, ScanPixel* top, ScanPixel* bot,
                         bool* objSemiTransparent, const u8* windowFlags) {
    u16 bldcnt = m_memory->readIO16(IO::BLDCNT);
    u16 bldalpha = m_memory->readIO16(IO::BLDALPHA);
    u16 bldy = m_memory->readIO16(IO::BLDY);
    
    int blendMode = (bldcnt >> 6) & 3;
    int eva = bldalpha & 0x1F;
    int evb = (bldalpha >> 8) & 0x1F;
    int evy = bldy & 0x1F;
    
    // Clamp coefficients to 16
    if (eva > 16) eva = 16;
    if (evb > 16) evb = 16;
    if (evy > 16) evy = 16;
    
    for (int x = 0; x < SCREEN_WIDTH; x++) {
        u16 color = top[x].color;
        u8 layer1 = top[x].layer;
        u8 layer2 = bot[x].layer;
        
        // Check 1st and 2nd target flags in BLDCNT
        // Bits 0-5: 1st target (BG0, BG1, BG2, BG3, OBJ, BD)
        // Bits 8-13: 2nd target (BG0, BG1, BG2, BG3, OBJ, BD)
        bool is1stTarget = (bldcnt >> layer1) & 1;
        bool is2ndTarget = (bldcnt >> (8 + layer2)) & 1;
        
        // Check if color effects are enabled for this pixel's window region
        bool sfxEnabled = (windowFlags[x] >> 5) & 1;
        
        // Semi-transparent OBJ: forces alpha blending regardless of BLDCNT mode
        // The OBJ is always treated as 1st target; 2nd target check still applies
        // Semi-transparent OBJ always applies even when SFX is disabled by window
        if (objSemiTransparent[x] && is2ndTarget) {
            color = blendAlpha(color, bot[x].color, eva, evb);
        } else if (sfxEnabled) {
            switch (blendMode) {
                case 1: // Alpha blending
                    if (is1stTarget && is2ndTarget) {
                        color = blendAlpha(color, bot[x].color, eva, evb);
                    }
                    break;
                case 2: // Brightness increase (fade to white)
                    if (is1stTarget) {
                        int r = color & 0x1F;
                        int g = (color >> 5) & 0x1F;
                        int b = (color >> 10) & 0x1F;
                        r += (31 - r) * evy / 16;
                        g += (31 - g) * evy / 16;
                        b += (31 - b) * evy / 16;
                        color = (u16)(r | (g << 5) | (b << 10));
                    }
                    break;
                case 3: // Brightness decrease (fade to black)
                    if (is1stTarget) {
                        int r = color & 0x1F;
                        int g = (color >> 5) & 0x1F;
                        int b = (color >> 10) & 0x1F;
                        r -= r * evy / 16;
                        g -= g * evy / 16;
                        b -= b * evy / 16;
                        color = (u16)(r | (g << 5) | (b << 10));
                    }
                    break;
                default: // Mode 0: no blending
                    break;
            }
        }
        
        line[x] = rgb555ToARGB(color);
    }
}

void PPU::enterHBlank() {
    m_inHBlank = true;
    
    renderScanline();
    
    // Update affine reference points per scanline (sx += PB, sy += PD)
    s16 bg2pb = (s16)m_memory->readIO16(IO::BG2PA + 2);
    s16 bg2pd = (s16)m_memory->readIO16(IO::BG2PA + 6);
    s16 bg3pb = (s16)m_memory->readIO16(IO::BG3PA + 2);
    s16 bg3pd = (s16)m_memory->readIO16(IO::BG3PA + 6);
    m_bg2RefX += bg2pb;
    m_bg2RefY += bg2pd;
    m_bg3RefX += bg3pb;
    m_bg3RefY += bg3pd;
    
    u16 dispstat = m_memory->readDISPSTAT();
    dispstat |= DISPSTAT::HBLANK_FLAG;
    m_memory->writeDISPSTAT(dispstat);
    
    if (dispstat & DISPSTAT::HBLANK_IRQ) {
        m_memory->requestIRQ(IRQ::HBLANK);
    }
    
    // Trigger HBlank DMA (visible lines 0-159)
    m_dma->runHBlank();
    // Trigger video capture DMA (display start, vcounts 2-159 in visible range)
    if (m_vcount >= 2) m_dma->runDisplayStart(m_vcount);
}

void PPU::enterVBlank() {
    m_inVBlank = true;
    
    // Latch affine reference points at start of VBlank
    latchAffineRefPoints();
    
    u16 dispstat = m_memory->readDISPSTAT();
    dispstat |= DISPSTAT::VBLANK_FLAG;
    m_memory->writeDISPSTAT(dispstat);
    
    if (dispstat & DISPSTAT::VBLANK_IRQ) {
        m_memory->requestIRQ(IRQ::VBLANK);
    }
    
    // Trigger VBlank DMA
    m_dma->runVBlank();
}

void PPU::writeRegister(u32 offset, u16 value) {
    // When games write to affine reference point registers,
    // immediately latch the new value into the internal register.
    // The IO array is already updated by memory.cpp before this call.
    (void)value;
    
    auto signExtend28 = [](s32 val) -> s32 {
        return (val << 4) >> 4;
    };
    
    switch (offset) {
        case IO::BG2X:
        case IO::BG2X_H:
            m_bg2RefX = signExtend28((s32)m_memory->readIO32(IO::BG2X));
            break;
        case IO::BG2Y:
        case IO::BG2Y_H:
            m_bg2RefY = signExtend28((s32)m_memory->readIO32(IO::BG2Y));
            break;
        case IO::BG3X:
        case IO::BG3X_H:
            m_bg3RefX = signExtend28((s32)m_memory->readIO32(IO::BG3X));
            break;
        case IO::BG3Y:
        case IO::BG3Y_H:
            m_bg3RefY = signExtend28((s32)m_memory->readIO32(IO::BG3Y));
            break;
        default:
            break;
    }
}

void PPU::latchAffineRefPoints() {
    auto signExtend28 = [](s32 val) -> s32 {
        return (val << 4) >> 4;
    };
    
    m_bg2RefX = signExtend28((s32)m_memory->readIO32(IO::BG2X));
    m_bg2RefY = signExtend28((s32)m_memory->readIO32(IO::BG2Y));
    m_bg3RefX = signExtend28((s32)m_memory->readIO32(IO::BG3X));
    m_bg3RefY = signExtend28((s32)m_memory->readIO32(IO::BG3Y));
}

void PPU::saveState(Buffer* buf) {
    buffer_write(buf, m_framebuffer, sizeof(m_framebuffer));
    buffer_write(buf, &m_vcount, sizeof(m_vcount));
    buffer_write(buf, &m_cycles, sizeof(m_cycles));
    buffer_write(buf, &m_inHBlank, sizeof(m_inHBlank));
    buffer_write(buf, &m_inVBlank, sizeof(m_inVBlank));
    buffer_write(buf, &m_bg2RefX, sizeof(m_bg2RefX));
    buffer_write(buf, &m_bg2RefY, sizeof(m_bg2RefY));
    buffer_write(buf, &m_bg3RefX, sizeof(m_bg3RefX));
    buffer_write(buf, &m_bg3RefY, sizeof(m_bg3RefY));
}

void PPU::loadState(Buffer* buf) {
    buffer_read(buf, m_framebuffer, sizeof(m_framebuffer));
    buffer_read(buf, &m_vcount, sizeof(m_vcount));
    buffer_read(buf, &m_cycles, sizeof(m_cycles));
    buffer_read(buf, &m_inHBlank, sizeof(m_inHBlank));
    buffer_read(buf, &m_inVBlank, sizeof(m_inVBlank));
    buffer_read(buf, &m_bg2RefX, sizeof(m_bg2RefX));
    buffer_read(buf, &m_bg2RefY, sizeof(m_bg2RefY));
    buffer_read(buf, &m_bg3RefX, sizeof(m_bg3RefX));
    buffer_read(buf, &m_bg3RefY, sizeof(m_bg3RefY));
}

} // namespace gba

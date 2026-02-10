#include "ppu.h"
#include "cpu.h"
#include "cartridge.h"
#include <cstring>
#include <algorithm>

namespace nes {

// Standard NES palette (NTSC) - 64 colors in ARGB8888 format
const std::array<u32, 64> PPU::s_nesPalette = {{
    0xFF666666, 0xFF002A88, 0xFF1412A7, 0xFF3B00A4,  // 00-03
    0xFF5C007E, 0xFF6E0040, 0xFF6C0600, 0xFF561D00,  // 04-07
    0xFF333500, 0xFF0B4800, 0xFF005200, 0xFF004F08,  // 08-0B
    0xFF00404D, 0xFF000000, 0xFF000000, 0xFF000000,  // 0C-0F
    0xFFADADAD, 0xFF155FD9, 0xFF4240FF, 0xFF7527FE,  // 10-13
    0xFFA01ACC, 0xFFB71E7B, 0xFFB53120, 0xFF994E00,  // 14-17
    0xFF6B6D00, 0xFF388700, 0xFF0C9300, 0xFF008F32,  // 18-1B
    0xFF007C8D, 0xFF000000, 0xFF000000, 0xFF000000,  // 1C-1F
    0xFFFFFEFF, 0xFF64B0FF, 0xFF9290FF, 0xFFC676FF,  // 20-23
    0xFFF36AFF, 0xFFFE6ECC, 0xFFFE8170, 0xFFEA9E22,  // 24-27
    0xFFBCBE00, 0xFF88D800, 0xFF5CE430, 0xFF45E082,  // 28-2B
    0xFF48CDDE, 0xFF4F4F4F, 0xFF000000, 0xFF000000,  // 2C-2F
    0xFFFFFEFF, 0xFFC0DFFF, 0xFFD3D2FF, 0xFFE8C8FF,  // 30-33
    0xFFFBC2FF, 0xFFFEC4EA, 0xFFFECCC5, 0xFFF7D8A5,  // 34-37
    0xFFE4E594, 0xFFCFEF96, 0xFFBDF4AB, 0xFFB3F3CC,  // 38-3B
    0xFFB5EBF2, 0xFFB8B8B8, 0xFF000000, 0xFF000000   // 3C-3F
}};

PPU::PPU()
    : m_cpu(nullptr)
    , m_cartridge(nullptr)
    , m_videoDevice(nullptr)
    , m_cycle(0)
    , m_scanline(0)
    , m_oddFrame(false)
    , m_ppuCtrl(0)
    , m_ppuMask(0)
    , m_ppuStatus(0)
    , m_oamAddr(0)
    , m_vramAddr(0)
    , m_tempAddr(0)
    , m_fineX(0)
    , m_writeToggle(false)
    , m_dataBuffer(0)
    , m_fetchPhase(PPUFetchPhase::IDLE)
    , m_spriteCount(0)
    , m_sprite0OnScanline(false)
    , m_nmiOccurred(false)
    , m_nmiOutput(false)
    , m_openBus(0) {
    reset();
}

void PPU::reset() {
    m_cycle = 0;
    m_scanline = 0;
    m_oddFrame = false;
    
    m_ppuCtrl = 0;
    m_ppuMask = 0;
    m_ppuStatus = 0;
    m_oamAddr = 0;
    
    m_vramAddr = 0;
    m_tempAddr = 0;
    m_fineX = 0;
    m_writeToggle = false;
    
    m_dataBuffer = 0;
    
    m_fetchPhase = PPUFetchPhase::IDLE;
    
    m_spriteCount = 0;
    m_sprite0OnScanline = false;
    
    m_nmiOccurred = false;
    m_nmiOutput = false;
    
    m_openBus = 0;
    
    m_vram.fill(0);
    m_palette.fill(0);
    m_oam.fill(0);
    m_secondaryOam.fill(0xFF);
    m_framebuffer.fill(0xFF000000);
    m_bgLine.fill(0);
    m_sprLine.fill(0);
    m_sprBehindBg.fill(false);
    m_sprOamIndex.fill(0xFF);
    
    for (auto& sprite : m_spriteRenderData) {
        sprite = {};
    }
}

// ============================================================================
// Register Access
// ============================================================================

u8 PPU::readRegister(u16 address) {
    u8 result = m_openBus;
    
    switch (address & 0x2007) {
        case PPUCTRL:   // $2000 - Write only
            log_error("[PPU] Read from write-only register PPUCTRL ($2000)");
            break;
            
        case PPUMASK:   // $2001 - Write only
            log_error("[PPU] Read from write-only register PPUMASK ($2001)");
            break;
            
        case PPUSTATUS: // $2002
            // Return VBlank, Sprite 0 hit, Sprite overflow
            result = (m_ppuStatus & 0xE0) | (m_openBus & 0x1F);
            
            // Reading clears VBlank flag
            m_ppuStatus &= ~PPUSTATUS_VBLANK;
            m_nmiOccurred = false;
            
            // Reset write toggle
            m_writeToggle = false;
            break;
            
        case OAMADDR:   // $2003 - Write only
            log_error("[PPU] Read from write-only register OAMADDR ($2003)");
            break;
            
        case OAMDATA:   // $2004
            result = m_oam[m_oamAddr];
            
            // Bits 2-4 of sprite attributes are unimplemented and read back as 0
            if ((m_oamAddr & 0x03) == 2) {
                result &= 0xE3;
            }
            break;
            
        case PPUSCROLL: // $2005 - Write only
            log_error("[PPU] Read from write-only register PPUSCROLL ($2005)");
            break;
            
        case PPUADDR:   // $2006 - Write only
            log_error("[PPU] Read from write-only register PPUADDR ($2006)");
            break;
            
        case PPUDATA:   // $2007
            result = m_dataBuffer;
            m_dataBuffer = ppuRead(m_vramAddr);
            
            // Palette reads are not buffered
            if (m_vramAddr >= 0x3F00) {
                result = m_dataBuffer;
                // Buffer gets the mirrored nametable data "underneath" the palette
                m_dataBuffer = ppuRead(m_vramAddr - 0x1000);
            }
            
            // Increment VRAM address
            m_vramAddr += (m_ppuCtrl & PPUCTRL_INCREMENT) ? 32 : 1;
            m_vramAddr &= 0x7FFF;
            break;
    }
    
    m_openBus = result;
    return result;
}

void PPU::writeRegister(u16 address, u8 value) {
    m_openBus = value;
    
    switch (address & 0x2007) {
        case PPUCTRL:   // $2000
        {
            bool wasNmiEnabled = m_ppuCtrl & PPUCTRL_NMI_ENABLE;
            m_ppuCtrl = value;
            m_nmiOutput = (value & PPUCTRL_NMI_ENABLE) != 0;
            
            // Update temp address nametable select
            m_tempAddr = (m_tempAddr & 0xF3FF) | ((static_cast<u16>(value) & 0x03) << 10);
            
            // If NMI was just enabled and we're in VBlank, trigger NMI
            if (!wasNmiEnabled && m_nmiOutput && m_nmiOccurred) {
                m_cpu->nmi();
            }
            break;
        }
        
        case PPUMASK:   // $2001
            m_ppuMask = value;
            break;
            
        case PPUSTATUS: // $2002 - Read only
            log_error("[PPU] Write to read-only register PPUSTATUS ($2002) = $%x", value);
            break;
            
        case OAMADDR:   // $2003
            m_oamAddr = value;
            break;
            
        case OAMDATA:   // $2004
            // Writes during rendering increment OAMADDR but don't write data
            if (!isRenderingEnabled() || 
                (m_scanline >= VISIBLE_SCANLINES && m_scanline != PRE_RENDER_SCANLINE)) {
                m_oam[m_oamAddr] = value;
            }
            m_oamAddr++;
            break;
            
        case PPUSCROLL: // $2005
            if (!m_writeToggle) {
                // First write - horizontal scroll
                m_fineX = value & 0x07;
                m_tempAddr = (m_tempAddr & 0xFFE0) | (value >> 3);
            } else {
                // Second write - vertical scroll
                m_tempAddr = (m_tempAddr & 0x8C1F) |
                            ((static_cast<u16>(value) & 0x07) << 12) |
                            ((static_cast<u16>(value) & 0xF8) << 2);
            }
            m_writeToggle = !m_writeToggle;
            break;
            
        case PPUADDR:   // $2006
            if (!m_writeToggle) {
                // First write - high byte
                m_tempAddr = (m_tempAddr & 0x00FF) | ((static_cast<u16>(value) & 0x3F) << 8);
            } else {
                // Second write - low byte
                m_tempAddr = (m_tempAddr & 0xFF00) | value;
                m_vramAddr = m_tempAddr;
            }
            m_writeToggle = !m_writeToggle;
            break;
            
        case PPUDATA:   // $2007
            ppuWrite(m_vramAddr, value);
            m_vramAddr += (m_ppuCtrl & PPUCTRL_INCREMENT) ? 32 : 1;
            m_vramAddr &= 0x7FFF;
            break;
    }
}

// ============================================================================
// PPU Memory Access
// ============================================================================

u8 PPU::ppuRead(u16 address) {
    address &= 0x3FFF;  // PPU address space is 14 bits
    
    if (address < 0x2000) {
        // Pattern tables - read from cartridge CHR
        return m_cartridge ? m_cartridge->readCHR(address) : 0;
    }
    else if (address < 0x3F00) {
        // Nametables
        u8 value;
        if (m_cartridge && m_cartridge->readNametable(address, value)) {
            return value;
        }
        return m_vram[mirrorNametableAddress(address)];
    }
    else {
        // Palette
        u16 paletteAddr = address & 0x1F;
        
        // Addresses $3F10, $3F14, $3F18, $3F1C mirror $3F00, $3F04, $3F08, $3F0C
        if ((paletteAddr & 0x13) == 0x10) {
            paletteAddr &= 0x0F;
        }
        
        u8 result = m_palette[paletteAddr];
        
        // Apply grayscale if enabled
        if (m_ppuMask & PPUMASK_GRAYSCALE) {
            result &= 0x30;
        }
        
        return result;
    }
}

void PPU::ppuWrite(u16 address, u8 value) {
    address &= 0x3FFF;
    
    if (address < 0x2000) {
        if (m_cartridge) {
            m_cartridge->writeCHR(address, value);
        }
    }
    else if (address < 0x3F00) {
        if (m_cartridge && m_cartridge->writeNametable(address, value)) {
            return;
        }
        m_vram[mirrorNametableAddress(address)] = value;
    }
    else {
        u16 paletteAddr = address & 0x1F;
        if ((paletteAddr & 0x13) == 0x10) {
            paletteAddr &= 0x0F;
        }
        m_palette[paletteAddr] = value;
    }
}

u16 PPU::mirrorNametableAddress(u16 address) const {
    address = (address - 0x2000) & 0x0FFF;
    
    MirrorMode mirror = m_cartridge ? m_cartridge->getMirrorMode() : MirrorMode::HORIZONTAL;
    
    switch (mirror) {
        case MirrorMode::HORIZONTAL:
            if (address < 0x0800) {
                return address & 0x03FF;
            } else {
                return 0x0400 + (address & 0x03FF);
            }
            
        case MirrorMode::VERTICAL:
            return address & 0x07FF;
            
        case MirrorMode::SINGLE_SCREEN_A:
            return address & 0x03FF;
            
        case MirrorMode::SINGLE_SCREEN_B:
            return 0x0400 + (address & 0x03FF);
            
        case MirrorMode::FOUR_SCREEN:
        default:
            return address & 0x07FF;
    }
}

// ============================================================================
// Rendering State Helpers
// ============================================================================

bool PPU::isBackgroundEnabled() const {
    return (m_ppuMask & PPUMASK_SHOW_BG) != 0;
}

bool PPU::isSpriteEnabled() const {
    return (m_ppuMask & PPUMASK_SHOW_SPR) != 0;
}

u8 PPU::getSpriteHeight() const {
    return (m_ppuCtrl & PPUCTRL_SPRITE_SIZE) ? 16 : 8;
}

// ============================================================================
// Address Manipulation (Loopy)
// ============================================================================

void PPU::incrementX() {
    if (!isRenderingEnabled()) return;
    
    if ((m_vramAddr & 0x001F) == 31) {
        m_vramAddr &= ~0x001F;
        m_vramAddr ^= 0x0400;
    } else {
        m_vramAddr++;
    }
}

void PPU::incrementY() {
    if (!isRenderingEnabled()) return;
    
    if ((m_vramAddr & 0x7000) != 0x7000) {
        m_vramAddr += 0x1000;
    } else {
        m_vramAddr &= ~0x7000;
        
        u16 coarseY = (m_vramAddr & 0x03E0) >> 5;
        if (coarseY == 29) {
            coarseY = 0;
            m_vramAddr ^= 0x0800;
        } else if (coarseY == 31) {
            coarseY = 0;
        } else {
            coarseY++;
        }
        
        m_vramAddr = (m_vramAddr & ~0x03E0) | (coarseY << 5);
    }
}

void PPU::transferX() {
    if (!isRenderingEnabled()) return;
    m_vramAddr = (m_vramAddr & 0xFBE0) | (m_tempAddr & 0x041F);
}

void PPU::transferY() {
    if (!isRenderingEnabled()) return;
    m_vramAddr = (m_vramAddr & 0x841F) | (m_tempAddr & 0x7BE0);
}

// ============================================================================
// Scanline-Based Background Rendering
// ============================================================================

void PPU::renderBackgroundLine() {
    m_bgLine.fill(0);
    
    if (!isBackgroundEnabled()) return;
    
    m_fetchPhase = PPUFetchPhase::BACKGROUND;
    
    u16 savedAddr = m_vramAddr;
    
    // We need to render 256 pixels (32 tiles + potentially 1 extra for fine X scroll)
    // Fine X determines the starting bit offset within the first tile
    
    u16 fineY = (m_vramAddr >> 12) & 0x07;
    u16 bgTableBase = (m_ppuCtrl & PPUCTRL_BG_TABLE) ? 0x1000 : 0x0000;
    
    // Render 33 tiles (32 visible + 1 partial for fine X scroll)
    for (int tile = 0; tile < 33; tile++) {
        // Fetch nametable byte
        u8 tileId = ppuRead(0x2000 | (m_vramAddr & 0x0FFF));
        
        // Fetch attribute byte
        u16 attrAddr = 0x23C0 |
                      (m_vramAddr & 0x0C00) |
                      ((m_vramAddr >> 4) & 0x38) |
                      ((m_vramAddr >> 2) & 0x07);
        u8 attrByte = ppuRead(attrAddr);
        
        // Select the 2-bit palette for this tile
        if (m_vramAddr & 0x0002) attrByte >>= 2;
        if (m_vramAddr & 0x0040) attrByte >>= 4;
        u8 palette = attrByte & 0x03;
        
        // Fetch pattern table bytes
        u16 patternAddr = bgTableBase + (static_cast<u16>(tileId) << 4) + fineY;
        u8 patternLow = ppuRead(patternAddr);
        u8 patternHigh = ppuRead(patternAddr + 8);
        
        // Render 8 pixels from this tile
        for (int bit = 0; bit < 8; bit++) {
            int pixelX = tile * 8 + bit - m_fineX;
            
            if (pixelX < 0 || pixelX >= 256) continue;
            
            // Check left 8 pixel masking
            if (pixelX < 8 && !(m_ppuMask & PPUMASK_SHOW_BG_LEFT)) {
                m_bgLine[pixelX] = 0;
                continue;
            }
            
            u8 bitSelect = 7 - bit;
            u8 pixel = 0;
            if (patternLow & (1 << bitSelect)) pixel |= 0x01;
            if (patternHigh & (1 << bitSelect)) pixel |= 0x02;
            
            if (pixel == 0) {
                m_bgLine[pixelX] = 0;  // Transparent
            } else {
                m_bgLine[pixelX] = (palette << 2) | pixel;
            }
        }
        
        // Increment horizontal scroll (coarse X)
        if ((m_vramAddr & 0x001F) == 31) {
            m_vramAddr &= ~0x001F;
            m_vramAddr ^= 0x0400;
        } else {
            m_vramAddr++;
        }
    }
    
    // Restore coarse X and nametable X from saved state (will be set properly by transferX)
    m_vramAddr = savedAddr;
    
    m_fetchPhase = PPUFetchPhase::IDLE;
}

// ============================================================================
// Scanline-Based Sprite Evaluation & Rendering
// ============================================================================

void PPU::evaluateSpritesForScanline() {
    m_spriteCount = 0;
    m_sprite0OnScanline = false;
    m_secondaryOam.fill(0xFF);
    
    u8 spriteHeight = getSpriteHeight();
    
    for (u8 i = 0; i < 64 && m_spriteCount < 8; i++) {
        const OAMEntry* sprite = reinterpret_cast<const OAMEntry*>(&m_oam[i * 4]);
        
        s16 diff = static_cast<s16>(m_scanline) - static_cast<s16>(sprite->y);
        
        if (diff >= 0 && diff < spriteHeight) {
            m_secondaryOam[m_spriteCount * 4 + 0] = sprite->y;
            m_secondaryOam[m_spriteCount * 4 + 1] = sprite->tileIndex;
            m_secondaryOam[m_spriteCount * 4 + 2] = sprite->attributes;
            m_secondaryOam[m_spriteCount * 4 + 3] = sprite->x;
            
            if (i == 0) {
                m_sprite0OnScanline = true;
            }
            
            m_spriteRenderData[m_spriteCount].oamIndex = i;
            m_spriteCount++;
        }
    }
    
    // Check for sprite overflow
    if (m_spriteCount == 8) {
        for (u8 i = 0; i < 64; i++) {
            const OAMEntry* sprite = reinterpret_cast<const OAMEntry*>(&m_oam[i * 4]);
            s16 diff = static_cast<s16>(m_scanline) - static_cast<s16>(sprite->y);
            
            if (diff >= 0 && diff < spriteHeight) {
                m_ppuStatus |= PPUSTATUS_SPRITE_OVERFLOW;
                break;
            }
        }
    }
}

void PPU::fetchSpritePattern(u8 spriteIndex) {
    u8 y = m_secondaryOam[spriteIndex * 4 + 0];
    u8 tileIndex = m_secondaryOam[spriteIndex * 4 + 1];
    u8 attributes = m_secondaryOam[spriteIndex * 4 + 2];
    u8 x = m_secondaryOam[spriteIndex * 4 + 3];
    
    u8 spriteHeight = getSpriteHeight();
    
    s16 row = static_cast<s16>(m_scanline) - static_cast<s16>(y);
    
    if (attributes & OAM_FLIP_V) {
        row = spriteHeight - 1 - row;
    }
    
    u16 patternAddr;
    
    if (spriteHeight == 16) {
        u16 table = (tileIndex & 0x01) ? 0x1000 : 0x0000;
        tileIndex &= 0xFE;
        
        if (row >= 8) {
            tileIndex++;
            row -= 8;
        }
        
        patternAddr = table + (static_cast<u16>(tileIndex) << 4) + row;
    } else {
        u16 table = (m_ppuCtrl & PPUCTRL_SPRITE_TABLE) ? 0x1000 : 0x0000;
        patternAddr = table + (static_cast<u16>(tileIndex) << 4) + row;
    }
    
    u8 patternLow = ppuRead(patternAddr);
    u8 patternHigh = ppuRead(patternAddr + 8);
    
    if (attributes & OAM_FLIP_H) {
        auto reverseBits = [](u8 b) -> u8 {
            b = ((b & 0xF0) >> 4) | ((b & 0x0F) << 4);
            b = ((b & 0xCC) >> 2) | ((b & 0x33) << 2);
            b = ((b & 0xAA) >> 1) | ((b & 0x55) << 1);
            return b;
        };
        patternLow = reverseBits(patternLow);
        patternHigh = reverseBits(patternHigh);
    }
    
    m_spriteRenderData[spriteIndex].patternLow = patternLow;
    m_spriteRenderData[spriteIndex].patternHigh = patternHigh;
    m_spriteRenderData[spriteIndex].attributes = attributes;
    m_spriteRenderData[spriteIndex].x = x;
}

void PPU::renderSpriteLine() {
    m_sprLine.fill(0);
    m_sprBehindBg.fill(false);
    m_sprOamIndex.fill(0xFF);
    
    if (!isSpriteEnabled()) return;
    
    m_fetchPhase = PPUFetchPhase::SPRITE;
    
    // Evaluate sprites for this scanline
    evaluateSpritesForScanline();
    
    // Fetch pattern data for sprites
    for (u8 i = 0; i < m_spriteCount; i++) {
        fetchSpritePattern(i);
    }
    
    // Render sprites into line buffer (reverse order for priority - lower index wins)
    for (int i = m_spriteCount - 1; i >= 0; i--) {
        const auto& spr = m_spriteRenderData[i];
        u8 palette = spr.attributes & OAM_PALETTE;
        bool behindBg = (spr.attributes & OAM_PRIORITY) != 0;
        
        for (int bit = 0; bit < 8; bit++) {
            int pixelX = static_cast<int>(spr.x) + bit;
            if (pixelX >= 256) break;
            if (pixelX < 0) continue;
            
            // Check left 8 pixel masking
            if (pixelX < 8 && !(m_ppuMask & PPUMASK_SHOW_SPR_LEFT)) continue;
            
            u8 bitSelect = 7 - bit;
            u8 pixel = 0;
            if (spr.patternLow & (1 << bitSelect)) pixel |= 0x01;
            if (spr.patternHigh & (1 << bitSelect)) pixel |= 0x02;
            
            if (pixel == 0) continue;  // Transparent
            
            // Lower-indexed sprites have higher priority (we render back to front)
            m_sprLine[pixelX] = 0x10 | (palette << 2) | pixel;
            m_sprBehindBg[pixelX] = behindBg;
            m_sprOamIndex[pixelX] = spr.oamIndex;
        }
    }
    
    m_fetchPhase = PPUFetchPhase::IDLE;
}

// ============================================================================
// Composite Background + Sprites and Output
// ============================================================================

void PPU::compositeAndOutputLine() {
    u32* fbRow = &m_framebuffer[m_scanline * SCREEN_WIDTH];
    
    bool bgEnabled = isBackgroundEnabled();
    bool sprEnabled = isSpriteEnabled();
    
    for (int x = 0; x < 256; x++) {
        u8 bgPixel = m_bgLine[x];
        u8 sprPixel = m_sprLine[x];
        
        bool bgOpaque = (bgPixel & 0x03) != 0;
        bool sprOpaque = (sprPixel & 0x03) != 0;
        
        // Sprite 0 hit detection
        if (m_sprite0OnScanline && sprOpaque && bgOpaque &&
            m_sprOamIndex[x] == 0 && x != 255 && bgEnabled && sprEnabled) {
            if (x >= 8 || 
                ((m_ppuMask & PPUMASK_SHOW_BG_LEFT) && (m_ppuMask & PPUMASK_SHOW_SPR_LEFT))) {
                m_ppuStatus |= PPUSTATUS_SPRITE0_HIT;
            }
        }
        
        // Priority multiplexer
        u8 finalPixel = 0;
        
        if (!bgOpaque && !sprOpaque) {
            finalPixel = 0;
        } else if (bgOpaque && !sprOpaque) {
            finalPixel = bgPixel;
        } else if (!bgOpaque && sprOpaque) {
            finalPixel = sprPixel;
        } else {
            // Both opaque - use priority
            if (m_sprBehindBg[x]) {
                finalPixel = bgPixel;
            } else {
                finalPixel = sprPixel;
            }
        }
        
        // Look up color in palette
        u8 colorIndex = ppuRead(0x3F00 + finalPixel) & 0x3F;
        u32 color = s_nesPalette[colorIndex];
        
        // Apply color emphasis
        if (m_ppuMask & (PPUMASK_EMPHASIZE_R | PPUMASK_EMPHASIZE_G | PPUMASK_EMPHASIZE_B)) {
            u8 r = (color >> 16) & 0xFF;
            u8 g = (color >> 8) & 0xFF;
            u8 b = color & 0xFF;
            
            if (m_ppuMask & PPUMASK_EMPHASIZE_R) {
                r = std::min(255, static_cast<int>(r * 1.1));
                g = static_cast<u8>(g * 0.9);
                b = static_cast<u8>(b * 0.9);
            }
            if (m_ppuMask & PPUMASK_EMPHASIZE_G) {
                r = static_cast<u8>(r * 0.9);
                g = std::min(255, static_cast<int>(g * 1.1));
                b = static_cast<u8>(b * 0.9);
            }
            if (m_ppuMask & PPUMASK_EMPHASIZE_B) {
                r = static_cast<u8>(r * 0.9);
                g = static_cast<u8>(g * 0.9);
                b = std::min(255, static_cast<int>(b * 1.1));
            }
            
            color = 0xFF000000 | (r << 16) | (g << 8) | b;
        }
        
        fbRow[x] = color;
    }
}

// ============================================================================
// Visible Scanline Rendering (all-in-one)
// ============================================================================

void PPU::renderVisibleScanline() {
    // Set cycle to beginning of visible area for mapper compatibility
    m_cycle = 1;
    
    // Render background tiles for this scanline
    renderBackgroundLine();
    
    // Set cycle to sprite fetch range for mapper compatibility
    m_cycle = 257;
    
    // Render sprites for this scanline
    renderSpriteLine();
    
    // Composite and output to framebuffer
    compositeAndOutputLine();
    
    // Update VRAM address: increment Y at end of visible scanline
    incrementY();
    
    // Transfer horizontal bits from temp to VRAM address
    transferX();
    
    // Set cycle to mapper IRQ clock point
    // MMC3 clocks on A12 rising edge around cycle 260
    // MMC5 clocks at cycle 3 (attribute read)
    u16 targetCycle = m_cartridge->getMapperNumber() == 5 ? 3 : 260;
    m_cycle = targetCycle;
    
    if (isRenderingEnabled() && m_cartridge) {
        m_cartridge->scanlineCounter();
        if (m_cartridge->irqState()) {
            if (m_cpu) {
                m_cpu->irq();
            }
            m_cartridge->irqClear();
        }
    }
    
    // End of scanline
    m_cycle = 340;
}

// ============================================================================
// Main Scanline Step Function
// ============================================================================

void PPU::stepScanline() {
    // Visible scanlines (0-239)
    if (m_scanline < VISIBLE_SCANLINES) {
        if (isRenderingEnabled()) {
            renderVisibleScanline();
        } else {
            // Rendering disabled - fill with background color
            u8 colorIndex = ppuRead(0x3F00) & 0x3F;
            u32 color = s_nesPalette[colorIndex];
            u32* fbRow = &m_framebuffer[m_scanline * SCREEN_WIDTH];
            for (int x = 0; x < 256; x++) {
                fbRow[x] = color;
            }
            m_cycle = 340;
        }
    }
    // Post-render scanline (240) - idle
    else if (m_scanline == 240) {
        m_cycle = 340;
        // Nothing to do
    }
    // VBlank start (241)
    else if (m_scanline == 241) {
        m_cycle = 1;
        
        // Set VBlank flag
        m_ppuStatus |= PPUSTATUS_VBLANK;
        m_nmiOccurred = true;
        
        // Generate NMI if enabled
        if (m_nmiOutput) {
            m_cpu->nmi();
        }
        
        // Render frame
        if (m_videoDevice) {
            m_videoDevice->render(m_framebuffer.data());
        }
        
        m_cycle = 340;
    }
    // VBlank scanlines (242-260) - idle
    else if (m_scanline > 241 && m_scanline < PRE_RENDER_SCANLINE) {
        m_cycle = 340;
        // Nothing to do
    }
    // Pre-render scanline (261)
    else if (m_scanline == PRE_RENDER_SCANLINE) {
        m_cycle = 1;
        
        // Clear VBlank, sprite 0 hit, and overflow
        m_ppuStatus &= ~(PPUSTATUS_VBLANK | PPUSTATUS_SPRITE0_HIT | PPUSTATUS_SPRITE_OVERFLOW);
        m_nmiOccurred = false;
        
        if (isRenderingEnabled()) {
            // Perform background tile fetches for prefetch (updates VRAM addr)
            m_fetchPhase = PPUFetchPhase::BACKGROUND;
            
            // Increment Y (pre-render scanline behaves like visible for address ops)
            incrementY();
            
            // Transfer horizontal position
            transferX();
            
            // Transfer vertical position (cycles 280-304)
            transferY();
            
            m_fetchPhase = PPUFetchPhase::IDLE;
            
            // Clock scanline counter for pre-render scanline too (MMC3 needs this)
            u16 targetCycle = m_cartridge->getMapperNumber() == 5 ? 3 : 260;
            m_cycle = targetCycle;
            
            if (m_cartridge) {
                m_cartridge->scanlineCounter();
                if (m_cartridge->irqState()) {
                    if (m_cpu) {
                        m_cpu->irq();
                    }
                    m_cartridge->irqClear();
                }
            }
            
            // Evaluate sprites for first visible scanline
            evaluateSpritesForScanline();
        }
        
        m_cycle = 340;
    }
    
    // Advance to next scanline
    m_scanline++;
    
    if (m_scanline > PRE_RENDER_SCANLINE) {
        m_scanline = 0;
        m_oddFrame = !m_oddFrame;
    }
}

// ============================================================================
// Save/Load State
// ============================================================================

void PPU::saveState(Buffer* buf) {
    // Timing
    buffer_write(buf, &m_cycle, sizeof(m_cycle));
    buffer_write(buf, &m_scanline, sizeof(m_scanline));
    buffer_write(buf, &m_oddFrame, sizeof(m_oddFrame));
    
    // Registers
    buffer_write(buf, &m_ppuCtrl, sizeof(m_ppuCtrl));
    buffer_write(buf, &m_ppuMask, sizeof(m_ppuMask));
    buffer_write(buf, &m_ppuStatus, sizeof(m_ppuStatus));
    buffer_write(buf, &m_oamAddr, sizeof(m_oamAddr));
    
    // Internal registers
    buffer_write(buf, &m_vramAddr, sizeof(m_vramAddr));
    buffer_write(buf, &m_tempAddr, sizeof(m_tempAddr));
    buffer_write(buf, &m_fineX, sizeof(m_fineX));
    buffer_write(buf, &m_writeToggle, sizeof(m_writeToggle));
    buffer_write(buf, &m_dataBuffer, sizeof(m_dataBuffer));
    
    // Background shift registers (write zeros for compatibility - not used in scanline mode)
    u16 zero16 = 0;
    buffer_write(buf, &zero16, sizeof(zero16));  // m_bgShiftPatternLow
    buffer_write(buf, &zero16, sizeof(zero16));  // m_bgShiftPatternHigh
    buffer_write(buf, &zero16, sizeof(zero16));  // m_bgShiftAttrLow
    buffer_write(buf, &zero16, sizeof(zero16));  // m_bgShiftAttrHigh
    
    // Background latches (write zeros for compatibility)
    u8 zero8 = 0;
    buffer_write(buf, &zero8, sizeof(zero8));    // m_bgNextTileId
    buffer_write(buf, &zero8, sizeof(zero8));    // m_bgNextTileAttr
    buffer_write(buf, &zero8, sizeof(zero8));    // m_bgNextTileLow
    buffer_write(buf, &zero8, sizeof(zero8));    // m_bgNextTileHigh
    
    // Memory
    buffer_write(buf, m_vram.data(), m_vram.size());
    buffer_write(buf, m_palette.data(), m_palette.size());
    buffer_write(buf, m_oam.data(), m_oam.size());
    buffer_write(buf, m_secondaryOam.data(), m_secondaryOam.size());
    
    // Sprite state
    buffer_write(buf, &m_spriteCount, sizeof(m_spriteCount));
    buffer_write(buf, &m_sprite0OnScanline, sizeof(m_sprite0OnScanline));
    bool dummy = false;
    buffer_write(buf, &dummy, sizeof(dummy));  // m_sprite0HitPossible (compat)
    
    // NMI state
    buffer_write(buf, &m_nmiOccurred, sizeof(m_nmiOccurred));
    buffer_write(buf, &m_nmiOutput, sizeof(m_nmiOutput));
    
    buffer_write(buf, &m_openBus, sizeof(m_openBus));
}

void PPU::loadState(Buffer* buf) {
    // Timing
    buffer_read(buf, &m_cycle, sizeof(m_cycle));
    buffer_read(buf, &m_scanline, sizeof(m_scanline));
    buffer_read(buf, &m_oddFrame, sizeof(m_oddFrame));
    
    // Registers
    buffer_read(buf, &m_ppuCtrl, sizeof(m_ppuCtrl));
    buffer_read(buf, &m_ppuMask, sizeof(m_ppuMask));
    buffer_read(buf, &m_ppuStatus, sizeof(m_ppuStatus));
    buffer_read(buf, &m_oamAddr, sizeof(m_oamAddr));
    
    // Internal registers
    buffer_read(buf, &m_vramAddr, sizeof(m_vramAddr));
    buffer_read(buf, &m_tempAddr, sizeof(m_tempAddr));
    buffer_read(buf, &m_fineX, sizeof(m_fineX));
    buffer_read(buf, &m_writeToggle, sizeof(m_writeToggle));
    buffer_read(buf, &m_dataBuffer, sizeof(m_dataBuffer));
    
    // Background shift registers (read and discard - not used in scanline mode)
    u16 dummy16;
    buffer_read(buf, &dummy16, sizeof(dummy16));
    buffer_read(buf, &dummy16, sizeof(dummy16));
    buffer_read(buf, &dummy16, sizeof(dummy16));
    buffer_read(buf, &dummy16, sizeof(dummy16));
    
    // Background latches (read and discard)
    u8 dummy8;
    buffer_read(buf, &dummy8, sizeof(dummy8));
    buffer_read(buf, &dummy8, sizeof(dummy8));
    buffer_read(buf, &dummy8, sizeof(dummy8));
    buffer_read(buf, &dummy8, sizeof(dummy8));
    
    // Memory
    buffer_read(buf, m_vram.data(), m_vram.size());
    buffer_read(buf, m_palette.data(), m_palette.size());
    buffer_read(buf, m_oam.data(), m_oam.size());
    buffer_read(buf, m_secondaryOam.data(), m_secondaryOam.size());
    
    // Sprite state
    buffer_read(buf, &m_spriteCount, sizeof(m_spriteCount));
    buffer_read(buf, &m_sprite0OnScanline, sizeof(m_sprite0OnScanline));
    bool dummy;
    buffer_read(buf, &dummy, sizeof(dummy));  // m_sprite0HitPossible (compat)
    
    // NMI state
    buffer_read(buf, &m_nmiOccurred, sizeof(m_nmiOccurred));
    buffer_read(buf, &m_nmiOutput, sizeof(m_nmiOutput));
    
    buffer_read(buf, &m_openBus, sizeof(m_openBus));
}

} // namespace nes

#include "ppu.h"
#include "cpu.h"
#include "cartridge.h"
#include <cstring>
#include <algorithm>
#include <iostream>

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
    , m_frameComplete(false)
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
    , m_bgShiftPatternLow(0)
    , m_bgShiftPatternHigh(0)
    , m_bgShiftAttrLow(0)
    , m_bgShiftAttrHigh(0)
    , m_bgNextTileId(0)
    , m_bgNextTileAttr(0)
    , m_bgNextTileLow(0)
    , m_bgNextTileHigh(0)
    , m_spriteCount(0)
    , m_sprite0OnScanline(false)
    , m_sprite0HitPossible(false)
    , m_spriteEvalN(0)
    , m_spriteEvalM(0)
    , m_secondaryOamAddr(0)
    , m_spriteEvalComplete(false)
    , m_nmiOccurred(false)
    , m_nmiOutput(false)
    , m_openBus(0) {
    reset();
}

void PPU::reset() {
    m_cycle = 0;
    m_scanline = 0;
    m_frameComplete = false;
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
    
    m_bgShiftPatternLow = 0;
    m_bgShiftPatternHigh = 0;
    m_bgShiftAttrLow = 0;
    m_bgShiftAttrHigh = 0;
    
    m_bgNextTileId = 0;
    m_bgNextTileAttr = 0;
    m_bgNextTileLow = 0;
    m_bgNextTileHigh = 0;
    
    m_spriteCount = 0;
    m_sprite0OnScanline = false;
    m_sprite0HitPossible = false;
    m_spriteEvalN = 0;
    m_spriteEvalM = 0;
    m_secondaryOamAddr = 0;
    m_spriteEvalComplete = false;
    
    m_nmiOccurred = false;
    m_nmiOutput = false;
    
    m_openBus = 0;
    
    m_vram.fill(0);
    m_palette.fill(0);
    m_oam.fill(0);
    m_secondaryOam.fill(0xFF);  // Secondary OAM clears to 0xFF
    m_framebuffer.fill(0xFF000000);
    
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
            std::cerr << "[PPU] Read from write-only register PPUCTRL ($2000)" << std::endl;
            break;
            
        case PPUMASK:   // $2001 - Write only
            std::cerr << "[PPU] Read from write-only register PPUMASK ($2001)" << std::endl;
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
            std::cerr << "[PPU] Read from write-only register OAMADDR ($2003)" << std::endl;
            break;
            
        case OAMDATA:   // $2004
            // Reading during rendering returns 0xFF for bytes 0-1 of secondary OAM
            // but we'll simplify and just return OAM data
            result = m_oam[m_oamAddr];
            
            // Bits 2-4 of sprite attributes are unimplemented and read back as 0
            if ((m_oamAddr & 0x03) == 2) {
                result &= 0xE3;
            }
            break;
            
        case PPUSCROLL: // $2005 - Write only
            std::cerr << "[PPU] Read from write-only register PPUSCROLL ($2005)" << std::endl;
            break;
            
        case PPUADDR:   // $2006 - Write only
            std::cerr << "[PPU] Read from write-only register PPUADDR ($2006)" << std::endl;
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
            std::cerr << "[PPU] Write to read-only register PPUSTATUS ($2002) = $"
                      << std::hex << static_cast<int>(value) << std::dec << std::endl;
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
        // Check if mapper wants to handle nametable read
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
        // Pattern tables - write to cartridge CHR (if CHR RAM)
        if (m_cartridge) {
            m_cartridge->writeCHR(address, value);
        }
    }
    else if (address < 0x3F00) {
        // Nametables
        if (m_cartridge && m_cartridge->writeNametable(address, value)) {
            return;
        }
        m_vram[mirrorNametableAddress(address)] = value;
    }
    else {
        // Palette
        u16 paletteAddr = address & 0x1F;
        
        // Mirror sprite palette to background palette
        if ((paletteAddr & 0x13) == 0x10) {
            paletteAddr &= 0x0F;
        }
        
        m_palette[paletteAddr] = value;
    }
}

u16 PPU::mirrorNametableAddress(u16 address) const {
    // Remove base offset and mirror to 0-0xFFF range
    address = (address - 0x2000) & 0x0FFF;
    
    MirrorMode mirror = m_cartridge ? m_cartridge->getMirrorMode() : MirrorMode::HORIZONTAL;
    
    switch (mirror) {
        case MirrorMode::HORIZONTAL:
            // $2000-$23FF and $2400-$27FF both map to first 0x400 bytes
            // $2800-$2BFF and $2C00-$2FFF both map to second 0x400 bytes
            if (address < 0x0800) {
                return address & 0x03FF;
            } else {
                return 0x0400 + (address & 0x03FF);
            }
            
        case MirrorMode::VERTICAL:
            // $2000-$23FF and $2800-$2BFF both map to first 0x400 bytes
            // $2400-$27FF and $2C00-$2FFF both map to second 0x400 bytes
            return address & 0x07FF;
            
        case MirrorMode::SINGLE_SCREEN_A:
            return address & 0x03FF;
            
        case MirrorMode::SINGLE_SCREEN_B:
            return 0x0400 + (address & 0x03FF);
            
        case MirrorMode::FOUR_SCREEN:
        default:
            return address & 0x07FF;  // Limited to 2KB, but real 4-screen uses cart RAM
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

bool PPU::isFetchingBackgroundPattern() const {
    // Background pattern fetches happen during cycles 1-256 and 321-336
    return isRenderingEnabled() && ((m_cycle >= 1 && m_cycle <= 256) || 
                                    (m_cycle >= 321 && m_cycle <= 336));
}

bool PPU::isFetchingSpritePattern() const {
    // Sprite pattern fetches happen during cycles 257-320
    return isRenderingEnabled() && (m_cycle >= 257 && m_cycle <= 320);
}

// ============================================================================
// Address Manipulation (Loopy)
// ============================================================================

void PPU::incrementX() {
    if (!isRenderingEnabled()) return;
    
    // Increment coarse X
    if ((m_vramAddr & 0x001F) == 31) {
        m_vramAddr &= ~0x001F;           // Reset coarse X to 0
        m_vramAddr ^= 0x0400;            // Switch horizontal nametable
    } else {
        m_vramAddr++;
    }
}

void PPU::incrementY() {
    if (!isRenderingEnabled()) return;
    
    // Increment fine Y
    if ((m_vramAddr & 0x7000) != 0x7000) {
        m_vramAddr += 0x1000;
    } else {
        m_vramAddr &= ~0x7000;           // Reset fine Y to 0
        
        // Increment coarse Y
        u16 coarseY = (m_vramAddr & 0x03E0) >> 5;
        if (coarseY == 29) {
            coarseY = 0;
            m_vramAddr ^= 0x0800;        // Switch vertical nametable
        } else if (coarseY == 31) {
            coarseY = 0;                 // Don't switch nametable (out-of-bounds)
        } else {
            coarseY++;
        }
        
        m_vramAddr = (m_vramAddr & ~0x03E0) | (coarseY << 5);
    }
}

void PPU::transferX() {
    if (!isRenderingEnabled()) return;
    
    // Copy horizontal position from temp to vram address
    // ....F.....EDCBA = ....F.....EDCBA
    m_vramAddr = (m_vramAddr & 0xFBE0) | (m_tempAddr & 0x041F);
}

void PPU::transferY() {
    if (!isRenderingEnabled()) return;
    
    // Copy vertical position from temp to vram address
    // IHGF.ED CBA..... = IHGF.ED CBA.....
    m_vramAddr = (m_vramAddr & 0x841F) | (m_tempAddr & 0x7BE0);
}

// ============================================================================
// Background Rendering
// ============================================================================

void PPU::fetchBackgroundTile() {
    // This is called at specific cycles to fetch background tile data
    // Cycles 1-256 and 321-336 do background fetches (every 8 cycles)
    
    switch (m_cycle & 0x07) {
        case 1:
            // Load shifters with previously fetched data
            loadBackgroundShifters();
            
            // Fetch nametable byte
            m_bgNextTileId = ppuRead(0x2000 | (m_vramAddr & 0x0FFF));
            break;
            
        case 3:
            // Fetch attribute byte
            {
                u16 attrAddr = 0x23C0 |
                              (m_vramAddr & 0x0C00) |           // Nametable select
                              ((m_vramAddr >> 4) & 0x38) |      // Coarse Y / 4 * 8
                              ((m_vramAddr >> 2) & 0x07);       // Coarse X / 4
                u8 attrByte = ppuRead(attrAddr);
                
                // Select the 2-bit palette for this tile
                if (m_vramAddr & 0x0002) attrByte >>= 2;        // Right half
                if (m_vramAddr & 0x0040) attrByte >>= 4;        // Bottom half
                m_bgNextTileAttr = attrByte & 0x03;
            }
            break;
            
        case 5:
            // Fetch pattern table low byte
            {
                u16 patternAddr = ((m_ppuCtrl & PPUCTRL_BG_TABLE) ? 0x1000 : 0x0000) +
                                 (static_cast<u16>(m_bgNextTileId) << 4) +
                                 ((m_vramAddr >> 12) & 0x07);   // Fine Y
                m_bgNextTileLow = ppuRead(patternAddr);
            }
            break;
            
        case 7:
            // Fetch pattern table high byte
            {
                u16 patternAddr = ((m_ppuCtrl & PPUCTRL_BG_TABLE) ? 0x1000 : 0x0000) +
                                 (static_cast<u16>(m_bgNextTileId) << 4) +
                                 ((m_vramAddr >> 12) & 0x07) + 8;
                m_bgNextTileHigh = ppuRead(patternAddr);
            }
            break;
            
        case 0:
            // Increment horizontal position
            incrementX();
            break;
    }
}

void PPU::loadBackgroundShifters() {
    // Load the new tile data into the shift registers
    m_bgShiftPatternLow = (m_bgShiftPatternLow & 0xFF00) | m_bgNextTileLow;
    m_bgShiftPatternHigh = (m_bgShiftPatternHigh & 0xFF00) | m_bgNextTileHigh;
    
    // Attribute is same for all 8 pixels, so expand to 8 bits
    m_bgShiftAttrLow = (m_bgShiftAttrLow & 0xFF00) | ((m_bgNextTileAttr & 0x01) ? 0xFF : 0x00);
    m_bgShiftAttrHigh = (m_bgShiftAttrHigh & 0xFF00) | ((m_bgNextTileAttr & 0x02) ? 0xFF : 0x00);
}

void PPU::updateShifters() {
    if (isBackgroundEnabled()) {
        m_bgShiftPatternLow <<= 1;
        m_bgShiftPatternHigh <<= 1;
        m_bgShiftAttrLow <<= 1;
        m_bgShiftAttrHigh <<= 1;
    }
}

u8 PPU::getBackgroundPixel() const {
    if (!isBackgroundEnabled()) return 0;
    
    // Check left 8 pixel masking
    if (m_cycle <= 8 && !(m_ppuMask & PPUMASK_SHOW_BG_LEFT)) return 0;
    
    // Select the bit from the shift register based on fine X
    u16 bitSelect = 0x8000 >> m_fineX;
    
    u8 pixel = 0;
    if (m_bgShiftPatternLow & bitSelect) pixel |= 0x01;
    if (m_bgShiftPatternHigh & bitSelect) pixel |= 0x02;
    
    u8 palette = 0;
    if (m_bgShiftAttrLow & bitSelect) palette |= 0x01;
    if (m_bgShiftAttrHigh & bitSelect) palette |= 0x02;
    
    // Return full palette index (palette << 2 | pixel)
    return (palette << 2) | pixel;
}

// ============================================================================
// Sprite Evaluation
// ============================================================================

void PPU::evaluateSprites() {
    // Sprite evaluation happens during cycles 65-256 of visible scanlines
    // But we simplify by doing it all at once at cycle 257
    
    m_spriteCount = 0;
    m_sprite0OnScanline = false;
    m_secondaryOam.fill(0xFF);
    
    u8 spriteHeight = getSpriteHeight();
    
    // Clear sprite overflow flag for next scanline
    // (actual overflow detection happens during evaluation)
    
    for (u8 i = 0; i < 64 && m_spriteCount < 8; i++) {
        const OAMEntry* sprite = reinterpret_cast<const OAMEntry*>(&m_oam[i * 4]);
        
        // Check if sprite is on next scanline
        // Sprite Y is the Y position minus 1
        s16 diff = static_cast<s16>(m_scanline) - static_cast<s16>(sprite->y);
        
        if (diff >= 0 && diff < spriteHeight) {
            // Copy to secondary OAM
            m_secondaryOam[m_spriteCount * 4 + 0] = sprite->y;
            m_secondaryOam[m_spriteCount * 4 + 1] = sprite->tileIndex;
            m_secondaryOam[m_spriteCount * 4 + 2] = sprite->attributes;
            m_secondaryOam[m_spriteCount * 4 + 3] = sprite->x;
            
            // Track sprite 0 for hit detection
            if (i == 0) {
                m_sprite0OnScanline = true;
            }
            
            // Store OAM index for sprite 0 hit detection
            m_spriteRenderData[m_spriteCount].oamIndex = i;
            
            m_spriteCount++;
        }
    }
    
    // Check for sprite overflow (continue evaluation even after 8 sprites)
    // This is a simplified implementation - real hardware has a bug
    if (m_spriteCount == 8) {
        for (u8 i = 0; i < 64; i++) {
            const OAMEntry* sprite = reinterpret_cast<const OAMEntry*>(&m_oam[i * 4]);
            s16 diff = static_cast<s16>(m_scanline) - static_cast<s16>(sprite->y);
            
            if (diff >= 0 && diff < spriteHeight) {
                // Set sprite overflow flag
                m_ppuStatus |= PPUSTATUS_SPRITE_OVERFLOW;
                break;
            }
        }
    }
}

void PPU::loadSpriteTiles() {
    // Load pattern data for each sprite found during evaluation
    // This happens during cycles 257-320
    
    for (u8 i = 0; i < m_spriteCount; i++) {
        fetchSpritePattern(i);
    }
    
    // Fill remaining sprite slots with transparent data
    for (u8 i = m_spriteCount; i < 8; i++) {
        m_spriteRenderData[i].patternLow = 0;
        m_spriteRenderData[i].patternHigh = 0;
        m_spriteRenderData[i].attributes = 0xFF;
        m_spriteRenderData[i].x = 0xFF;
    }
}

void PPU::fetchSpritePattern(u8 spriteIndex) {
    u8 y = m_secondaryOam[spriteIndex * 4 + 0];
    u8 tileIndex = m_secondaryOam[spriteIndex * 4 + 1];
    u8 attributes = m_secondaryOam[spriteIndex * 4 + 2];
    u8 x = m_secondaryOam[spriteIndex * 4 + 3];
    
    u8 spriteHeight = getSpriteHeight();
    
    // Calculate row within sprite
    s16 row = static_cast<s16>(m_scanline) - static_cast<s16>(y);
    
    // Handle vertical flip
    if (attributes & OAM_FLIP_V) {
        row = spriteHeight - 1 - row;
    }
    
    u16 patternAddr;
    
    if (spriteHeight == 16) {
        // 8x16 sprites
        // Tile index bit 0 selects pattern table
        u16 table = (tileIndex & 0x01) ? 0x1000 : 0x0000;
        tileIndex &= 0xFE;  // Clear bit 0
        
        // Select top or bottom tile
        if (row >= 8) {
            tileIndex++;
            row -= 8;
        }
        
        patternAddr = table + (static_cast<u16>(tileIndex) << 4) + row;
    } else {
        // 8x8 sprites
        u16 table = (m_ppuCtrl & PPUCTRL_SPRITE_TABLE) ? 0x1000 : 0x0000;
        patternAddr = table + (static_cast<u16>(tileIndex) << 4) + row;
    }
    
    u8 patternLow = ppuRead(patternAddr);
    u8 patternHigh = ppuRead(patternAddr + 8);
    
    // Handle horizontal flip by reversing bits
    if (attributes & OAM_FLIP_H) {
        // Reverse bits using lookup or bit manipulation
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

u8 PPU::getSpritePixel(u8& spriteIndex, bool& priority) const {
    if (!isSpriteEnabled()) return 0;
    
    // Check left 8 pixel masking
    if (m_cycle <= 8 && !(m_ppuMask & PPUMASK_SHOW_SPR_LEFT)) return 0;
    
    // Check each sprite to find the first non-transparent pixel
    for (u8 i = 0; i < m_spriteCount; i++) {
        // Calculate X offset within sprite
        s16 offset = static_cast<s16>(m_cycle - 1) - static_cast<s16>(m_spriteRenderData[i].x);
        
        if (offset >= 0 && offset < 8) {
            u8 bitSelect = 7 - offset;
            
            u8 pixel = 0;
            if (m_spriteRenderData[i].patternLow & (1 << bitSelect)) pixel |= 0x01;
            if (m_spriteRenderData[i].patternHigh & (1 << bitSelect)) pixel |= 0x02;
            
            // Skip transparent pixels
            if (pixel == 0) continue;
            
            spriteIndex = i;
            priority = (m_spriteRenderData[i].attributes & OAM_PRIORITY) != 0;
            
            // Return full palette index (0x10 for sprite palette + palette number + pixel)
            u8 palette = m_spriteRenderData[i].attributes & OAM_PALETTE;
            return 0x10 | (palette << 2) | pixel;
        }
    }
    
    return 0;
}

// ============================================================================
// Pixel Output
// ============================================================================

void PPU::renderPixel() {
    if (m_cycle == 0 || m_cycle > 256 || m_scanline >= 240) return;
    
    u8 bgPixel = getBackgroundPixel();
    u8 spriteIndex = 0;
    bool spritePriority = false;
    u8 spritePixel = getSpritePixel(spriteIndex, spritePriority);
    
    u8 finalPixel = 0;
    
    // Priority logic:
    // BG pixel 0 means transparent (use background color)
    // Sprite pixel 0 means transparent
    // Priority bit determines front/back when both non-zero
    
    bool bgOpaque = (bgPixel & 0x03) != 0;
    bool spriteOpaque = (spritePixel & 0x03) != 0;
    
    // Sprite 0 hit detection
    // Hit occurs when both BG and sprite 0 are opaque, and rendering is enabled
    if (m_sprite0OnScanline && spriteOpaque && bgOpaque &&
        m_spriteRenderData[spriteIndex].oamIndex == 0 &&
        m_cycle != 256 && isBackgroundEnabled() && isSpriteEnabled()) {
        
        // Don't set sprite 0 hit in the leftmost 8 pixels if clipping is enabled
        if (m_cycle > 8 || 
            ((m_ppuMask & PPUMASK_SHOW_BG_LEFT) && (m_ppuMask & PPUMASK_SHOW_SPR_LEFT))) {
            m_ppuStatus |= PPUSTATUS_SPRITE0_HIT;
        }
    }
    
    // Determine which pixel to render
    if (!bgOpaque && !spriteOpaque) {
        // Both transparent - use background color
        finalPixel = 0;
    } else if (bgOpaque && !spriteOpaque) {
        // Only BG is opaque
        finalPixel = bgPixel;
    } else if (!bgOpaque && spriteOpaque) {
        // Only sprite is opaque
        finalPixel = spritePixel;
    } else {
        // Both opaque - use priority
        if (spritePriority) {
            // Sprite behind background
            finalPixel = bgPixel;
        } else {
            // Sprite in front of background
            finalPixel = spritePixel;
        }
    }
    
    // Look up color in palette
    u8 colorIndex = ppuRead(0x3F00 + finalPixel) & 0x3F;
    u32 color = s_nesPalette[colorIndex];
    
    // Apply color emphasis (simplified)
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
    
    // Write to framebuffer
    u32 x = m_cycle - 1;
    u32 y = m_scanline;
    m_framebuffer[y * SCREEN_WIDTH + x] = color;
}

// ============================================================================
// Main Step Function
// ============================================================================

void PPU::step() {
    // Visible scanlines (0-239)
    if (m_scanline < VISIBLE_SCANLINES) {
        // Sprite evaluation at cycle 257
        if (m_cycle == 257 && isRenderingEnabled()) {
            evaluateSprites();
            loadSpriteTiles();
        }
        
        // Background fetching
        if ((m_cycle >= 1 && m_cycle <= 256) || (m_cycle >= 321 && m_cycle <= 336)) {
            updateShifters();
            fetchBackgroundTile();
        }
        
        // Render pixel
        if (m_cycle >= 1 && m_cycle <= 256) {
            renderPixel();
        }
        
        // Increment Y at end of scanline
        if (m_cycle == 256) {
            incrementY();
        }
        
        // Transfer horizontal position at cycle 257
        if (m_cycle == 257) {
            transferX();
        }
    }
    
    // Post-render scanline (240) - idle
    
    // VBlank scanlines (241-260)
    if (m_scanline == 241 && m_cycle == 1) {
        // Set VBlank flag
        m_ppuStatus |= PPUSTATUS_VBLANK;
        m_nmiOccurred = true;
        
        // Generate NMI if enabled
        if (m_nmiOutput) {
            // Start NMI. CPU will delay NMI by 2 instructions.
            m_cpu->nmi();
        }
        
        // Render frame
        if (m_videoDevice) {
            m_videoDevice->render(m_framebuffer.data());
        }
        
        m_frameComplete = true;
    }
    
    // Pre-render scanline (261)
    if (m_scanline == PRE_RENDER_SCANLINE) {
        // Clear VBlank, sprite 0 hit, and overflow at cycle 1
        if (m_cycle == 1) {
            m_ppuStatus &= ~(PPUSTATUS_VBLANK | PPUSTATUS_SPRITE0_HIT | PPUSTATUS_SPRITE_OVERFLOW);
            m_nmiOccurred = false;
        }
        
        // Background fetching (for next frame's first scanline)
        if ((m_cycle >= 1 && m_cycle <= 256) || (m_cycle >= 321 && m_cycle <= 336)) {
            updateShifters();
            fetchBackgroundTile();
        }
        
        // Transfer vertical position during cycles 280-304
        if (m_cycle >= 280 && m_cycle <= 304) {
            transferY();
        }
        
        // Increment Y at end of scanline
        if (m_cycle == 256) {
            incrementY();
        }
        
        // Transfer horizontal position at cycle 257
        if (m_cycle == 257) {
            transferX();
            // Also evaluate sprites for first visible scanline
            if (isRenderingEnabled()) {
                evaluateSprites();
                loadSpriteTiles();
            }
        }
    }
    
    // Check for scanline counter (for MMC3/MMC5 IRQ)
    // MMC3 clocks on A12 rising edge, which happens during PPU rendering
    // A12 only transitions when the PPU is fetching pattern data, which requires rendering to be enabled
    // Must also clock on pre-render scanline (261) so CHR banks are correct for scanline 0
    // MMC5 clocks at PPU cycle 4, when the PPU does the attribute table byte read
    u16 targetCycle = m_cartridge->getMapperNumber() == 5 ? 3 : 260;
    if (m_cycle == targetCycle && 
        isRenderingEnabled() &&
        (m_scanline < VISIBLE_SCANLINES || m_scanline == PRE_RENDER_SCANLINE)) {
        if (m_cartridge) {
            m_cartridge->scanlineCounter();
            if (m_cartridge->irqState()) {
                if (m_cpu) {
                    m_cpu->irq();
                }
                m_cartridge->irqClear();
            }
        }
    }
    
    // Increment cycle and scanline
    m_cycle++;
    
    if (m_cycle > 340) {
        m_cycle = 0;
        m_scanline++;
        
        if (m_scanline > PRE_RENDER_SCANLINE) {
            m_scanline = 0;
            m_oddFrame = !m_oddFrame;
            
            // Skip cycle 0 on odd frames when rendering enabled
            if (m_oddFrame && isRenderingEnabled()) {
                m_cycle = 1;
            }
        }
    }
}

// ============================================================================
// Save/Load State
// ============================================================================

void PPU::saveState(Buffer* buf) {
    // Timing
    buffer_write(buf, &m_cycle, sizeof(m_cycle));
    buffer_write(buf, &m_scanline, sizeof(m_scanline));
    buffer_write(buf, &m_frameComplete, sizeof(m_frameComplete));
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
    
    // Background shift registers
    buffer_write(buf, &m_bgShiftPatternLow, sizeof(m_bgShiftPatternLow));
    buffer_write(buf, &m_bgShiftPatternHigh, sizeof(m_bgShiftPatternHigh));
    buffer_write(buf, &m_bgShiftAttrLow, sizeof(m_bgShiftAttrLow));
    buffer_write(buf, &m_bgShiftAttrHigh, sizeof(m_bgShiftAttrHigh));
    
    // Background latches
    buffer_write(buf, &m_bgNextTileId, sizeof(m_bgNextTileId));
    buffer_write(buf, &m_bgNextTileAttr, sizeof(m_bgNextTileAttr));
    buffer_write(buf, &m_bgNextTileLow, sizeof(m_bgNextTileLow));
    buffer_write(buf, &m_bgNextTileHigh, sizeof(m_bgNextTileHigh));
    
    // Memory
    buffer_write(buf, m_vram.data(), m_vram.size());
    buffer_write(buf, m_palette.data(), m_palette.size());
    buffer_write(buf, m_oam.data(), m_oam.size());
    buffer_write(buf, m_secondaryOam.data(), m_secondaryOam.size());
    
    // Sprite state
    buffer_write(buf, &m_spriteCount, sizeof(m_spriteCount));
    buffer_write(buf, &m_sprite0OnScanline, sizeof(m_sprite0OnScanline));
    buffer_write(buf, &m_sprite0HitPossible, sizeof(m_sprite0HitPossible));
    
    // NMI state
    buffer_write(buf, &m_nmiOccurred, sizeof(m_nmiOccurred));
    buffer_write(buf, &m_nmiOutput, sizeof(m_nmiOutput));
    
    buffer_write(buf, &m_openBus, sizeof(m_openBus));
}

void PPU::loadState(Buffer* buf) {
    // Timing
    buffer_read(buf, &m_cycle, sizeof(m_cycle));
    buffer_read(buf, &m_scanline, sizeof(m_scanline));
    buffer_read(buf, &m_frameComplete, sizeof(m_frameComplete));
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
    
    // Background shift registers
    buffer_read(buf, &m_bgShiftPatternLow, sizeof(m_bgShiftPatternLow));
    buffer_read(buf, &m_bgShiftPatternHigh, sizeof(m_bgShiftPatternHigh));
    buffer_read(buf, &m_bgShiftAttrLow, sizeof(m_bgShiftAttrLow));
    buffer_read(buf, &m_bgShiftAttrHigh, sizeof(m_bgShiftAttrHigh));
    
    // Background latches
    buffer_read(buf, &m_bgNextTileId, sizeof(m_bgNextTileId));
    buffer_read(buf, &m_bgNextTileAttr, sizeof(m_bgNextTileAttr));
    buffer_read(buf, &m_bgNextTileLow, sizeof(m_bgNextTileLow));
    buffer_read(buf, &m_bgNextTileHigh, sizeof(m_bgNextTileHigh));
    
    // Memory
    buffer_read(buf, m_vram.data(), m_vram.size());
    buffer_read(buf, m_palette.data(), m_palette.size());
    buffer_read(buf, m_oam.data(), m_oam.size());
    buffer_read(buf, m_secondaryOam.data(), m_secondaryOam.size());
    
    // Sprite state
    buffer_read(buf, &m_spriteCount, sizeof(m_spriteCount));
    buffer_read(buf, &m_sprite0OnScanline, sizeof(m_sprite0OnScanline));
    buffer_read(buf, &m_sprite0HitPossible, sizeof(m_sprite0HitPossible));
    
    // NMI state
    buffer_read(buf, &m_nmiOccurred, sizeof(m_nmiOccurred));
    buffer_read(buf, &m_nmiOutput, sizeof(m_nmiOutput));
    
    buffer_read(buf, &m_openBus, sizeof(m_openBus));
}

} // namespace nes

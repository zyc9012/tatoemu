#include "ppu.h"
#include "cpu.h"
#include "mmu.h"
#include <algorithm>
#include <cstring>

namespace gb {

// LCD timing constants
constexpr u32 SCANLINE_OAM_CYCLES = 80;
constexpr u32 SCANLINE_VRAM_CYCLES = 172;
constexpr u32 SCANLINE_HBLANK_CYCLES = 204;
constexpr u32 SCANLINE_CYCLES = SCANLINE_OAM_CYCLES + SCANLINE_VRAM_CYCLES + SCANLINE_HBLANK_CYCLES;

PPU::PPU()
    : m_cpu(nullptr)
    , m_mmu(nullptr)
    , m_videoDevice(nullptr)
    , m_vramBank(0)
    , m_lcdc(0x00)  // Will be set properly in reset()
    , m_stat(0)
    , m_scy(0)
    , m_scx(0)
    , m_ly(0)
    , m_lyc(0)
    , m_dma(0)
    , m_bgp(0x00)   // Will be set properly in reset()
    , m_obp0(0x00)  // Will be set properly in reset()
    , m_obp1(0x00)  // Will be set properly in reset()
    , m_wy(0)
    , m_wx(0)
    , m_bgpi(0)
    , m_obpi(0)
    , m_hdma1(0)
    , m_hdma2(0)
    , m_hdma3(0)
    , m_hdma4(0)
    , m_hdma5(0xFF)
    , m_hdmaActive(false)
    , m_hdmaSource(0)
    , m_hdmaDest(0)
    , m_hdmaRemaining(0)
    , m_mode(PPUMode::OAM_SCAN)
    , m_modeCycles(0)
    , m_windowLineCounter(0)
    , m_windowRenderedThisFrame(false)
    , m_gbcMode(false)
    , m_statInterruptLine(false)
    , m_modeChangeDelay(0)
    , m_dmaCycles(0) {
    std::fill(m_vram.begin(), m_vram.end(), 0);
    std::fill(m_oam.begin(), m_oam.end(), 0);
    std::fill(m_bgPaletteData.begin(), m_bgPaletteData.end(), 0);
    std::fill(m_objPaletteData.begin(), m_objPaletteData.end(), 0);
    std::fill(m_framebuffer.begin(), m_framebuffer.end(), 0xFFFFFFFF);
    std::fill(m_bgPriority.begin(), m_bgPriority.end(), 0);
    std::fill(m_bgPriorityFlag.begin(), m_bgPriorityFlag.end(), false);
}

PPU::~PPU() {
}

void PPU::setCPU(CPU* cpu) {
    m_cpu = cpu;
}

void PPU::setMMU(MMU* mmu) {
    m_mmu = mmu;
}

void PPU::setVideoDevice(VideoDevice* videoDevice) {
    m_videoDevice = videoDevice;
}

void PPU::reset(bool useBootrom) {
    std::fill(m_vram.begin(), m_vram.end(), 0);
    std::fill(m_oam.begin(), m_oam.end(), 0);
    
    m_vramBank = 0;
    
    if (useBootrom) {
        // When using bootrom, LCD starts disabled and bootrom initializes it
        m_lcdc = 0x00;  // LCD disabled
        m_stat = 0;
        m_scy = 0;
        m_scx = 0;
        m_ly = 0;
        m_lyc = 0;
        m_dma = 0;
        m_bgp = 0x00;   // Bootrom will initialize
        m_obp0 = 0x00;  // Bootrom will initialize
        m_obp1 = 0x00;  // Bootrom will initialize
        m_wy = 0;
        m_wx = 0;
    } else {
        // Post-bootrom values (skip bootrom)
        m_lcdc = 0x91;
        m_stat = 0;
        m_scy = 0;
        m_scx = 0;
        m_ly = 0;
        m_lyc = 0;
        m_dma = 0;
        m_bgp = 0xFC;
        m_obp0 = 0xFF;
        m_obp1 = 0xFF;
        m_wy = 0;
        m_wx = 0;
    }
    
    m_bgpi = 0;
    m_obpi = 0;
    std::fill(m_bgPaletteData.begin(), m_bgPaletteData.end(), 0);
    std::fill(m_objPaletteData.begin(), m_objPaletteData.end(), 0);
    
    m_hdma1 = 0;
    m_hdma2 = 0;
    m_hdma3 = 0;
    m_hdma4 = 0;
    m_hdma5 = 0xFF;
    m_hdmaActive = false;
    m_hdmaSource = 0;
    m_hdmaDest = 0;
    m_hdmaRemaining = 0;
    
    m_mode = PPUMode::OAM_SCAN;
    m_modeCycles = 0;
    m_windowLineCounter = 0;
    m_windowRenderedThisFrame = false;
    m_statInterruptLine = false;
    m_modeChangeDelay = 0;
    
    std::fill(m_framebuffer.begin(), m_framebuffer.end(), 0xFFFFFFFF);
}

void PPU::setGBCMode(bool enabled) {
    m_gbcMode = enabled;
}

void PPU::step(u32 cycles) {
    // If LCD is disabled, do nothing
    if (!(m_lcdc & LCDC_LCD_ENABLE)) {
        return;
    }

    // In double speed mode, the PPU should run at the same real-time rate
    // Since CPU provides cycles at 2x rate in double speed, we accumulate
    // cycles at the same rate (PPU timing is based on real clock, not CPU clock)
    m_modeCycles += cycles;

    // PPU timing thresholds are doubled in double speed mode to maintain
    // the same real-time rate (since CPU cycles come in at 2x rate)
    u32 speedMultiplier = (m_mmu && m_mmu->isDoubleSpeed()) ? 2 : 1;

    // Check for delayed STAT interrupt from previous mode change
    if (m_modeChangeDelay > 0) {
        m_modeChangeDelay--;
        if (m_modeChangeDelay == 0) {
            updateStatInterrupt();
        }
    }
    
    switch (m_mode) {
        case PPUMode::OAM_SCAN:
            if (m_modeCycles >= SCANLINE_OAM_CYCLES * speedMultiplier) {
                m_modeCycles -= SCANLINE_OAM_CYCLES * speedMultiplier;
                // Reset window rendered flag for this scanline
                m_windowRenderedThisFrame = false;
                setMode(PPUMode::DRAWING);
            }
            break;

        case PPUMode::DRAWING:
            if (m_modeCycles >= SCANLINE_VRAM_CYCLES * speedMultiplier) {
                m_modeCycles -= SCANLINE_VRAM_CYCLES * speedMultiplier;
                
                // Render the current scanline
                renderScanline();
                
                setMode(PPUMode::HBLANK);
                
                // Perform HDMA transfer if active
                if (m_hdmaActive && m_gbcMode) {
                    performHDMA();
                }
            }
            break;

        case PPUMode::HBLANK:
            if (m_modeCycles >= SCANLINE_HBLANK_CYCLES * speedMultiplier) {
                m_modeCycles -= SCANLINE_HBLANK_CYCLES * speedMultiplier;
                
                // Increment window line counter if window was rendered this scanline
                if (m_windowRenderedThisFrame && (m_lcdc & LCDC_WINDOW_ENABLE) && 
                    m_wy <= m_ly && m_wx < 167) {
                    m_windowLineCounter++;
                }
                
                m_ly++;

                // Check for STAT mode 2 (OAM) interrupt at start of VBLANK
                // This interrupt triggers on DMG/SGB but not on CGB/AGB/AGS
                if (m_ly >= SCREEN_HEIGHT && !m_gbcMode && (m_stat & STAT_OAM_INTERRUPT) && !m_statInterruptLine) {
                    if (m_cpu) {
                        m_cpu->requestInterrupt(INT_LCD_STAT);
                    }
                    m_statInterruptLine = true;
                }

                // Check LYC=LY
                if (m_ly == m_lyc) {
                    m_stat |= STAT_LYC_EQUAL;
                    if (m_stat & STAT_LYC_INTERRUPT) {
                        updateStatInterrupt();
                    }
                } else {
                    m_stat &= ~STAT_LYC_EQUAL;
                }
                
                if (m_ly >= SCREEN_HEIGHT) {
                    // Enter VBlank
                    setMode(PPUMode::VBLANK);
                    m_windowLineCounter = 0;
                    // Don't reset m_windowRenderedThisFrame here - it's per scanline, reset in OAM_SCAN

                    // Request VBlank interrupt
                    if (m_cpu) {
                        m_cpu->requestInterrupt(INT_VBLANK);
                    }

                    if (m_videoDevice) {
                        m_videoDevice->render(m_framebuffer.data());
                    }
                } else {
                    // Start next scanline
                    setMode(PPUMode::OAM_SCAN);
                }
            }
            break;

        case PPUMode::VBLANK:
            if (m_modeCycles >= SCANLINE_CYCLES * speedMultiplier) {
                m_modeCycles -= SCANLINE_CYCLES * speedMultiplier;
                m_ly++;
                
                // Check LYC=LY
                if (m_ly == m_lyc) {
                    m_stat |= STAT_LYC_EQUAL;
                    if (m_stat & STAT_LYC_INTERRUPT) {
                        updateStatInterrupt();
                    }
                } else {
                    m_stat &= ~STAT_LYC_EQUAL;
                }
                
                if (m_ly > 153) {
                    // Start new frame
                    m_ly = 0;
                    
                    // Check LYC=LY for line 0
                    if (m_ly == m_lyc) {
                        m_stat |= STAT_LYC_EQUAL;
                        if (m_stat & STAT_LYC_INTERRUPT) {
                            updateStatInterrupt();
                        }
                    } else {
                        m_stat &= ~STAT_LYC_EQUAL;
                    }
                    
                    setMode(PPUMode::OAM_SCAN);
                }
            }
            break;
    }
}

void PPU::setMode(PPUMode mode) {
    m_mode = mode;
    m_stat = (m_stat & 0xFC) | static_cast<u8>(mode);
    m_modeChangeDelay = 1;  // Delay STAT interrupt by 1 cycle
}

void PPU::updateStatInterrupt() {
    bool newStatLine = false;
    
    // Check if any STAT interrupt conditions are met
    if ((m_stat & STAT_LYC_INTERRUPT) && (m_stat & STAT_LYC_EQUAL)) {
        newStatLine = true;
    }
    if ((m_stat & STAT_OAM_INTERRUPT) && m_mode == PPUMode::OAM_SCAN) {
        newStatLine = true;
    }
    if ((m_stat & STAT_VBLANK_INTERRUPT) && m_mode == PPUMode::VBLANK) {
        newStatLine = true;
    }
    if ((m_stat & STAT_HBLANK_INTERRUPT) && m_mode == PPUMode::HBLANK) {
        newStatLine = true;
    }
    
    // Trigger interrupt on rising edge
    if (newStatLine && !m_statInterruptLine && m_cpu) {
        m_cpu->requestInterrupt(INT_LCD_STAT);
    }
    
    m_statInterruptLine = newStatLine;
}

void PPU::renderScanline() {
    // Clear priority buffers for this scanline
    std::fill(m_bgPriority.begin(), m_bgPriority.end(), 0);
    std::fill(m_bgPriorityFlag.begin(), m_bgPriorityFlag.end(), false);
    
    // Render background (always in GBC mode, conditional in DMG mode)
    if (m_gbcMode || (m_lcdc & LCDC_BG_WINDOW_ENABLE)) {
        renderBackground();
    } else {
        // Fill scanline with white in DMG mode when BG is disabled
        for (u16 x = 0; x < SCREEN_WIDTH; x++) {
            m_framebuffer[m_ly * SCREEN_WIDTH + x] = 0xFFFFFFFF;
        }
    }
    
    // Check if window should be rendered on this scanline
    if ((m_lcdc & LCDC_WINDOW_ENABLE) && m_wy <= m_ly && m_wx < 167) {
        renderWindow();
    }
    
    // Render sprites
    if (m_lcdc & LCDC_OBJ_ENABLE) {
        renderSprites();
    }
}

void PPU::renderBackground() {
    u16 tileMapBase = (m_lcdc & LCDC_BG_TILEMAP) ? 0x9C00 : 0x9800;
    bool unsignedTileIndex = (m_lcdc & LCDC_BG_WINDOW_TILES) != 0;
    
    u8 scrollY = m_scy;
    u8 scrollX = m_scx;
    
    u8 y = (m_ly + scrollY) & 0xFF;
    u8 tileY = y / 8;
    u8 tilePixelY = y % 8;
    
    for (u16 x = 0; x < SCREEN_WIDTH; x++) {
        u8 scrolledX = (x + scrollX) & 0xFF;
        u8 tileX = scrolledX / 8;
        u8 tilePixelX = scrolledX % 8;
        
        // Get tile index from tilemap
        u16 tileMapAddr = tileMapBase + (tileY * 32) + tileX;
        u8 tileIndex = m_vram[tileMapAddr - 0x8000];
        
        // Get tile attributes (GBC only)
        u8 tileAttrs = 0;
        if (m_gbcMode) {
            tileAttrs = m_vram[0x2000 + (tileMapAddr - 0x8000)]; // Bank 1
        }
        
        // Calculate tile data address
        u16 tileDataBase;
        if (unsignedTileIndex) {
            tileDataBase = 0x8000 + (tileIndex * 16);
        } else {
            s8 signedIndex = static_cast<s8>(tileIndex);
            tileDataBase = 0x9000 + (signedIndex * 16);
        }
        
        // Check for Y flip
        u8 actualPixelY = (tileAttrs & SPRITE_Y_FLIP) ? (7 - tilePixelY) : tilePixelY;
        
        // Check for X flip
        u8 actualPixelX = (tileAttrs & SPRITE_X_FLIP) ? (7 - tilePixelX) : tilePixelX;
        
        // Use VRAM bank from attributes (GBC only)
        bool useBank1 = m_gbcMode && (tileAttrs & SPRITE_BANK_GBC);
        
        // Get pixel color index
        u8 colorIndex = getTilePixel(tileDataBase, actualPixelX, actualPixelY, useBank1);
        
        // Store for sprite priority
        m_bgPriority[x] = colorIndex;
        
        // Store GBC BG priority flag (bit 7 of tile attributes)
        if (m_gbcMode) {
            m_bgPriorityFlag[x] = (tileAttrs & SPRITE_PRIORITY) != 0;
        }
        
        // Convert to RGB
        u32 color;
        if (m_gbcMode) {
            u8 paletteIndex = tileAttrs & 0x07;
            u16 gbcColor = readColorPalette(paletteIndex, colorIndex);
            color = convertGBCColorToRGB(gbcColor);
        } else {
            color = getDMGColor(m_bgp, colorIndex);
        }
        
        m_framebuffer[m_ly * SCREEN_WIDTH + x] = color;
    }
}

void PPU::renderWindow() {
    if (m_wx > 166 || m_wy > 143) {
        return;
    }
    
    s16 windowX = m_wx - 7;
    if (windowX >= SCREEN_WIDTH) {
        return;
    }
    
    // Check if window will actually render any pixels
    if (windowX >= SCREEN_WIDTH) {
        return;
    }
    
    u16 tileMapBase = (m_lcdc & LCDC_WINDOW_TILEMAP) ? 0x9C00 : 0x9800;
    bool unsignedTileIndex = (m_lcdc & LCDC_BG_WINDOW_TILES) != 0;
    
    u8 y = m_windowLineCounter;
    u8 tileY = y / 8;
    u8 tilePixelY = y % 8;
    
    // Mark that window was rendered this frame
    m_windowRenderedThisFrame = true;
    
    for (u16 x = 0; x < SCREEN_WIDTH; x++) {
        s16 screenX = x;
        if (screenX < windowX) {
            continue;
        }
        
        u8 windowPixelX = screenX - windowX;
        u8 tileX = windowPixelX / 8;
        u8 tilePixelX = windowPixelX % 8;
        
        // Get tile index from tilemap
        u16 tileMapAddr = tileMapBase + (tileY * 32) + tileX;
        u8 tileIndex = m_vram[tileMapAddr - 0x8000];
        
        // Get tile attributes (GBC only)
        u8 tileAttrs = 0;
        if (m_gbcMode) {
            tileAttrs = m_vram[0x2000 + (tileMapAddr - 0x8000)]; // Bank 1
        }
        
        // Calculate tile data address
        u16 tileDataBase;
        if (unsignedTileIndex) {
            tileDataBase = 0x8000 + (tileIndex * 16);
        } else {
            s8 signedIndex = static_cast<s8>(tileIndex);
            tileDataBase = 0x9000 + (signedIndex * 16);
        }
        
        // Check for Y flip
        u8 actualPixelY = (tileAttrs & SPRITE_Y_FLIP) ? (7 - tilePixelY) : tilePixelY;
        
        // Check for X flip
        u8 actualPixelX = (tileAttrs & SPRITE_X_FLIP) ? (7 - tilePixelX) : tilePixelX;
        
        // Use VRAM bank from attributes (GBC only)
        bool useBank1 = m_gbcMode && (tileAttrs & SPRITE_BANK_GBC);
        
        // Get pixel color index
        u8 colorIndex = getTilePixel(tileDataBase, actualPixelX, actualPixelY, useBank1);
        
        // Store for sprite priority
        m_bgPriority[x] = colorIndex;
        
        // Store GBC BG priority flag (bit 7 of tile attributes)
        if (m_gbcMode) {
            m_bgPriorityFlag[x] = (tileAttrs & SPRITE_PRIORITY) != 0;
        }
        
        // Convert to RGB
        u32 color;
        if (m_gbcMode) {
            u8 paletteIndex = tileAttrs & 0x07;
            u16 gbcColor = readColorPalette(paletteIndex, colorIndex);
            color = convertGBCColorToRGB(gbcColor);
        } else {
            color = getDMGColor(m_bgp, colorIndex);
        }
        
        m_framebuffer[m_ly * SCREEN_WIDTH + x] = color;
    }
    
    // Increment window line counter (happens at end of scanline in step())
}

void PPU::renderSprites() {
    bool tallSprites = (m_lcdc & LCDC_OBJ_SIZE) != 0;
    u8 spriteHeight = tallSprites ? 16 : 8;
    
    // Collect visible sprites for this scanline
    struct VisibleSprite {
        u8 oamIndex;
        u8 x;
    };
    std::array<VisibleSprite, 10> visibleSprites;
    u8 spriteCount = 0;
    
    // OAM contains 40 sprites
    for (u8 i = 0; i < 40 && spriteCount < 10; i++) {
        Sprite sprite;
        sprite.y = m_oam[i * 4 + 0];
        sprite.x = m_oam[i * 4 + 1];
        sprite.tileIndex = m_oam[i * 4 + 2];
        sprite.flags = m_oam[i * 4 + 3];
        
        // Check if sprite is on this scanline
        s16 spriteY = sprite.y - 16;
        s16 scanline = m_ly;
        
        if (scanline >= spriteY && scanline < spriteY + spriteHeight) {
            visibleSprites[spriteCount].oamIndex = i;
            visibleSprites[spriteCount].x = sprite.x;
            spriteCount++;
        }
    }
    
    // Sort sprites by X coordinate (lower X has priority in DMG, first in OAM has priority in GBC)
    // For simplicity, we render back-to-front
    for (u8 i = 0; i < spriteCount; i++) {
        for (u8 j = i + 1; j < spriteCount; j++) {
            if (m_gbcMode) {
                // In GBC mode, lower OAM index has priority
                if (visibleSprites[j].oamIndex < visibleSprites[i].oamIndex) {
                    std::swap(visibleSprites[i], visibleSprites[j]);
                }
            } else {
                // In DMG mode, lower X has priority
                if (visibleSprites[j].x < visibleSprites[i].x) {
                    std::swap(visibleSprites[i], visibleSprites[j]);
                }
            }
        }
    }
    
    // Render sprites back-to-front
    for (int i = spriteCount - 1; i >= 0; i--) {
        u8 oamIndex = visibleSprites[i].oamIndex;
        
        Sprite sprite;
        sprite.y = m_oam[oamIndex * 4 + 0];
        sprite.x = m_oam[oamIndex * 4 + 1];
        sprite.tileIndex = m_oam[oamIndex * 4 + 2];
        sprite.flags = m_oam[oamIndex * 4 + 3];
        
        s16 spriteY = sprite.y - 16;
        s16 spriteX = sprite.x - 8;
        
        u8 line = m_ly - spriteY;
        
        // Handle Y flip
        if (sprite.flags & SPRITE_Y_FLIP) {
            line = spriteHeight - 1 - line;
        }
        
        // For 8x16 sprites, tile index bit 0 is ignored
        u8 tileIndex = sprite.tileIndex;
        if (tallSprites) {
            tileIndex &= 0xFE;
            if (line >= 8) {
                tileIndex |= 0x01;
                line -= 8;
            }
        }
        
        u16 tileDataBase = 0x8000 + (tileIndex * 16);
        bool useBank1 = m_gbcMode && (sprite.flags & SPRITE_BANK_GBC);
        
        // Render sprite pixels
        for (u8 x = 0; x < 8; x++) {
            s16 pixelX = spriteX + x;
            
            if (pixelX < 0 || pixelX >= SCREEN_WIDTH) {
                continue;
            }
            
            // Handle X flip
            u8 actualX = (sprite.flags & SPRITE_X_FLIP) ? (7 - x) : x;
            
            // Get pixel color index
            u8 colorIndex = getTilePixel(tileDataBase, actualX, line, useBank1);
            
            // Color 0 is transparent for sprites
            if (colorIndex == 0) {
                continue;
            }
            
            // Check sprite priority
            bool spritePriority = (sprite.flags & SPRITE_PRIORITY) != 0;
            
            if (m_gbcMode) {
                // In GBC mode, check BG-to-OBJ priority flag first
                // If BG priority flag is set and BG color is not 0, BG wins
                if (m_bgPriorityFlag[pixelX] && m_bgPriority[pixelX] != 0) {
                    continue;
                }
                // Then check sprite priority bit (sprite behind BG colors 1-3)
                if (spritePriority && m_bgPriority[pixelX] != 0) {
                    continue;
                }
            } else {
                // In DMG mode, priority bit means sprite is behind non-zero BG colors
                if (spritePriority && m_bgPriority[pixelX] != 0) {
                    continue;
                }
            }
            
            // Convert to RGB
            u32 color;
            if (m_gbcMode) {
                u8 paletteIndex = (sprite.flags & SPRITE_PALETTE_GBC);
                u16 gbcColor = readColorPalette(paletteIndex + 8, colorIndex); // Object palettes are 8-15
                color = convertGBCColorToRGB(gbcColor);
            } else {
                u8 palette = (sprite.flags & SPRITE_PALETTE_DMG) ? m_obp1 : m_obp0;
                color = getDMGColor(palette, colorIndex);
            }
            
            m_framebuffer[m_ly * SCREEN_WIDTH + pixelX] = color;
        }
    }
}

u8 PPU::getTilePixel(u16 tileAddress, u8 x, u8 y, bool useBank1) const {
    u16 tileDataAddr = tileAddress + (y * 2);
    u16 vramOffset = tileDataAddr - 0x8000;
    
    if (useBank1) {
        vramOffset += 0x2000;
    }
    
    u8 byte1 = m_vram[vramOffset];
    u8 byte2 = m_vram[vramOffset + 1];
    
    u8 bitIndex = 7 - x;
    u8 colorBit0 = (byte1 >> bitIndex) & 0x01;
    u8 colorBit1 = (byte2 >> bitIndex) & 0x01;
    
    return (colorBit1 << 1) | colorBit0;
}

u32 PPU::getDMGColor(u8 paletteValue, u8 colorIndex) const {
    u8 shade = (paletteValue >> (colorIndex * 2)) & 0x03;
    
    // DMG color palette (light to dark)
    static const u32 dmgColors[4] = {
        0xFFE0F8D0,  // Lightest
        0xFF88C070,
        0xFF346856,
        0xFF081820   // Darkest
    };
    
    return dmgColors[shade];
}

u16 PPU::readColorPalette(u8 paletteIndex, u8 colorIndex) const {
    const std::array<u8, 64>& paletteData = (paletteIndex < 8) ? m_bgPaletteData : m_objPaletteData;
    u8 actualPaletteIndex = paletteIndex & 0x07;
    u16 offset = (actualPaletteIndex * 8) + (colorIndex * 2);
    
    u16 color = paletteData[offset] | (paletteData[offset + 1] << 8);
    return color;
}

u32 PPU::convertGBCColorToRGB(u16 color) const {
    // GBC color format: 0BBBBBGGGGGRRRRR
    u8 r = (color & 0x1F);
    u8 g = ((color >> 5) & 0x1F);
    u8 b = ((color >> 10) & 0x1F);
    
    // Convert from 5-bit to 8-bit
    r = (r << 3) | (r >> 2);
    g = (g << 3) | (g >> 2);
    b = (b << 3) | (b >> 2);
    
    return 0xFF000000 | (r << 16) | (g << 8) | b;
}

void PPU::performHDMA() {
    // Transfer 0x10 bytes per HBlank
    if (m_hdmaRemaining > 0) {
        // Transfer 16 bytes
        for (u8 i = 0; i < 0x10; i++) {
            if (m_mmu) {
                u8 data = m_mmu->read(m_hdmaSource);
                writeVRAM(m_hdmaDest, data);
                m_hdmaSource++;
                m_hdmaDest++;
            }
        }
        
        // HBlank DMA: ~8 cycles per byte in double speed, 16 in normal
        // 16 bytes * 8 = 128 cycles base (double speed)
        u32 speedMultiplier = (m_mmu && m_mmu->isDoubleSpeed()) ? 1 : 2;
        m_dmaCycles += 128 * speedMultiplier;
        
        m_hdmaRemaining--;
        
        // Update HDMA5 register
        m_hdma5 = m_hdmaRemaining - 1;
        
        if (m_hdmaRemaining == 0) {
            m_hdmaActive = false;
            m_hdma5 = 0xFF;
        }
    }
}

void PPU::performGDMA() {
    // Transfer all at once (general purpose DMA)
    u16 length = m_hdmaRemaining * 0x10;
    
    for (u16 i = 0; i < length; i++) {
        if (m_mmu) {
            u8 data = m_mmu->read(m_hdmaSource);
            writeVRAM(m_hdmaDest, data);
            m_hdmaSource++;
            m_hdmaDest++;
        }
    }
    
    // General DMA: ~8 cycles per byte in double speed, 16 in normal
    u32 speedMultiplier = (m_mmu && m_mmu->isDoubleSpeed()) ? 1 : 2;
    m_dmaCycles += (length * 8) * speedMultiplier;
    
    m_hdmaRemaining = 0;
    m_hdmaActive = false;
    m_hdma5 = 0xFF;
}

u8 PPU::readVRAM(u16 address) const {
    u16 offset = address - 0x8000;
    
    // If accessing during drawing mode, return 0xFF (VRAM not accessible)
    if (m_mode == PPUMode::DRAWING && (m_lcdc & LCDC_LCD_ENABLE)) {
        return 0xFF;
    }
    
    // Add bank offset for GBC
    if (m_vramBank == 1 && m_gbcMode) {
        offset += 0x2000;
    }
    
    return m_vram[offset];
}

void PPU::writeVRAM(u16 address, u8 value) {
    u16 offset = address - 0x8000;
    
    // If accessing during drawing mode, ignore write (VRAM not accessible)
    if (m_mode == PPUMode::DRAWING && (m_lcdc & LCDC_LCD_ENABLE)) {
        return;
    }
    
    // Add bank offset for GBC
    if (m_vramBank == 1 && m_gbcMode) {
        offset += 0x2000;
    }
    
    m_vram[offset] = value;
}

u8 PPU::readOAM(u16 address) const {
    u16 offset = address - 0xFE00;
    
    // If accessing during OAM scan or drawing, return 0xFF (OAM not accessible)
    if ((m_mode == PPUMode::OAM_SCAN || m_mode == PPUMode::DRAWING) && (m_lcdc & LCDC_LCD_ENABLE)) {
        return 0xFF;
    }
    
    return m_oam[offset];
}

void PPU::writeOAM(u16 address, u8 value) {
    u16 offset = address - 0xFE00;
    
    // If accessing during OAM scan or drawing, ignore write (OAM not accessible)
    if ((m_mode == PPUMode::OAM_SCAN || m_mode == PPUMode::DRAWING) && (m_lcdc & LCDC_LCD_ENABLE)) {
        return;
    }
    
    m_oam[offset] = value;
}

u8 PPU::readRegister(u16 address) const {
    switch (address) {
        case 0xFF40: return m_lcdc;
        case 0xFF41: return m_stat | 0x80; // Bit 7 is always 1
        case 0xFF42: return m_scy;
        case 0xFF43: return m_scx;
        case 0xFF44: return m_ly;
        case 0xFF45: return m_lyc;
        case 0xFF46: return m_dma;
        case 0xFF47: return m_bgp;
        case 0xFF48: return m_obp0;
        case 0xFF49: return m_obp1;
        case 0xFF4A: return m_wy;
        case 0xFF4B: return m_wx;
        case 0xFF4F: return m_vramBank | 0xFE; // Only bit 0 is used
        case 0xFF51: return m_hdma1;
        case 0xFF52: return m_hdma2;
        case 0xFF53: return m_hdma3;
        case 0xFF54: return m_hdma4;
        case 0xFF55: return m_hdma5;
        case 0xFF68: return m_bgpi | 0x40; // Bit 6 is always 1
        case 0xFF69: {
            u8 index = m_bgpi & 0x3F;
            return m_bgPaletteData[index];
        }
        case 0xFF6A: return m_obpi | 0x40; // Bit 6 is always 1
        case 0xFF6B: {
            u8 index = m_obpi & 0x3F;
            return m_objPaletteData[index];
        }
        default: return 0xFF;
    }
}

void PPU::writeRegister(u16 address, u8 value) {
    switch (address) {
        case 0xFF40: {
            bool wasEnabled = (m_lcdc & LCDC_LCD_ENABLE) != 0;
            m_lcdc = value;
            bool isEnabled = (m_lcdc & LCDC_LCD_ENABLE) != 0;
            
            // If LCD is turned off, reset to beginning of frame
            if (wasEnabled && !isEnabled) {
                m_ly = 0;
                m_modeCycles = 0;
                setMode(PPUMode::HBLANK);
                m_stat &= ~STAT_LYC_EQUAL;
                m_windowLineCounter = 0;
                m_windowRenderedThisFrame = false;
            }
            break;
        }
        case 0xFF41:
            // Only bits 3-6 are writable
            m_stat = (m_stat & 0x87) | (value & 0x78);
            updateStatInterrupt();
            break;
        case 0xFF42: m_scy = value; break;
        case 0xFF43: m_scx = value; break;
        case 0xFF44: /* LY is read-only */ break;
        case 0xFF45:
            m_lyc = value;
            // Check LYC=LY immediately
            if (m_ly == m_lyc) {
                m_stat |= STAT_LYC_EQUAL;
            } else {
                m_stat &= ~STAT_LYC_EQUAL;
            }
            updateStatInterrupt();
            break;
        case 0xFF46:
            // OAM DMA transfer
            m_dma = value;
            if (m_mmu) {
                u16 source = value << 8;
                for (u16 i = 0; i < 0xA0; i++) {
                    writeOAM(0xFE00 + i, m_mmu->read(source + i));
                }
                
                // Calculate DMA cycle cost adjusted for double speed
                // Normal speed: 8 initial + 160 bytes * 4 cycles = 648 cycles
                // Then multiply by (2 - doubleSpeed) to get actual cycles:
                // Normal: 648 * 2 = 1296 cycles
                // Double: 648 * 1 = 648 cycles
                u32 speedMultiplier = (m_mmu && m_mmu->isDoubleSpeed()) ? 1 : 2;
                u32 baseCycles = 8 + (160 * 4);  // 648 cycles (double speed base)
                m_dmaCycles += baseCycles * speedMultiplier;
            }
            break;
        case 0xFF47: m_bgp = value; break;
        case 0xFF48: m_obp0 = value; break;
        case 0xFF49: m_obp1 = value; break;
        case 0xFF4A: m_wy = value; break;
        case 0xFF4B: m_wx = value; break;
        case 0xFF4F:
            if (m_gbcMode) {
                m_vramBank = value & 0x01;
            }
            break;
        case 0xFF51: m_hdma1 = value; break;
        case 0xFF52: m_hdma2 = value & 0xF0; break; // Lower 4 bits are ignored
        case 0xFF53: m_hdma3 = value & 0x1F; break; // Upper 3 bits are ignored
        case 0xFF54: m_hdma4 = value & 0xF0; break; // Lower 4 bits are ignored
        case 0xFF55:
            if (m_gbcMode) {
                bool isHDMA = (value & 0x80) != 0;
                
                // If HDMA is active and bit 7 is 0, terminate HDMA
                if (m_hdmaActive && !isHDMA) {
                    m_hdmaActive = false;
                    m_hdma5 = 0x80 | (m_hdmaRemaining - 1);
                    return;
                }
                
                // Setup DMA parameters
                m_hdmaSource = ((m_hdma1 << 8) | m_hdma2) & 0xFFF0;
                m_hdmaDest = 0x8000 | (((m_hdma3 << 8) | m_hdma4) & 0x1FF0);
                m_hdmaRemaining = (value & 0x7F) + 1;
                
                if (isHDMA) {
                    // HBlank DMA
                    m_hdmaActive = true;
                    m_hdma5 = value & 0x7F;
                } else {
                    // General Purpose DMA
                    performGDMA();
                }
            }
            break;
        case 0xFF68:
            m_bgpi = value;
            break;
        case 0xFF69: {
            u8 index = m_bgpi & 0x3F;
            m_bgPaletteData[index] = value;
            
            // Auto-increment if bit 7 is set
            if (m_bgpi & 0x80) {
                m_bgpi = 0x80 | ((index + 1) & 0x3F);
            }
            break;
        }
        case 0xFF6A:
            m_obpi = value;
            break;
        case 0xFF6B: {
            u8 index = m_obpi & 0x3F;
            m_objPaletteData[index] = value;
            
            // Auto-increment if bit 7 is set
            if (m_obpi & 0x80) {
                m_obpi = 0x80 | ((index + 1) & 0x3F);
            }
            break;
        }
    }
}

void PPU::saveState(std::ofstream& file) const {
    file.write(reinterpret_cast<const char*>(m_vram.data()), m_vram.size());
    file.write(reinterpret_cast<const char*>(m_oam.data()), m_oam.size());
    
    file.write(reinterpret_cast<const char*>(&m_vramBank), sizeof(m_vramBank));
    file.write(reinterpret_cast<const char*>(&m_lcdc), sizeof(m_lcdc));
    file.write(reinterpret_cast<const char*>(&m_stat), sizeof(m_stat));
    file.write(reinterpret_cast<const char*>(&m_scy), sizeof(m_scy));
    file.write(reinterpret_cast<const char*>(&m_scx), sizeof(m_scx));
    file.write(reinterpret_cast<const char*>(&m_ly), sizeof(m_ly));
    file.write(reinterpret_cast<const char*>(&m_lyc), sizeof(m_lyc));
    file.write(reinterpret_cast<const char*>(&m_dma), sizeof(m_dma));
    file.write(reinterpret_cast<const char*>(&m_bgp), sizeof(m_bgp));
    file.write(reinterpret_cast<const char*>(&m_obp0), sizeof(m_obp0));
    file.write(reinterpret_cast<const char*>(&m_obp1), sizeof(m_obp1));
    file.write(reinterpret_cast<const char*>(&m_wy), sizeof(m_wy));
    file.write(reinterpret_cast<const char*>(&m_wx), sizeof(m_wx));
    
    file.write(reinterpret_cast<const char*>(&m_bgpi), sizeof(m_bgpi));
    file.write(reinterpret_cast<const char*>(&m_obpi), sizeof(m_obpi));
    file.write(reinterpret_cast<const char*>(m_bgPaletteData.data()), m_bgPaletteData.size());
    file.write(reinterpret_cast<const char*>(m_objPaletteData.data()), m_objPaletteData.size());
    
    file.write(reinterpret_cast<const char*>(&m_hdma1), sizeof(m_hdma1));
    file.write(reinterpret_cast<const char*>(&m_hdma2), sizeof(m_hdma2));
    file.write(reinterpret_cast<const char*>(&m_hdma3), sizeof(m_hdma3));
    file.write(reinterpret_cast<const char*>(&m_hdma4), sizeof(m_hdma4));
    file.write(reinterpret_cast<const char*>(&m_hdma5), sizeof(m_hdma5));
    file.write(reinterpret_cast<const char*>(&m_hdmaActive), sizeof(m_hdmaActive));
    file.write(reinterpret_cast<const char*>(&m_hdmaSource), sizeof(m_hdmaSource));
    file.write(reinterpret_cast<const char*>(&m_hdmaDest), sizeof(m_hdmaDest));
    file.write(reinterpret_cast<const char*>(&m_hdmaRemaining), sizeof(m_hdmaRemaining));
    
    u8 modeValue = static_cast<u8>(m_mode);
    file.write(reinterpret_cast<const char*>(&modeValue), sizeof(modeValue));
    file.write(reinterpret_cast<const char*>(&m_modeCycles), sizeof(m_modeCycles));
    file.write(reinterpret_cast<const char*>(&m_windowLineCounter), sizeof(m_windowLineCounter));
    file.write(reinterpret_cast<const char*>(&m_windowRenderedThisFrame), sizeof(m_windowRenderedThisFrame));
    file.write(reinterpret_cast<const char*>(&m_gbcMode), sizeof(m_gbcMode));
    file.write(reinterpret_cast<const char*>(&m_statInterruptLine), sizeof(m_statInterruptLine));
    file.write(reinterpret_cast<const char*>(&m_modeChangeDelay), sizeof(m_modeChangeDelay));
    file.write(reinterpret_cast<const char*>(&m_dmaCycles), sizeof(m_dmaCycles));
}

void PPU::loadState(std::ifstream& file) {
    file.read(reinterpret_cast<char*>(m_vram.data()), m_vram.size());
    file.read(reinterpret_cast<char*>(m_oam.data()), m_oam.size());
    
    file.read(reinterpret_cast<char*>(&m_vramBank), sizeof(m_vramBank));
    file.read(reinterpret_cast<char*>(&m_lcdc), sizeof(m_lcdc));
    file.read(reinterpret_cast<char*>(&m_stat), sizeof(m_stat));
    file.read(reinterpret_cast<char*>(&m_scy), sizeof(m_scy));
    file.read(reinterpret_cast<char*>(&m_scx), sizeof(m_scx));
    file.read(reinterpret_cast<char*>(&m_ly), sizeof(m_ly));
    file.read(reinterpret_cast<char*>(&m_lyc), sizeof(m_lyc));
    file.read(reinterpret_cast<char*>(&m_dma), sizeof(m_dma));
    file.read(reinterpret_cast<char*>(&m_bgp), sizeof(m_bgp));
    file.read(reinterpret_cast<char*>(&m_obp0), sizeof(m_obp0));
    file.read(reinterpret_cast<char*>(&m_obp1), sizeof(m_obp1));
    file.read(reinterpret_cast<char*>(&m_wy), sizeof(m_wy));
    file.read(reinterpret_cast<char*>(&m_wx), sizeof(m_wx));
    
    file.read(reinterpret_cast<char*>(&m_bgpi), sizeof(m_bgpi));
    file.read(reinterpret_cast<char*>(&m_obpi), sizeof(m_obpi));
    file.read(reinterpret_cast<char*>(m_bgPaletteData.data()), m_bgPaletteData.size());
    file.read(reinterpret_cast<char*>(m_objPaletteData.data()), m_objPaletteData.size());
    
    file.read(reinterpret_cast<char*>(&m_hdma1), sizeof(m_hdma1));
    file.read(reinterpret_cast<char*>(&m_hdma2), sizeof(m_hdma2));
    file.read(reinterpret_cast<char*>(&m_hdma3), sizeof(m_hdma3));
    file.read(reinterpret_cast<char*>(&m_hdma4), sizeof(m_hdma4));
    file.read(reinterpret_cast<char*>(&m_hdma5), sizeof(m_hdma5));
    file.read(reinterpret_cast<char*>(&m_hdmaActive), sizeof(m_hdmaActive));
    file.read(reinterpret_cast<char*>(&m_hdmaSource), sizeof(m_hdmaSource));
    file.read(reinterpret_cast<char*>(&m_hdmaDest), sizeof(m_hdmaDest));
    file.read(reinterpret_cast<char*>(&m_hdmaRemaining), sizeof(m_hdmaRemaining));
    
    u8 modeValue;
    file.read(reinterpret_cast<char*>(&modeValue), sizeof(modeValue));
    m_mode = static_cast<PPUMode>(modeValue);
    file.read(reinterpret_cast<char*>(&m_modeCycles), sizeof(m_modeCycles));
    file.read(reinterpret_cast<char*>(&m_windowLineCounter), sizeof(m_windowLineCounter));
    file.read(reinterpret_cast<char*>(&m_windowRenderedThisFrame), sizeof(m_windowRenderedThisFrame));
    file.read(reinterpret_cast<char*>(&m_gbcMode), sizeof(m_gbcMode));
    file.read(reinterpret_cast<char*>(&m_statInterruptLine), sizeof(m_statInterruptLine));
    file.read(reinterpret_cast<char*>(&m_modeChangeDelay), sizeof(m_modeChangeDelay));
    file.read(reinterpret_cast<char*>(&m_dmaCycles), sizeof(m_dmaCycles));
}

} // namespace gb


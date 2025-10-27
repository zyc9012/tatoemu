#include "ppu.h"
#include "cpu.h"
#include "mmu.h"
#include <algorithm>
#include <cstring>

PPU::PPU()
    : m_cpu(nullptr)
    , m_mmu(nullptr)
    , m_vramBank(0)
    , m_lcdc(0x91)
    , m_stat(0)
    , m_scy(0)
    , m_scx(0)
    , m_ly(0)
    , m_lyc(0)
    , m_dma(0)
    , m_bgp(0xFC)
    , m_obp0(0xFF)
    , m_obp1(0xFF)
    , m_wy(0)
    , m_wx(0)
    , m_bcps(0)
    , m_ocps(0)
    , m_hdma1(0), m_hdma2(0), m_hdma3(0), m_hdma4(0), m_hdma5(0xFF)
    , m_hdmaActive(false)
    , m_hdmaSource(0)
    , m_hdmaDest(0)
    , m_hdmaRemaining(0)
    , m_mode(MODE_OAM_SCAN)
    , m_modeCycles(0)
    , m_frameReady(false)
    , m_gbcMode(false) {
    std::fill(m_vram.begin(), m_vram.end(), 0);
    std::fill(m_oam.begin(), m_oam.end(), 0);
    std::fill(m_framebuffer.begin(), m_framebuffer.end(), 0xFFFFFFFF);
    std::fill(m_scanlineBuffer.begin(), m_scanlineBuffer.end(), 0xFFFFFFFF);
    std::fill(m_bgPaletteRAM.begin(), m_bgPaletteRAM.end(), 0xFF);
    std::fill(m_objPaletteRAM.begin(), m_objPaletteRAM.end(), 0xFF);
}

PPU::~PPU() {
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
    file.write(reinterpret_cast<const char*>(&m_bcps), sizeof(m_bcps));
    file.write(reinterpret_cast<const char*>(&m_ocps), sizeof(m_ocps));
    file.write(reinterpret_cast<const char*>(m_bgPaletteRAM.data()), m_bgPaletteRAM.size());
    file.write(reinterpret_cast<const char*>(m_objPaletteRAM.data()), m_objPaletteRAM.size());
    file.write(reinterpret_cast<const char*>(&m_hdma1), sizeof(m_hdma1));
    file.write(reinterpret_cast<const char*>(&m_hdma2), sizeof(m_hdma2));
    file.write(reinterpret_cast<const char*>(&m_hdma3), sizeof(m_hdma3));
    file.write(reinterpret_cast<const char*>(&m_hdma4), sizeof(m_hdma4));
    file.write(reinterpret_cast<const char*>(&m_hdma5), sizeof(m_hdma5));
    file.write(reinterpret_cast<const char*>(&m_hdmaActive), sizeof(m_hdmaActive));
    file.write(reinterpret_cast<const char*>(&m_hdmaSource), sizeof(m_hdmaSource));
    file.write(reinterpret_cast<const char*>(&m_hdmaDest), sizeof(m_hdmaDest));
    file.write(reinterpret_cast<const char*>(&m_hdmaRemaining), sizeof(m_hdmaRemaining));
    file.write(reinterpret_cast<const char*>(&m_mode), sizeof(m_mode));
    file.write(reinterpret_cast<const char*>(&m_modeCycles), sizeof(m_modeCycles));
    file.write(reinterpret_cast<const char*>(&m_gbcMode), sizeof(m_gbcMode));
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
    file.read(reinterpret_cast<char*>(&m_bcps), sizeof(m_bcps));
    file.read(reinterpret_cast<char*>(&m_ocps), sizeof(m_ocps));
    file.read(reinterpret_cast<char*>(m_bgPaletteRAM.data()), m_bgPaletteRAM.size());
    file.read(reinterpret_cast<char*>(m_objPaletteRAM.data()), m_objPaletteRAM.size());
    file.read(reinterpret_cast<char*>(&m_hdma1), sizeof(m_hdma1));
    file.read(reinterpret_cast<char*>(&m_hdma2), sizeof(m_hdma2));
    file.read(reinterpret_cast<char*>(&m_hdma3), sizeof(m_hdma3));
    file.read(reinterpret_cast<char*>(&m_hdma4), sizeof(m_hdma4));
    file.read(reinterpret_cast<char*>(&m_hdma5), sizeof(m_hdma5));
    file.read(reinterpret_cast<char*>(&m_hdmaActive), sizeof(m_hdmaActive));
    file.read(reinterpret_cast<char*>(&m_hdmaSource), sizeof(m_hdmaSource));
    file.read(reinterpret_cast<char*>(&m_hdmaDest), sizeof(m_hdmaDest));
    file.read(reinterpret_cast<char*>(&m_hdmaRemaining), sizeof(m_hdmaRemaining));
    file.read(reinterpret_cast<char*>(&m_mode), sizeof(m_mode));
    file.read(reinterpret_cast<char*>(&m_modeCycles), sizeof(m_modeCycles));
    file.read(reinterpret_cast<char*>(&m_gbcMode), sizeof(m_gbcMode));
    m_frameReady = false;
}

void PPU::setCPU(CPU* cpu) {
    m_cpu = cpu;
}

void PPU::setMMU(MMU* mmu) {
    m_mmu = mmu;
}

void PPU::reset() {
    m_vramBank = 0;
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
    m_bcps = 0;
    m_ocps = 0;
    m_hdma1 = m_hdma2 = m_hdma3 = m_hdma4 = 0;
    m_hdma5 = 0xFF;
    m_hdmaActive = false;
    m_mode = MODE_OAM_SCAN;
    m_modeCycles = 0;
    m_frameReady = false;
}

void PPU::setGBCMode(bool enabled) {
    m_gbcMode = enabled;
}

void PPU::step(u32 cycles) {
    if (!(m_lcdc & 0x80)) {
        // LCD is off
        return;
    }

    m_modeCycles += cycles;

    switch (m_mode) {
        case MODE_OAM_SCAN:
            if (m_modeCycles >= 80) {
                m_modeCycles -= 80;
                setMode(MODE_DRAWING);
            }
            break;

        case MODE_DRAWING:
            if (m_modeCycles >= 172) {
                m_modeCycles -= 172;
                renderScanline();
                setMode(MODE_HBLANK);
                
                // STAT interrupt for Mode 0 (H-Blank)
                if (m_stat & 0x08) {
                    if (m_cpu) {
                        m_cpu->requestInterrupt(INT_LCD_STAT);
                    }
                }
            }
            break;

        case MODE_HBLANK:
            // Perform HDMA during H-Blank if active
            if (m_gbcMode && m_hdmaActive) {
                performHDMA();
            }
            
            if (m_modeCycles >= 204) {
                m_modeCycles -= 204;
                m_ly++;

                if (m_ly == 144) {
                    setMode(MODE_VBLANK);
                    m_frameReady = true;
                    
                    // V-Blank interrupt
                    if (m_cpu) {
                        m_cpu->requestInterrupt(INT_VBLANK);
                    }
                    
                    // STAT interrupt for Mode 1 (V-Blank)
                    if (m_stat & 0x10) {
                        if (m_cpu) {
                            m_cpu->requestInterrupt(INT_LCD_STAT);
                        }
                    }
                } else {
                    setMode(MODE_OAM_SCAN);
                    
                    // STAT interrupt for Mode 2 (OAM Scan)
                    if (m_stat & 0x20) {
                        if (m_cpu) {
                            m_cpu->requestInterrupt(INT_LCD_STAT);
                        }
                    }
                }

                // LYC=LY coincidence check
                if (m_ly == m_lyc) {
                    m_stat |= 0x04;
                    if (m_stat & 0x40) {
                        if (m_cpu) {
                            m_cpu->requestInterrupt(INT_LCD_STAT);
                        }
                    }
                } else {
                    m_stat &= ~0x04;
                }
            }
            break;

        case MODE_VBLANK:
            if (m_modeCycles >= 456) {
                m_modeCycles -= 456;
                m_ly++;

                if (m_ly > 153) {
                    m_ly = 0;
                    setMode(MODE_OAM_SCAN);
                    
                    // STAT interrupt for Mode 2 (OAM Scan)
                    if (m_stat & 0x20) {
                        if (m_cpu) {
                            m_cpu->requestInterrupt(INT_LCD_STAT);
                        }
                    }
                }

                // LYC=LY coincidence check
                if (m_ly == m_lyc) {
                    m_stat |= 0x04;
                    if (m_stat & 0x40) {
                        if (m_cpu) {
                            m_cpu->requestInterrupt(INT_LCD_STAT);
                        }
                    }
                } else {
                    m_stat &= ~0x04;
                }
            }
            break;
    }
}

void PPU::setMode(PPUMode mode) {
    m_mode = mode;
    m_stat = (m_stat & 0xFC) | mode;
}

void PPU::renderScanline() {
    std::fill(m_scanlineBuffer.begin(), m_scanlineBuffer.end(), 0xFFFFFFFF);

    if (m_lcdc & 0x01) {
        renderBackground(m_ly);
    }

    if (m_lcdc & 0x20) {
        renderWindow(m_ly);
    }

    if (m_lcdc & 0x02) {
        renderSprites(m_ly);
    }

    // Copy scanline to framebuffer
    std::memcpy(&m_framebuffer[m_ly * SCREEN_WIDTH], m_scanlineBuffer.data(), 
                SCREEN_WIDTH * sizeof(u32));
}

void PPU::renderBackground(u8 line) {
    u8 scrollY = m_scy;
    u8 scrollX = m_scx;
    
    u8 y = scrollY + line;
    u8 tileRow = (y / 8) & 0x1F;  // Wrap to 32 tiles (0-31)
    u8 tileY = y % 8;

    // Determine tile map address
    u16 tileMapAddr = (m_lcdc & 0x08) ? 0x1C00 : 0x1800;

    for (u8 x = 0; x < SCREEN_WIDTH; x++) {
        u8 scrolledX = scrollX + x;
        u8 tileCol = (scrolledX / 8) & 0x1F;  // Wrap to 32 tiles (0-31)
        u8 tileX = scrolledX % 8;

        // Get tile number from tile map
        u16 tileMapIndex = tileRow * 32 + tileCol;
        u8 tileNum = m_vram[tileMapAddr + tileMapIndex];

        // GBC: Get tile attributes from VRAM bank 1
        u8 tileAttr = 0;
        if (m_gbcMode) {
            tileAttr = m_vram[0x2000 + tileMapAddr + tileMapIndex];
        }

        // Determine tile data address (relative to VRAM start)
        u16 tileDataAddr;
        if (m_lcdc & 0x10) {
            // Unsigned addressing: tiles at 0x8000-0x8FFF (VRAM 0x0000-0x0FFF)
            tileDataAddr = tileNum * 16;
        } else {
            // Signed addressing: tiles at 0x8800-0x97FF, base at 0x9000 (VRAM 0x0800-0x17FF, base 0x1000)
            s8 signedTileNum = static_cast<s8>(tileNum);
            tileDataAddr = 0x1000 + (signedTileNum * 16);
        }

        // GBC: Check if tile is in VRAM bank 1
        if (m_gbcMode && (tileAttr & 0x08)) {
            tileDataAddr += 0x2000;
        }

        // Handle Y flip
        u8 effectiveTileY = tileY;
        if (m_gbcMode && (tileAttr & 0x40)) {
            effectiveTileY = 7 - tileY;
        }

        // Get tile data
        u16 tileLineAddr = tileDataAddr + (effectiveTileY * 2);
        u8 byte1 = m_vram[tileLineAddr];
        u8 byte2 = m_vram[tileLineAddr + 1];

        // Extract color (handle X flip)
        u8 colorBit = tileX;
        if (m_gbcMode && (tileAttr & 0x20)) {
            colorBit = 7 - colorBit; // X flip
        } else {
            colorBit = 7 - colorBit;
        }
        u8 colorId = ((byte2 >> colorBit) & 1) << 1 | ((byte1 >> colorBit) & 1);

        if (m_gbcMode) {
            u8 paletteIndex = tileAttr & 0x07;
            m_scanlineBuffer[x] = getGBCColor(paletteIndex, colorId, false);
        } else {
            m_scanlineBuffer[x] = getColor(colorId, m_bgp);
        }
    }
}

void PPU::renderWindow(u8 line) {
    if (line < m_wy) {
        return;
    }

    u8 windowY = line - m_wy;
    u8 tileRow = (windowY / 8) & 0x1F;  // Wrap to 32 tiles (0-31)
    u8 tileY = windowY % 8;

    // Determine tile map address
    u16 tileMapAddr = (m_lcdc & 0x40) ? 0x1C00 : 0x1800;

    for (u8 x = 0; x < SCREEN_WIDTH; x++) {
        if (x + 7 < m_wx) {
            continue;
        }

        u8 windowX = x + 7 - m_wx;
        u8 tileCol = (windowX / 8) & 0x1F;  // Wrap to 32 tiles (0-31)
        u8 tileX = windowX % 8;

        // Get tile number from tile map
        u16 tileMapIndex = tileRow * 32 + tileCol;
        u8 tileNum = m_vram[tileMapAddr + tileMapIndex];

        // GBC: Get tile attributes from VRAM bank 1
        u8 tileAttr = 0;
        if (m_gbcMode) {
            tileAttr = m_vram[0x2000 + tileMapAddr + tileMapIndex];
        }

        // Determine tile data address (relative to VRAM start)
        u16 tileDataAddr;
        if (m_lcdc & 0x10) {
            // Unsigned addressing: tiles at 0x8000-0x8FFF (VRAM 0x0000-0x0FFF)
            tileDataAddr = tileNum * 16;
        } else {
            // Signed addressing: tiles at 0x8800-0x97FF, base at 0x9000 (VRAM 0x0800-0x17FF, base 0x1000)
            s8 signedTileNum = static_cast<s8>(tileNum);
            tileDataAddr = 0x1000 + (signedTileNum * 16);
        }

        // GBC: Check if tile is in VRAM bank 1
        if (m_gbcMode && (tileAttr & 0x08)) {
            tileDataAddr += 0x2000;
        }

        // Handle Y flip
        u8 effectiveTileY = tileY;
        if (m_gbcMode && (tileAttr & 0x40)) {
            effectiveTileY = 7 - tileY;
        }

        // Get tile data
        u16 tileLineAddr = tileDataAddr + (effectiveTileY * 2);
        u8 byte1 = m_vram[tileLineAddr];
        u8 byte2 = m_vram[tileLineAddr + 1];

        // Extract color (handle X flip)
        u8 colorBit = tileX;
        if (m_gbcMode && (tileAttr & 0x20)) {
            colorBit = 7 - colorBit; // X flip
        } else {
            colorBit = 7 - colorBit;
        }
        u8 colorId = ((byte2 >> colorBit) & 1) << 1 | ((byte1 >> colorBit) & 1);

        if (m_gbcMode) {
            u8 paletteIndex = tileAttr & 0x07;
            m_scanlineBuffer[x] = getGBCColor(paletteIndex, colorId, false);
        } else {
            m_scanlineBuffer[x] = getColor(colorId, m_bgp);
        }
    }
}

void PPU::renderSprites(u8 line) {
    u8 spriteHeight = (m_lcdc & 0x04) ? 16 : 8;

    // Scan all 40 sprites
    for (int sprite = 0; sprite < 40; sprite++) {
        u8 index = sprite * 4;
        u8 spriteY = m_oam[index] - 16;
        u8 spriteX = m_oam[index + 1] - 8;
        u8 tileNum = m_oam[index + 2];
        u8 attributes = m_oam[index + 3];

        // Check if sprite is on this scanline
        if (line < spriteY || line >= spriteY + spriteHeight) {
            continue;
        }

        // Determine which line of the sprite to render
        u8 tileY = line - spriteY;
        
        // Y flip
        if (attributes & 0x40) {
            tileY = spriteHeight - 1 - tileY;
        }

        // Get tile data
        u16 tileAddr = tileNum * 16 + tileY * 2;
        
        // GBC: Check if sprite uses VRAM bank 1 (bit 3 of attributes)
        if (m_gbcMode && (attributes & 0x08)) {
            tileAddr += 0x2000;
        }
        
        u8 byte1 = m_vram[tileAddr];
        u8 byte2 = m_vram[tileAddr + 1];

        // Render 8 pixels
        for (u8 x = 0; x < 8; x++) {
            u8 pixelX = spriteX + x;
            
            if (pixelX >= SCREEN_WIDTH) {
                continue;
            }

            // X flip
            u8 colorBit = (attributes & 0x20) ? x : (7 - x);
            u8 colorId = ((byte2 >> colorBit) & 1) << 1 | ((byte1 >> colorBit) & 1);

            // Color 0 is transparent
            if (colorId == 0) {
                continue;
            }

            // Check priority
            bool priority = !(attributes & 0x80);
            if (!priority) {
                // Behind background, only render if background is color 0
                u32 bgColor = m_scanlineBuffer[pixelX];
                u32 transparentColor = m_gbcMode ? getGBCColor(0, 0, false) : getColor(0, m_bgp);
                if (bgColor != transparentColor) {
                    continue;
                }
            }

            if (m_gbcMode) {
                // GBC: use palette index from bits 0-2
                u8 paletteIndex = attributes & 0x07;
                m_scanlineBuffer[pixelX] = getGBCColor(paletteIndex, colorId, true);
            } else {
                // DMG: Select palette (bit 4)
                u8 palette = (attributes & 0x10) ? m_obp1 : m_obp0;
                m_scanlineBuffer[pixelX] = getColor(colorId, palette);
            }
        }
    }
}

u32 PPU::getColor(u8 colorId, u8 palette) const {
    u8 colorIndex = (palette >> (colorId * 2)) & 0x03;
    
    // GameBoy color palette
    static const u32 colors[4] = {
        0xFFEFFFDE,  // Lightest green
        0xFFADD794,  // Light green
        0xFF529273,  // Dark green
        0xFF183442   // Darkest green
    };
    
    return colors[colorIndex];
}

u8 PPU::readVRAM(u16 address) const {
    if (address >= 0x8000 && address < 0xA000) {
        u16 offset = address - 0x8000;
        // In GBC mode, VRAM bank 1 is at offset 0x2000
        if (m_gbcMode && m_vramBank == 1) {
            offset += 0x2000;
        }
        return m_vram[offset];
    }
    return 0xFF;
}

void PPU::writeVRAM(u16 address, u8 value) {
    if (address >= 0x8000 && address < 0xA000) {
        u16 offset = address - 0x8000;
        // In GBC mode, VRAM bank 1 is at offset 0x2000
        if (m_gbcMode && m_vramBank == 1) {
            offset += 0x2000;
        }
        m_vram[offset] = value;
    }
}

u8 PPU::readOAM(u16 address) const {
    if (address >= 0xFE00 && address < 0xFEA0) {
        return m_oam[address - 0xFE00];
    }
    return 0xFF;
}

void PPU::writeOAM(u16 address, u8 value) {
    if (address >= 0xFE00 && address < 0xFEA0) {
        m_oam[address - 0xFE00] = value;
    }
}

u8 PPU::readRegister(u16 address) const {
    switch (address) {
        case 0xFF40: return m_lcdc;
        case 0xFF41: return m_stat | 0x80;
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
        // GBC registers
        case 0xFF4F: return m_vramBank | 0xFE; // VRAM bank (only bit 0 valid)
        case 0xFF51: return m_hdma1;
        case 0xFF52: return m_hdma2;
        case 0xFF53: return m_hdma3;
        case 0xFF54: return m_hdma4;
        case 0xFF55: return m_hdma5;
        case 0xFF68: return m_bcps | 0x40; // BCPS - bit 6 always 1
        case 0xFF69: // BCPD - read from palette RAM
            if (m_gbcMode) {
                return m_bgPaletteRAM[m_bcps & 0x3F];
            }
            return 0xFF;
        case 0xFF6A: return m_ocps | 0x40; // OCPS - bit 6 always 1
        case 0xFF6B: // OCPD - read from palette RAM
            if (m_gbcMode) {
                return m_objPaletteRAM[m_ocps & 0x3F];
            }
            return 0xFF;
        default: return 0xFF;
    }
}

void PPU::writeRegister(u16 address, u8 value) {
    switch (address) {
        case 0xFF40:
            m_lcdc = value;
            if (!(value & 0x80)) {
                // LCD turned off, reset state
                m_ly = 0;
                m_modeCycles = 0;
                setMode(MODE_HBLANK);
            }
            break;
        case 0xFF41:
            m_stat = (m_stat & 0x07) | (value & 0xF8);
            break;
        case 0xFF42: m_scy = value; break;
        case 0xFF43: m_scx = value; break;
        case 0xFF44: m_ly = 0; break; // LY is read-only, writing resets it
        case 0xFF45: m_lyc = value; break;
        case 0xFF46:
            m_dma = value;
            performDMA(value);
            break;
        case 0xFF47: m_bgp = value; break;
        case 0xFF48: m_obp0 = value; break;
        case 0xFF49: m_obp1 = value; break;
        case 0xFF4A: m_wy = value; break;
        case 0xFF4B: m_wx = value; break;
        // GBC registers
        case 0xFF4F: // VRAM bank
            if (m_gbcMode) {
                m_vramBank = value & 0x01;
            }
            break;
        case 0xFF51: m_hdma1 = value; break;
        case 0xFF52: m_hdma2 = value; break;
        case 0xFF53: m_hdma3 = value; break;
        case 0xFF54: m_hdma4 = value; break;
        case 0xFF55: // HDMA5 - start DMA transfer
            if (m_gbcMode) {
                m_hdma5 = value;
                m_hdmaSource = (m_hdma1 << 8) | (m_hdma2 & 0xF0);
                m_hdmaDest = 0x8000 | ((m_hdma3 << 8) | (m_hdma4 & 0xF0));
                m_hdmaRemaining = ((value & 0x7F) + 1) * 0x10;
                
                if (value & 0x80) {
                    // H-Blank DMA
                    m_hdmaActive = true;
                } else {
                    // General purpose DMA - transfer immediately
                    m_hdmaActive = false;
                    for (u16 i = 0; i < m_hdmaRemaining && m_mmu; i++) {
                        u8 data = m_mmu->read(m_hdmaSource++);
                        writeVRAM(m_hdmaDest++, data);
                    }
                    m_hdma5 = 0xFF; // Mark as complete
                }
            }
            break;
        case 0xFF68: // BCPS - Background Color Palette Specification
            if (m_gbcMode) {
                m_bcps = value;
            }
            break;
        case 0xFF69: // BCPD - Background Color Palette Data
            if (m_gbcMode) {
                u8 index = m_bcps & 0x3F;
                m_bgPaletteRAM[index] = value;
                // Auto-increment if bit 7 is set
                if (m_bcps & 0x80) {
                    m_bcps = 0x80 | ((index + 1) & 0x3F);
                }
            }
            break;
        case 0xFF6A: // OCPS - Object Color Palette Specification
            if (m_gbcMode) {
                m_ocps = value;
            }
            break;
        case 0xFF6B: // OCPD - Object Color Palette Data
            if (m_gbcMode) {
                u8 index = m_ocps & 0x3F;
                m_objPaletteRAM[index] = value;
                // Auto-increment if bit 7 is set
                if (m_ocps & 0x80) {
                    m_ocps = 0x80 | ((index + 1) & 0x3F);
                }
            }
            break;
    }
}

void PPU::performDMA(u8 value) {
    if (!m_mmu) {
        return;
    }
    
    // DMA transfers 160 bytes from XX00-XX9F to OAM (0xFE00-0xFE9F)
    // where XX is the value written to the DMA register
    u16 sourceAddr = value * 0x100;
    
    for (u16 i = 0; i < 0xA0; i++) {
        m_oam[i] = m_mmu->read(sourceAddr + i);
    }
}

void PPU::performHDMA() {
    if (!m_mmu || !m_hdmaActive || m_hdmaRemaining == 0) {
        return;
    }
    
    // Transfer 0x10 bytes during H-Blank
    u16 bytesToTransfer = (m_hdmaRemaining >= 0x10) ? 0x10 : m_hdmaRemaining;
    
    for (u16 i = 0; i < bytesToTransfer; i++) {
        u8 data = m_mmu->read(m_hdmaSource++);
        writeVRAM(m_hdmaDest++, data);
    }
    
    m_hdmaRemaining -= bytesToTransfer;
    
    if (m_hdmaRemaining == 0) {
        m_hdmaActive = false;
        m_hdma5 = 0xFF; // Mark as complete
    } else {
        // Update HDMA5 with remaining length
        m_hdma5 = ((m_hdmaRemaining / 0x10) - 1) & 0x7F;
    }
}

u32 PPU::getGBCColor(u8 paletteIndex, u8 colorId, bool isSprite) const {
    // Get color from palette RAM (2 bytes per color, 4 colors per palette)
    const auto& paletteRAM = isSprite ? m_objPaletteRAM : m_bgPaletteRAM;
    u8 offset = (paletteIndex * 8) + (colorId * 2);
    
    u8 byte1 = paletteRAM[offset];
    u8 byte2 = paletteRAM[offset + 1];
    
    // Extract RGB555 color
    u16 color555 = byte1 | (byte2 << 8);
    u8 r = (color555 & 0x1F);
    u8 g = ((color555 >> 5) & 0x1F);
    u8 b = ((color555 >> 10) & 0x1F);
    
    // Convert from 5-bit to 8-bit color
    r = (r << 3) | (r >> 2);
    g = (g << 3) | (g >> 2);
    b = (b << 3) | (b >> 2);
    
    return 0xFF000000 | (r << 16) | (g << 8) | b;
}


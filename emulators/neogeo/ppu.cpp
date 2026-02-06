#include "ppu.h"
#include "cpu.h"
#include "cartridge.h"
#include "memory.h"
#include "db.h"
#include "../core.h"
#include <cstring>
#include <algorithm>
#include <iomanip>

namespace neogeo {

PPU::PPU()
    : m_cpu(nullptr)
    , m_cartridge(nullptr)
    , m_memory(nullptr)
    , m_videoDevice(nullptr)
    , m_scanline(0)
    , m_cycles(0)
    , m_spriteFrameSpeed(0)
    , m_spriteFrameTimer(0)
    , m_spriteFrame(0)
    , m_graphicsRamPointer(0)
    , m_graphicsRamModulo(0)
    , m_enableGraphics(true)
    , m_enableSprites(true)
    , m_enableText(true)
    , m_bankXPos(0)
    , m_bankYPos(0)
    , m_bankXZoom(0)
    , m_bankYZoom(0)
    , m_bankSize(0)
    , m_spriteTileMask(0)
    , m_maxSpriteTile(0)
    , m_screenWidth(320)
    , m_screenHeight(224)
    , m_sliceStart(0x10)  // First visible scanline (Neo Geo Y coordinate)
    , m_sliceEnd(0xF0)    // Last visible scanline + 1 (0x10 + 224 = 0xF0 = 240)
    , m_textBankMode(TextBankMode::NONE)
    , m_maxSpriteBank(0x17d)
{
    // Initialize framebuffer with default size, will be resized when ROM is loaded
    m_frameBuffer.resize(320 * 224, 0);
    m_graphicsRam.fill(0);
    m_palette.fill(0);
    m_bankLookupAddress.fill(0);
    m_bankLookupShift.fill(0);
}

void PPU::reset() {
    m_graphicsRam.fill(0);
    m_scanline = 0;
    m_cycles = 0;
    m_spriteFrameSpeed = 0;
    m_spriteFrameTimer = 0;
    m_spriteFrame = 0;
    m_graphicsRamPointer = 0;
    m_graphicsRamModulo = 0;
    m_bankXPos = 0;
    m_bankYPos = 0;
    m_bankXZoom = 0;
    m_bankYZoom = 0;
    m_bankSize = 0;

    if (m_cartridge) {
        m_screenWidth = m_cartridge->getGameInfo()->screenWidth;
        m_screenHeight = m_cartridge->getGameInfo()->screenHeight;
        m_frameBuffer.resize(m_screenWidth * m_screenHeight);
        std::fill(m_frameBuffer.begin(), m_frameBuffer.end(), 0);
    }
    
    // Initialize palette to black
    m_palette.fill(0xFF000000);

    // Load zoom ROM (256 bytes * 256 zoom levels = 64KB)
    m_zoomRom.resize(0x10000);
    for (u32 i = 0; i < 0x10000; i++) {
        m_zoomRom[i] = m_cartridge->readZoomROM8(i);
    }
    
    // Initialize sprite ROM attributes
    initSpriteROM();
    
    // Initialize text ROM attributes
    initTextROM();
    
    // Initialize text bank switching tables
    initTextBankSwitching();
}

void PPU::initSpriteROM() {
    if (!m_cartridge) {
        return;
    }
    
    u32 spriteRomSize = m_cartridge->getSpriteROMSize();
    if (spriteRomSize == 0) {
        return;
    }
    
    // Each tile is 128 bytes (16x16 pixels, 4bpp)
    u32 numTiles = spriteRomSize / 128;
    
    // Calculate tile mask (power of 2 - 1)
    m_maxSpriteTile = numTiles;
    m_spriteTileMask = 1;
    while (m_spriteTileMask < numTiles) {
        m_spriteTileMask <<= 1;
    }
    m_spriteTileMask--;
    
    // Initialize tile transparency attributes
    m_spriteTileAttrib.resize(m_spriteTileMask + 1, 0);
    
    // Scan all tiles to determine which are fully transparent
    for (u32 i = 0; i < m_maxSpriteTile; i++) {
        bool transparent = true;
        for (u32 j = 0; j < 128; j++) {
            if (m_cartridge->readSpriteROM8(i * 128 + j) != 0) {
                transparent = false;
                break;
            }
        }
        m_spriteTileAttrib[i] = transparent ? 1 : 0;
    }
    
    // Mark remaining tiles as transparent
    for (u32 i = m_maxSpriteTile; i <= m_spriteTileMask; i++) {
        m_spriteTileAttrib[i] = 1;
    }
}

void PPU::initTextROM() {
    if (!m_cartridge) {
        return;
    }
    
    u32 textRomSize = m_cartridge->getTextROMSize();
    
    // Decode text ROM tiles
    // Each tile in ROM is 32 bytes, we decode to 32 bytes (8x8 pixels, 4bpp, 2 pixels per byte)
    if (textRomSize > 0) {
        u32 numTiles = std::max(0x1000u, textRomSize / 32);  // At least 4096 tiles
        m_decodedText.resize(numTiles * 32, 0);
        m_textTileAttrib.resize(numTiles, 1);
        
        // Copy already-decoded text ROM from cartridge
        for (u32 i = 0; i < textRomSize; i++) {
            m_decodedText[i] = m_cartridge->readTextROM8(i);
        }
        
        // Build transparency attributes
        for (u32 i = 0; i < textRomSize / 32; i++) {
            bool transparent = true;
            for (u32 j = 0; j < 32; j++) {
                if (m_decodedText[i * 32 + j] != 0) {
                    transparent = false;
                    break;
                }
            }
            m_textTileAttrib[i] = transparent ? 1 : 0;
        }
    }
    
    // Decode BIOS text ROM
    u32 biosTextSize = 0x20000;  // 128KB BIOS text ROM
    m_decodedTextBios.resize(biosTextSize, 0);
    m_textTileAttribBios.resize(biosTextSize / 32, 1);
    
    for (u32 i = 0; i < biosTextSize; i++) {
        m_decodedTextBios[i] = m_cartridge->readBIOSText8(i);
    }
    
    // Build BIOS text transparency attributes
    for (u32 i = 0; i < biosTextSize / 32; i++) {
        bool transparent = true;
        for (u32 j = 0; j < 32; j++) {
            if (m_decodedTextBios[i * 32 + j] != 0) {
                transparent = false;
                break;
            }
        }
        m_textTileAttribBios[i] = transparent ? 1 : 0;
    }
}

void PPU::initTextBankSwitching() {
    if (!m_cartridge) {
        m_textBankMode = TextBankMode::NONE;
        return;
    }
    
    u32 textRomSize = m_cartridge->getTextROMSize();
    const GameInfo* gameInfo = m_cartridge->getGameInfo();
    
    // Check if text ROM bank switching is needed (text ROM > 256KB)
    if (textRomSize <= 0x40000) {
        m_textBankMode = TextBankMode::NONE;
        return;
    }
    
    // Determine bank switching mode based on game flags
    if (gameInfo && (gameInfo->flags & GAME_FLAG_ALTERNATE_TEXT)) {
        m_textBankMode = TextBankMode::ALTERNATE;
        
        // Precompute bank lookup tables for ALTERNATE_TEXT mode (KOF2000 style)
        // Bank info is stored at 0xEA00, with 2 bits per tile, 6 tiles per word
        for (u32 x = 0; x < 40; x++) {
            m_bankLookupAddress[x] = (x / 6) << 6;  // (x / 6) * 64, byte offset for column group
            m_bankLookupShift[x] = (5 - (x % 6)) << 1;  // bit shift position for 2-bit bank value
        }
    } else {
        m_textBankMode = TextBankMode::STANDARD;
        // Standard mode doesn't use lookup tables; bank is determined per-row at render time
    }
}

void PPU::step(u32 cycles) {
    m_cycles += cycles;
    
    if (m_cycles >= CPU_CYCLES_PER_SCANLINE) {
        m_cycles -= CPU_CYCLES_PER_SCANLINE;
        m_scanline++;

        if (m_scanline >= TOTAL_SCANLINES) {
            // Render the remaining sprites
            if (m_enableSprites) {
                renderSprites();
            }
            
            // Render text layer on top
            if (m_enableText) {
                renderText();
            }
            
            // Send frame to video device
            if (m_videoDevice) {
                m_videoDevice->render(m_frameBuffer.data());
            }

            m_scanline = 0;

            // Update sprite frame timing
            updateSpriteFrame();

            // Trigger VBlank interrupt
            m_memory->vblankIRQ();
        }
    }
}

void PPU::newFrame() {
    // Update palette from memory
    updatePalette();
    
    // Clear screen to backdrop color
    clearScreen();

    // Reset bank position for first sprite
    m_bankXPos = 0;
    m_bankYPos = 0;
    m_bankXZoom = 0;
    m_bankYZoom = 0;
    m_bankSize = 0;

    // Reset slice rendering
    m_sliceStart = 0x10;
    m_sliceEnd = 0xF0;
}

void PPU::clearScreen() {
    // Clear to backdrop color (palette entry 0x0FFF - the last palette entry)
    // This is the standard Neo Geo backdrop color register
    u32 backdropColor = m_palette[0x0FFF];
    std::fill(m_frameBuffer.begin(), m_frameBuffer.end(), backdropColor);
}

void PPU::updatePalette() {
    if (!m_memory) {
        return;
    }
    
    // Read palette bank control from memory
    bool darken = false;  // TODO: Get from I/O register
    
    // Convert all 4096 palette entries from 16-bit to 32-bit ARGB
    for (u32 i = 0; i < 4096; i++) {
        u16 palEntry = m_memory->readPalette16(i * 2);
        m_palette[i] = convertPaletteEntry(palEntry, darken);
    }
}

u32 PPU::convertPaletteEntry(u16 entry, bool /* darken */) {
    // Neo Geo palette format (16-bit):
    // Bits 11-8: Red (4 bits)
    // Bits 7-4: Green (4 bits)
    // Bits 3-0: Blue (4 bits)
    // Bit 14: Red bit 4
    // Bit 13: Green bit 4
    // Bit 12: Blue bit 4
    // Bit 15: Dark bit (contributes to bit 5 of each channel)
    //
    // Each color is 6 bits (0-63), with resistor network weighting
    
    // Extract 6-bit color values (stored in bits 7-2, bits 1-0 are zero)
    u32 r = (entry & 0x0F00) >> 4;  // Bits 11-8 -> bits 7-4
    r |= (entry >> 11) & 8;          // Bit 14 -> bit 3
    r |= (entry >> 13) & 4;          // Bit 15 (dark) -> bit 2
    
    u32 g = (entry & 0x00F0);        // Bits 7-4
    g |= (entry >> 10) & 8;          // Bit 13 -> bit 3
    g |= (entry >> 13) & 4;          // Bit 15 (dark) -> bit 2
    
    u32 b = (entry & 0x000F) << 4;   // Bits 3-0 -> bits 7-4
    b |= (entry >> 9) & 8;           // Bit 12 -> bit 3
    b |= (entry >> 13) & 4;          // Bit 15 (dark) -> bit 2
    
    // Scale from 6-bit (in bits 7-2) to 8-bit by replicating top bits to bottom
    r = r | (r >> 6);
    g = g | (g >> 6);
    b = b | (b >> 6);
    
    return 0xFF000000 | (r << 16) | (g << 8) | b;
}

void PPU::renderSprites() {
    if (!m_cartridge || m_spriteTileMask == 0 || m_sliceStart >= 0xF0) {
        return;
    }

    // Render to the current scanline
    m_sliceEnd = std::min((m_scanline + 248) % 264, static_cast<u32>(0xF0));

    // Calculate sprite bank limit for optimization
    calcSpriteBankLimit();
    
    // Render all sprite banks
    constexpr u32 MAX_SPRITE_BANKS = 0x17d;  // 381 sprite banks
    u32 numBanks = std::min(m_maxSpriteBank, MAX_SPRITE_BANKS);
    
    for (u32 bank = 0; bank < numBanks; bank++) {
        // Read sprite control block attributes
        // SCB2: Y position and size (0x10400 + bank * 2)
        // SCB3: X position (0x10800 + bank * 2)
        // SCB1: Tile data pointer (bank * 0x80)
        
        u16 attrib02 = readGraphicsRAM16(0x10400 + bank * 2);
        u16 attrib03 = readGraphicsRAM16(0x10800 + bank * 2);
        
        // Check if this is a chained sprite (bit 6 of attrib02)
        if (attrib02 & 0x40) {
            // Chained sprite - continues previous sprite horizontally
            m_bankXPos += m_bankXZoom + 1;
        } else {
            // New sprite strip
            m_bankYPos = (0x0200 - (attrib02 >> 7)) & 0x01FF;
            m_bankXPos = (attrib03 >> 7);
            
            // Adjust for non-overscan mode (304 pixels)
            if (m_screenWidth == 304) {
                m_bankXPos -= 8;
            }
            
            u16 attrib01 = readGraphicsRAM16(0x10000 + bank * 2);
            m_bankYZoom = attrib01 & 0xFF;
            m_bankSize = attrib02 & 0x3F;
        }
        
        // Skip if size is 0
        if (m_bankSize == 0) {
            continue;
        }
        
        // Get X zoom
        u16 attrib01 = readGraphicsRAM16(0x10000 + bank * 2);
        m_bankXZoom = (attrib01 >> 8) & 0x0F;
        
        // Handle X position wraparound
        if (m_bankXPos >= 0x01E0) {
            m_bankXPos -= 0x200;
        }
        
        // Only render if sprite is visible
        if (m_bankXPos >= -static_cast<s32>(m_bankXZoom) && m_bankXPos < static_cast<s32>(m_screenWidth)) {
            renderSpriteBank(bank);
        }
    }

    m_sliceStart = m_sliceEnd;
}

void PPU::calcSpriteBankLimit() {
    // Determine the highest sprite "bank" we might need to process, based on
    // the hardware per-scanline sprite strip limit (96).
    constexpr u32 MAX_SPRITE_BANKS = 0x17d;       // 381 sprite banks
    constexpr u32 MAX_SPRITEBANK_LINE = 0x60;     // 96 sprite banks per scanline

    u32 maxSpriteBank = 0;

    // SCB2 (0x10400) contains Y position + size + chain bit.
    // We scan visible lines (240) and count how many banks could be active.
    s32 bankYPos = 0;
    s32 bankSize = 0;

    for (u32 yLine = 0; yLine < 240; yLine++) {
        u32 yCount = 0;

        for (u32 bank = 0; bank < MAX_SPRITE_BANKS; bank++) {
            const u16 attrib02 = readGraphicsRAM16(0x10400 + bank * 2);

            // If not chained (bit 6 clear), update Y/size for this strip.
            // If chained, the hardware reuses the previous strip's Y/size.
            if ((attrib02 & 0x40) == 0) {
                bankYPos = (0x0200 - (attrib02 >> 7)) & 0x01FF;
                bankSize = attrib02 & 0x3F;
            }

            if (bankSize == 0) {
                continue;
            }

            const bool activeOnThisLine =
                (bankSize >= 0x20) ||
                (((static_cast<s32>(yLine) - bankYPos) & 0x01FF) < (bankSize << 4));

            if (!activeOnThisLine) {
                continue;
            }

            yCount++;

            if (bank + 1 > maxSpriteBank) {
                maxSpriteBank = bank + 1;
            }

            if (yCount >= MAX_SPRITEBANK_LINE) {
                break;
            }
        }
    }

    m_maxSpriteBank = maxSpriteBank;
}

void PPU::renderSpriteBank(u32 bankIndex) {
    if (!m_cartridge) {
        return;
    }
    
    // Get sprite tile data base pointer (64 bytes per sprite tile)
    u32 tileDataBase = bankIndex * 0x80;
    
    // Get zoom table for Y zoom
    const u8* zoomTable = &m_zoomRom[m_bankYZoom * 256];
    
    // Calculate number of lines to render
    u32 linesTotal = (m_bankSize >= 0x20) ? 0x01FF : ((m_bankSize << 4) - 1);
    u32 linesDone = 0;
    
    while (linesDone <= linesTotal) {
        u32 line = (m_bankYPos + linesDone) & 0x01FF;
        u32 yPos = line;
        
        // Skip lines outside visible area
        if (yPos < m_sliceStart) {
            linesDone += m_sliceStart - yPos;
            continue;
        }
        if (yPos >= m_sliceEnd) {
            linesDone += m_sliceStart + 512 - yPos;
            continue;
        }
        
        // Render this strip section
        u32 startTile = (linesDone >= 0x0100) ? 0x10 : 0;
        u32 startLine = linesDone & 0xFF;
        u32 endLine = (linesDone < 0x0100 && linesTotal >= 0x0100) ? 0xFF : (linesTotal & 0xFF);
        
        // Handle wraparound for full-size sprite strips
        if (m_bankSize > 0x10 && m_bankYZoom != 0xFF) {
            if (m_bankSize <= 0x20) {
                if (linesDone >= 0x0100) {
                    if (static_cast<s32>(linesDone) < (0x01FF - static_cast<s32>(m_bankYZoom))) {
                        linesDone = (0x01FF - m_bankYZoom);
                        continue;
                    }
                    startLine -= 0xFF - m_bankYZoom;
                    endLine -= 0xFF - m_bankYZoom;
                }
            } else {
                // Full strip with full wrap
                if (linesDone >= 0x0100) {
                    startLine -= 0xFF - m_bankYZoom;
                    if (static_cast<s32>(startLine) < 0) {
                        startLine = m_bankYZoom - ((-static_cast<s32>(startLine) - 1) % (m_bankYZoom + 1));
                        startTile = 0;
                    }
                } else {
                    if (static_cast<s32>(startLine) > static_cast<s32>(m_bankYZoom)) {
                        startLine %= m_bankYZoom + 1;
                        startTile = 0x10;
                    }
                }
                endLine = m_bankYZoom;
            }
        }
        
        linesDone += endLine - startLine + 1;
        
        // Clip to visible screen area
        if (endLine - startLine > m_sliceEnd - yPos - 1) {
            endLine = startLine + m_sliceEnd - yPos - 1;
        }
        
        // Render each line in this section
        u32 thisLine = startLine;
        s32 prevTile = -1;
        u32 tileNumber = 0;
        u16 tileAttrib = 0;
        u8 transparent = 0;
        u32* tilePalette = nullptr;
        
        while (thisLine <= endLine) {
            u32 tile = startTile + (zoomTable[thisLine] >> 4);
            
            // Only read tile data if tile changed
            if (static_cast<s32>(tile) != prevTile) {
                prevTile = tile;
                
                // Read tile number and attributes from sprite RAM
                // SCB1 format: tile number and attributes are stored as consecutive word pairs
                // Word 0: tile 0 number, Word 1: tile 0 attrib, Word 2: tile 1 number, etc.
                tileNumber = readGraphicsRAM16(tileDataBase + tile * 4);
                tileAttrib = readGraphicsRAM16(tileDataBase + tile * 4 + 2);
                
                // Combine tile number with high bits from attribute
                tileNumber |= (tileAttrib & 0xF0) << 12;
                tileNumber &= m_spriteTileMask;
                
                // Handle sprite animation
                if (tileAttrib & 8) {
                    tileNumber &= ~7;
                    tileNumber |= (m_spriteFrame & 7);
                } else if (tileAttrib & 4) {
                    tileNumber &= ~3;
                    tileNumber |= (m_spriteFrame & 3);
                }
                
                // Check transparency
                transparent = m_spriteTileAttrib[tileNumber];
                
                if (transparent != 1) {
                    // Get palette for this tile
                    u32 paletteIndex = (tileAttrib & 0xFF00) >> 4;
                    tilePalette = &m_palette[paletteIndex];
                }
            }
            
            // Render the line if not transparent
            if (transparent != 1) {
                u32 tileLine = (zoomTable[thisLine] & 0x0F);
                
                // Apply Y flip
                if (tileAttrib & 2) {
                    tileLine ^= 0x0F;
                }
                
                // Render the sprite line (lineData will be read inside the function)
                renderSpriteLine(nullptr, tilePalette, m_bankXPos, yPos - 0x10, 
                               tileNumber, tileLine, tileAttrib & 1, tileAttrib & 2, m_bankXZoom, transparent);
            }
            
            yPos++;
            thisLine++;
        }
    }
}

void PPU::renderSpriteLine(const u8* /* tileData */, u32* palette, s32 xPos, s32 yPos,
                          u32 tileNumber, u32 line, bool flipX, bool /* flipY */, u32 xZoom, u8 transparent) {
    if (!palette || !m_cartridge) {
        return;
    }
    
    // Check if line is visible
    if (yPos < 0 || yPos >= static_cast<s32>(m_screenHeight)) {
        return;
    }

    // Calculate pixel position in framebuffer
    u32* lineBuffer = &m_frameBuffer[yPos * m_screenWidth];
    
    // Sprite ROM is decoded into 32-bit words
    // Each sprite tile is 128 bytes = 32 UINT32 values
    // Each scanline uses 2 UINT32 values (16 pixels, 4bpp = 64 bits = 8 bytes)
    u32 tileOffset = tileNumber * 128;
    
    // Read the two 32-bit words for this line
    // Note: line is already the Y position within the tile (0-15)
    // Decoded sprite data is stored as u32 array, so read in little-endian order
    u32 word0 = m_cartridge->readSpriteROM8(tileOffset + line * 8 + 0) |
                m_cartridge->readSpriteROM8(tileOffset + line * 8 + 1) << 8 |
                m_cartridge->readSpriteROM8(tileOffset + line * 8 + 2) << 16 |
                m_cartridge->readSpriteROM8(tileOffset + line * 8 + 3) << 24;
    
    u32 word1 = m_cartridge->readSpriteROM8(tileOffset + line * 8 + 4) |
                m_cartridge->readSpriteROM8(tileOffset + line * 8 + 5) << 8 |
                m_cartridge->readSpriteROM8(tileOffset + line * 8 + 6) << 16 |
                m_cartridge->readSpriteROM8(tileOffset + line * 8 + 7) << 24;
    
    // Render pixels with zoom
    // xZoom 15 = full 16 pixels, xZoom 0 = 1 pixel
    u32 pixelsToRender = xZoom + 1;
    if (pixelsToRender > 16) pixelsToRender = 16;
    
    // Evenly distribute pixels across the 16 source pixels (centered nearest-neighbor shrink).
    static constexpr u8 pixelLookup[16][16] = {
        /* 0  */ { 8 },
        /* 1  */ { 4, 12 },
        /* 2  */ { 2, 8, 13 },
        /* 3  */ { 2, 6, 10, 14 },
        /* 4  */ { 1, 4, 8, 11, 14 },
        /* 5  */ { 1, 4, 6, 9, 12, 14 },
        /* 6  */ { 1, 3, 5, 8, 10, 12, 14 },
        /* 7  */ { 1, 3, 5, 7, 9, 11, 13, 15 },
        /* 8  */ { 0, 2, 4, 6, 8, 9, 11, 13, 15 },
        /* 9  */ { 0, 2, 4, 5, 7, 8, 10, 12, 13, 15 },
        /* 10 */ { 0, 2, 3, 5, 6, 8, 9, 10, 12, 13, 15 },
        /* 11 */ { 0, 2, 3, 4, 6, 7, 8, 10, 11, 12, 14, 15 },
        /* 12 */ { 0, 1, 3, 4, 5, 6, 8, 9, 10, 11, 12, 14, 15 },
        /* 13 */ { 0, 1, 2, 4, 5, 6, 7, 8, 9, 10, 12, 13, 14, 15 },
        /* 14 */ { 0, 1, 2, 3, 4, 5, 6, 8, 9, 10, 11, 12, 13, 14, 15 },
        /* 15 */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 },
    };

    for (u32 offset = 0; offset < pixelsToRender; offset++) {
        const u32 px = static_cast<u32>(pixelLookup[xZoom][offset]);
        s32 screenX = xPos + offset;
        
        // Clip to screen bounds
        if (screenX < 0 || screenX >= static_cast<s32>(m_screenWidth)) {
            continue;
        }
        
        // Get pixel index (with X flip)
        u32 pixelIdx = flipX ? (15 - px) : px;
        
        // Extract 4-bit color from the appropriate word
        // Pixels are packed sequentially: bits 3-0, 7-4, 11-8, 15-12, 19-16, 23-20, 27-24, 31-28
        u8 colorIdx;
        if (pixelIdx < 8) {
            // Pixels 0-7 are in word0, starting from LSB
            colorIdx = (word0 >> (pixelIdx * 4)) & 0x0F;
        } else {
            // Pixels 8-15 are in word1, starting from LSB
            colorIdx = (word1 >> ((pixelIdx - 8) * 4)) & 0x0F;
        }
        
        // Skip transparent pixels (color 0)
        if (colorIdx == 0) {
            continue;
        }
        
        // Plot pixel with alpha blending if needed
        u32 color = palette[colorIdx];
        
        if (transparent > 1) {
            // Apply transparency/alpha blending
            lineBuffer[screenX] = alphaBlend(lineBuffer[screenX], color, transparent);
        } else {
            lineBuffer[screenX] = color;
        }
    }
}

void PPU::renderText() {
    if (!m_cartridge || !m_memory) {
        return;
    }
    
    // Text layer is 40x28 tiles (320x224 pixels with 8x8 tiles)
    // Located at 0xE000-0xFFFF in graphics RAM
    // Each tile entry is 16 bits:
    // - Bits 15-12: Palette (16 palettes of 16 colors)
    // - Bits 11-0: Tile number (4096 tiles)
    
    const u8* textRom;
    const u8* tileAttrib;
    bool useBios = m_memory->isBIOSTextROMEnabled();
    
    // Check if BIOS text ROM is enabled
    if (useBios) {
        textRom = m_decodedTextBios.data();
        tileAttrib = m_textTileAttribBios.data();
        
        if (!textRom || m_decodedTextBios.empty()) {
            return;
        }
    } else {
        textRom = m_decodedText.data();
        tileAttrib = m_textTileAttrib.data();
        
        if (!textRom || m_decodedText.empty()) {
            return;
        }
    }
    
    // Render text tiles
    // Text layer is stored in COLUMN-MAJOR order in graphics RAM
    // Each column is 64 bytes (32 tiles * 2 bytes per tile)
    // Skip first 2 rows and last 2 rows (only render rows 2-29)

    // For 304-width games, clip leftmost and rightmost columns to center content
    u32 minX = (m_screenWidth == 304) ? 1 : 0;
    u32 maxX = (m_screenWidth == 304) ? 39 : 40;

    // Get bank switching mode (cached during initialization)
    TextBankMode bankMode = useBios ? TextBankMode::NONE : m_textBankMode;
    
    // Standard bank switching mode
    // This mode uses 0xEA00/0xEB00 to set bank per row rather than per tile
    // When a marker (0x0200 at 0xEA00 + z, 0xFF00 mask at 0xEB00 + z) is found,
    // extract bank from bits 0-1 of 0xEB00 + z, XOR with 3, and shift left 12 bits
    u32 rowBanks[32];
    if (bankMode == TextBankMode::STANDARD) {
        u32 currentBank = (3 << 12);  // Default bank 3
        u32 z = 0;
        for (u32 row = 0; row < 32; row++) {
            u16 marker = readGraphicsRAM16(0xEA00 + z);
            u16 bankData = readGraphicsRAM16(0xEB00 + z);
            
            if (marker == 0x0200 && (bankData & 0xFF00) == 0xFF00) {
                // Bank change marker found
                currentBank = (((bankData & 3) ^ 3) << 12);
                rowBanks[row] = currentBank;
                row++;  // Skip next row as it will use this bank too
                if (row < 32) {
                    rowBanks[row] = currentBank;
                }
            } else {
                rowBanks[row] = currentBank;
            }
            z += 4;
        }
    }
    
    for (u32 y = 2; y < 30; y++) {
        for (u32 x = minX; x < maxX; x++) {
            // Text layer is column-major: tile(x,y) = 0xE000 + x*64 + y*2
            u32 tileAddr = 0xE000 + (x << 6) + (y << 1);
            u16 tileEntry = readGraphicsRAM16(tileAddr);

            u32 tileNum = tileEntry & 0x0FFF;
            u32 paletteIdx = (tileEntry >> 12) & 0x0F;
            
            // Apply text bank switching
            if (bankMode == TextBankMode::ALTERNATE) {
                // ALTERNATE_TEXT mode (KOF2000): bank per tile using cached lookup table
                // Bank info starts at 0xEA00 + 2
                // Row offset: (y - 2) * 2 bytes per row
                // Column offset: m_bankLookupAddress[x] = (x / 6) * 64 bytes per column group
                // Each word contains bank info for 6 tiles using 2 bits each
                u16 bankInfo = readGraphicsRAM16(0xEA00 + 2 + (y - 2) * 2 + m_bankLookupAddress[x]);
                u32 bank = ((bankInfo >> m_bankLookupShift[x]) & 3) ^ 3;
                tileNum += bank << 12;
            } else if (bankMode == TextBankMode::STANDARD) {
                // Standard bank switching mode: bank per row
                // Use precomputed row bank (y-2 maps row 2-29 to index 0-27 in rowBanks)
                tileNum += rowBanks[y - 2];
            }
            
            // Check if tile is transparent
            if (tileNum < m_textTileAttrib.size() && tileAttrib[tileNum] == 1) {
                continue;  // Skip transparent tiles
            }
            
            // Render tile - rows 2-29 map to screen Y 0-223
            renderTextTile(x * 8, (y - 2) * 8, tileNum, paletteIdx * 16, textRom, tileAttrib);
        }
    }
}

void PPU::renderTextTile(s32 x, s32 y, u32 tileNum, u32 paletteOffset, 
                        const u8* textRom, const u8* /* attrib */) {
    if (!textRom) {
        return;
    }
    
    // Each text tile is 8x8 pixels, 4bpp (32 bytes)
    // Stored as 2 pixels per byte in decoded format
    const u8* tileData = textRom + tileNum * 32;
    u32* palette = &m_palette[paletteOffset];
    
    // Render each pixel of the tile
    for (u32 py = 0; py < 8; py++) {
        s32 screenY = y + py;
        
        // Clip to screen bounds
        if (screenY < 0 || screenY >= static_cast<s32>(m_screenHeight)) {
            continue;
        }

        u32* lineBuffer = &m_frameBuffer[screenY * m_screenWidth];
        
        for (u32 px = 0; px < 8; px++) {
            s32 screenX = x + px;
            
            // Adjust for non-overscan mode (304 pixels)
            if (m_screenWidth == 304) {
                screenX -= 8;
            }

            // Clip to screen bounds
            if (screenX < 0 || screenX >= static_cast<s32>(m_screenWidth)) {
                continue;
            }
            
            // Get pixel color (4bpp, 2 pixels per byte)
            u32 byteIdx = py * 4 + (px >> 1);
            u8 pixelPair = tileData[byteIdx];
            
            u8 colorIdx;
            if (px & 1) {
                colorIdx = pixelPair & 0x0F;  // Low nibble
            } else {
                colorIdx = pixelPair >> 4;     // High nibble
            }
            
            // Skip transparent pixels (color 0)
            if (colorIdx == 0) {
                continue;
            }
            
            // Plot pixel
            lineBuffer[screenX] = palette[colorIdx];
        }
    }
}

u32 PPU::alphaBlend(u32 dst, u32 src, u32 alpha) {
    // Simple alpha blending
    u32 invAlpha = 255 - alpha;
    
    u32 dstR = (dst >> 16) & 0xFF;
    u32 dstG = (dst >> 8) & 0xFF;
    u32 dstB = dst & 0xFF;
    
    u32 srcR = (src >> 16) & 0xFF;
    u32 srcG = (src >> 8) & 0xFF;
    u32 srcB = src & 0xFF;
    
    u32 r = (srcR * alpha + dstR * invAlpha) / 255;
    u32 g = (srcG * alpha + dstG * invAlpha) / 255;
    u32 b = (srcB * alpha + dstB * invAlpha) / 255;
    
    return 0xFF000000 | (r << 16) | (g << 8) | b;
}

// Sprite frame timing
void PPU::updateSpriteFrame() {
    if (m_memory && (m_memory->getIRQControl() & 0x08) == 0) {
        if (++m_spriteFrameTimer > m_spriteFrameSpeed) {
            m_spriteFrameTimer = 0;
            m_spriteFrame++;
        }
    }
}

// Video controller VRAM access
// The video controller pointer can access both 64KB banks
// Note: Reads do NOT auto-increment, only writes do
u16 PPU::readVRAM() {
    // Pointer is already a byte address (0x00000-0x1FFFF) from setVRAMPointer
    u32 fullAddress = m_graphicsRamPointer & 0x1FFFF;  // Safety mask
    return readGraphicsRAM16(fullAddress);
}

void PPU::writeVRAM(u16 value) {
    // Pointer is already a byte address (0x00000-0x1FFFF) from setVRAMPointer
    u32 fullAddress = m_graphicsRamPointer & 0x1FFFF;  // Safety mask

    writeGraphicsRAM16(fullAddress, value);
    m_graphicsRamPointer = (m_graphicsRamPointer + m_graphicsRamModulo) & 0x1FFFF;
}

// Graphics RAM access
u8 PPU::readGraphicsRAM8(u32 address) {
    address &= 0x1FFFF;  // Wrap to 128KB (0x00000-0x1FFFF)
    return m_graphicsRam[address];
}

u16 PPU::readGraphicsRAM16(u32 address) {
    address &= 0x1FFFE;  // Align to 16-bit and wrap to 128KB
    
    // Big endian
    return (m_graphicsRam[address] << 8) | m_graphicsRam[address + 1];
}

void PPU::writeGraphicsRAM8(u32 address, u8 value) {
    address &= 0x1FFFF;  // Wrap to 128KB
    m_graphicsRam[address] = value;
}

void PPU::writeGraphicsRAM16(u32 address, u16 value) {
    address &= 0x1FFFE;  // Align to 16-bit and wrap to 128KB

    // Big endian
    m_graphicsRam[address] = value >> 8;
    m_graphicsRam[address + 1] = value & 0xFF;
}

void PPU::saveState(Buffer* buf) {
    // Write graphics RAM
    buffer_write(buf, m_graphicsRam.data(), m_graphicsRam.size());

    // Write screen dimensions (for compatibility with different game resolutions)
    buffer_write(buf, &m_screenWidth, sizeof(m_screenWidth));
    buffer_write(buf, &m_screenHeight, sizeof(m_screenHeight));

    // Write frame state
    buffer_write(buf, &m_scanline, sizeof(m_scanline));
    buffer_write(buf, &m_cycles, sizeof(m_cycles));
    buffer_write(buf, &m_spriteFrameSpeed, sizeof(m_spriteFrameSpeed));
    buffer_write(buf, &m_spriteFrameTimer, sizeof(m_spriteFrameTimer));
    buffer_write(buf, &m_spriteFrame, sizeof(m_spriteFrame));
    buffer_write(buf, &m_graphicsRamPointer, sizeof(m_graphicsRamPointer));
    buffer_write(buf, &m_graphicsRamModulo, sizeof(m_graphicsRamModulo));
}

void PPU::loadState(Buffer* buf) {
    // Read graphics RAM
    buffer_read(buf, m_graphicsRam.data(), m_graphicsRam.size());

    // Read screen dimensions
    buffer_read(buf, &m_screenWidth, sizeof(m_screenWidth));
    buffer_read(buf, &m_screenHeight, sizeof(m_screenHeight));

    // Read frame state
    buffer_read(buf, &m_scanline, sizeof(m_scanline));
    buffer_read(buf, &m_cycles, sizeof(m_cycles));
    buffer_read(buf, &m_spriteFrameSpeed, sizeof(m_spriteFrameSpeed));
    buffer_read(buf, &m_spriteFrameTimer, sizeof(m_spriteFrameTimer));
    buffer_read(buf, &m_spriteFrame, sizeof(m_spriteFrame));
    buffer_read(buf, &m_graphicsRamPointer, sizeof(m_graphicsRamPointer));
    buffer_read(buf, &m_graphicsRamModulo, sizeof(m_graphicsRamModulo));
    
    // Update palette
    updatePalette();
}

} // namespace neogeo

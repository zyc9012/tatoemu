#pragma once

#include "../types.h"
#include "consts.h"
#include <array>
#include <vector>
#include <fstream>

namespace neogeo {

class CPU;
class Cartridge;
class Memory;

// Graphics RAM size (128KB total - two 64KB banks)
// Bank 0: 0x00000-0x0FFFF (sprite tile data, text layer)
// Bank 1: 0x10000-0x1FFFF (sprite control blocks SCB2/3/4)
constexpr u32 GRAPHICS_RAM_SIZE = 0x20000;

// PPU (Picture Processing Unit)
// Handles all video rendering for Neo Geo: sprites and text layer
class PPU {
public:
    PPU();
    ~PPU() = default;

    void reset();
    void step();
    
    bool isFrameComplete() const { return m_frameComplete; }
    void clearFrameComplete() { m_frameComplete = false; }
    
    // Component connections
    void setCPU(CPU* cpu) { m_cpu = cpu; }
    void setCartridge(Cartridge* cartridge) { m_cartridge = cartridge; }
    void setMemory(Memory* memory) { m_memory = memory; }
    void setVideoDevice(::VideoDevice* videoDevice) { m_videoDevice = videoDevice; }
    
    // Graphics RAM access (0x00000-0x1FFFF) - 128KB total
    // Bank 0 (0x00000-0x0FFFF):
    // 0x00000-0x06FFF: Sprite tile data (SCB1: 64 bytes per sprite, 448 sprites)
    // 0x08000-0x08FFF: SCB1 mirrors
    // 0x0E000-0x0FFFF: Text layer (fix layer, 40x32 tiles)
    // Bank 1 (0x10000-0x1FFFF):
    // 0x10000-0x107FF: Sprite size/Y position (SCB2)
    // 0x10800-0x10FFF: Sprite X position (SCB3)
    // 0x1C000-0x1C7FF: Sprite list even / Sprite number & palette (SCB4)
    // 0x1D000-0x1D7FF: Sprite list odd
    u8 readGraphicsRAM8(u32 address);
    u16 readGraphicsRAM16(u32 address);
    void writeGraphicsRAM8(u32 address, u8 value);
    void writeGraphicsRAM16(u32 address, u16 value);
    
    // Get Graphics RAM base for direct access by Memory class
    u8* getGraphicsRAM() { return m_graphicsRam.data(); }
    
    // Video controller interface (for auto-increment VRAM access)
    u16 readVRAM();
    void writeVRAM(u16 value);
    void setVRAMPointer(u16 pointer) {
        // Pointer is a word address with bank selection in bit 15
        // Convert to byte address: (lower 15 bits << 1) + bank offset
        m_graphicsRamPointer = ((pointer & 0x7FFF) << 1) | (pointer & 0x8000 ? 0x10000 : 0);
    }
    u16 getVRAMPointer() const { 
        // Convert byte address back to word address with bank bit
        u32 wordAddr = m_graphicsRamPointer >> 1;
        return (wordAddr & 0x7FFF) | (m_graphicsRamPointer & 0x10000 ? 0x8000 : 0);
    }
    void setVRAMModulo(s16 modulo) { 
        // Modulo is in words, convert to bytes
        m_graphicsRamModulo = static_cast<s32>(modulo) << 1; 
    }
    s16 getVRAMModulo() const { 
        // Convert byte modulo back to word modulo
        return static_cast<s16>(m_graphicsRamModulo >> 1); 
    }
    u32 getScanline() const { return m_scanline; }
    
    // Rendering control
    void setEnableGraphics(bool enable) { m_enableGraphics = enable; }
    void setEnableSprites(bool enable) { m_enableSprites = enable; }
    void setEnableText(bool enable) { m_enableText = enable; }
    
    // Get current sprite frame counter (for sprite animation)
    u32 getSpriteFrame() const { return m_spriteFrame; }
    void setSpriteFrame(u32 frame) { m_spriteFrame = frame; }
    
    // Save/Load state
    void saveState(std::ofstream& file);
    void loadState(std::ifstream& file);

private:
    CPU* m_cpu;
    Cartridge* m_cartridge;
    Memory* m_memory;
    VideoDevice* m_videoDevice;
    
    // Frame buffer (ARGB8888 format)
    std::vector<u32> m_frameBuffer;
    
    // Graphics RAM (128KB - two 64KB banks)
    // Contains sprite control blocks and text layer
    std::array<u8, GRAPHICS_RAM_SIZE> m_graphicsRam;
    
    // Decoded text tiles (up to 4096 8x8 tiles, 32 bytes each decoded)
    std::vector<u8> m_decodedText;
    std::vector<u8> m_decodedTextBios;
    
    // Text tile transparency attributes
    std::vector<u8> m_textTileAttrib;
    std::vector<u8> m_textTileAttribBios;
    
    // Sprite tile transparency attributes
    std::vector<u8> m_spriteTileAttrib;
    
    // 32-bit palette (converted from 16-bit palette in Memory's palette RAM)
    std::array<u32, 4096> m_palette;
    
    // Frame state
    bool m_frameComplete;
    u32 m_scanline;
    u32 m_cycles;
    u32 m_spriteFrame;  // Sprite animation frame counter
    
    // Video controller state (for auto-increment VRAM access)
    u32 m_graphicsRamPointer;  // Current VRAM byte address (0x00000-0x1FFFF)
    s32 m_graphicsRamModulo;   // VRAM modulo in bytes (added to pointer after each access)
    
    // Rendering enable flags
    bool m_enableGraphics;
    bool m_enableSprites;
    bool m_enableText;
    
    // Sprite control variables (state during sprite rendering)
    s32 m_bankXPos;
    s32 m_bankYPos;
    s32 m_bankXZoom;
    s32 m_bankYZoom;
    s32 m_bankSize;
    
    // Tile mask for sprite ROMs
    u32 m_spriteTileMask;
    u32 m_maxSpriteTile;
    
    // Zoom ROM (256 bytes * 256 zoom levels)
    std::vector<u8> m_zoomRom;
    
    // Screen dimensions
    u32 m_screenWidth;
    u32 m_screenHeight;
    
    // Slice rendering (for partial screen updates)
    u32 m_sliceStart;
    u32 m_sliceEnd;
    
    // Helper functions
    void renderFrame();
    void clearScreen();
    
    // Palette handling
    void updatePalette();
    u32 convertPaletteEntry(u16 entry, bool darken);
    
    // Layer rendering
    void renderSprites();
    void renderText();
    
    // Sprite rendering helpers
    void renderSpriteBank(u32 bankIndex);
    void renderSpriteLine(const u8* tileData, u32* palette, s32 xPos, s32 yPos, 
                         u32 tileNumber, u32 line, bool flipX, bool flipY, u32 xZoom, u8 transparent);
    
    // Text rendering helpers  
    void renderTextTile(s32 x, s32 y, u32 tileNum, u32 paletteOffset, const u8* textRom, const u8* attrib);
    
    // Initialize decoded text and sprite attributes
    void initTextROM();
    void initSpriteROM();
    void updateTextTileAttrib(u32 offset, u32 size);
    void updateSpriteTileAttrib(u32 offset, u32 size);
    
    // Alpha blending helper
    inline u32 alphaBlend(u32 dst, u32 src, u32 alpha);
    
    // Pixel plotting
    inline void plotPixel(s32 x, s32 y, u32 color);
    inline bool isPixelVisible(s32 x, s32 y) const;
    
    // Calculate sprite tile limit for optimization
    void calcSpriteBankLimit();
    u32 m_maxSpriteBank;
};

} // namespace neogeo

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

// NeoGeo Graphics RAM Layout (128KB total, 2x64KB banks):
// Bank 0 (0x0000-0xFFFF):
//   0x0000-0x7FFF: Sprite Control Block (SCB1) - Tile numbers and palette
//   0x8000-0x81FF: Sprite Control Block (SCB2) - Shrinking coefficients  
//   0x8200-0x83FF: Sprite Control Block (SCB3) - Y position, sticky bit, size
//   0x8400-0x85FF: Sprite Control Block (SCB4) - X position
//   0xE000-0xEFFF: Fix layer (text) tilemap - 40x32 tiles
// Bank 1 (0x10000-0x1FFFF):
//   Additional sprite data for sticky sprites

class PPU {
public:
    PPU();
    ~PPU() = default;

    void reset();
    void step();
    
    bool isFrameComplete() const { return m_frameComplete; }
    void clearFrameComplete() { m_frameComplete = false; }
    
    // Get current scanline for status register
    u32 getScanline() const { return m_scanline; }
    u32 getSpriteFrame() const { return m_spriteFrame; }
    
    // Graphics RAM access (via video controller)
    u16 readVRAM() const;
    void writeVRAM(u16 value);
    void setVRAMPointer(u16 value);
    void setVRAMModulo(s16 value);
    u16 getVRAMModulo() const { return static_cast<u16>(m_vramModulo >> 1); }
    
    void setCPU(CPU* cpu) { m_cpu = cpu; }
    void setCartridge(Cartridge* cartridge) { m_cartridge = cartridge; }
    void setMemory(Memory* memory) { m_memory = memory; }
    void setVideoDevice(::VideoDevice* videoDevice) { m_videoDevice = videoDevice; }
    
    // Save/Load state
    void saveState(std::ofstream& file);
    void loadState(std::ifstream& file);

private:
    CPU* m_cpu;
    Cartridge* m_cartridge;
    Memory* m_memory;
    VideoDevice* m_videoDevice;
    
    // Frame buffer (ARGB8888 format)
    std::array<u32, SCREEN_WIDTH * SCREEN_HEIGHT> m_frameBuffer;
    
    // Graphics RAM (VRAM) - 128KB (2 banks of 64KB)
    std::array<u8, 0x20000> m_vram;
    
    // VRAM access registers
    u32 m_vramPointer;      // Current VRAM address (in bytes)
    s32 m_vramModulo;       // Auto-increment value after write
    u8 m_vramBank;          // Current bank (0 or 1)
    
    // Frame state
    bool m_frameComplete;
    u32 m_scanline;
    u32 m_cycles;
    u32 m_spriteFrame;      // Current sprite animation frame (3-bit)
    u32 m_spriteFrameTimer; // Timer for auto-animation
    
    // Rendering
    void renderFrame();
    void renderSprites();
    void renderSprite(int spriteIndex);
    void renderTile(u32 tileNum, int x, int y, u32 palette, bool flipX, bool flipY, int zoomX, int zoomY);
    void renderText();
    void renderTextTile(u32 tileNum, int x, int y, u32 palette);
    
    // Palette
    u32 convertPalette(u16 paletteEntry) const;
    u32 getPaletteColor(u32 paletteIndex, u32 colorIndex) const;
    
    // Helpers
    u16 readVRAM16(u32 address) const;
    void writeVRAM16(u32 address, u16 value);
};

} // namespace neogeo

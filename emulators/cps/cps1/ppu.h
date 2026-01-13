#pragma once

#include "../../types.h"
#include "../ppu_base.h"
#include "../db.h"
#include "consts.h"
#include <array>
#include <vector>
#include <fstream>

namespace cps1 {

// Palette size (0xC00 bytes = 3072 bytes, 1536 16-bit entries across 6 pages)
constexpr u32 PALETTE_RAM_SIZE = 0xC00;

// Tile type constants
enum TileType {
    CTT_8X8   = 0,
    CTT_16X16 = 8,
    CTT_32X32 = 24,
    CTT_FLIPX = 1,
    CTT_CARE  = 2,  // Need to clip
};

// CPS1 PPU (Picture Processing Unit)
// Handles all video rendering including scroll layers, sprites, and palette
class PPU : public cps::PPUBase {
public:
    PPU();
    ~PPU() override = default;

    void reset() override;
    void step() override;
    
    bool isFrameComplete() const override { return m_frameComplete; }
    void clearFrameComplete() override { m_frameComplete = false; }
    
    void setCPU(cps::CPU* cpu) override { m_cpu = cpu; }
    void setCartridge(cps::Cartridge* cartridge) override;
    void setVideoDevice(::VideoDevice* videoDevice) override { m_videoDevice = videoDevice; }
    
    // VRAM access (from Memory class)
    u8 readVRAM8(u32 address);
    u16 readVRAM16(u32 address);
    u32 readVRAM32(u32 address);
    void writeVRAM8(u32 address, u8 value);
    void writeVRAM16(u32 address, u16 value);
    void writeVRAM32(u32 address, u32 value);
    
    // CPS Register access
    u8 readRegister8(u8 reg);
    void writeRegister8(u8 reg, u8 value);
    
    // Graphics ROM decoding (called after cartridge is loaded)
    void decodeGraphicsROM();
    
    // Save/Load state
    void saveState(std::ofstream& file) override;
    void loadState(std::ifstream& file) override;

private:
    cps::CPU* m_cpu;
    cps::Cartridge* m_cartridge;
    VideoDevice* m_videoDevice;
    
    // Frame buffer (ARGB8888 format)
    std::array<u32, SCREEN_WIDTH * SCREEN_HEIGHT> m_frameBuffer;
    
    // VRAM (192KB) - organized as:
    // 0x00000-0x2FFFF: Scroll/tile maps, sprite data, palette, etc.
    std::array<u8, VRAM_SIZE> m_vram;
    
    // Decoded graphics ROM (4bpp tiles unpacked for fast rendering)
    // Each byte represents one pixel (0-15)
    std::vector<u8> m_decodedGfx;
    u32 m_gfxLen;
    u32 m_gfxMask;
    
    // 32-bit palette (converted from 16-bit VRAM palette entries)
    std::array<u32, 0xC00> m_palette;
    
    // CPS registers (0x00-0xFF, mapped from 0x800100-0x8001FF)
    std::array<u8, 256> m_cpsRegs;
    
    // Board configuration (from cartridge/database)
    cps::BoardConfig m_boardConfig;
    
    // Scroll offsets (can be adjusted per-game)
    s32 m_layer1XOffs, m_layer1YOffs;
    s32 m_layer2XOffs, m_layer2YOffs;
    s32 m_layer3XOffs, m_layer3YOffs;
    s32 m_globalXOffs, m_globalYOffs;
    
    // Graphics bank offsets
    u32 m_gfxScroll[4];  // Offsets to scroll tiles
    
    // Graphics ROM bank mapper
    const cps::GfxRange* m_gfxMapper;
    u32 m_gfxBankSizes[4];
    
    // Frame state
    bool m_frameComplete;
    u32 m_scanline;
    u32 m_cycles;
    
    // Palette dirty flag
    bool m_paletteNeedsUpdate;
    
    // Helper functions
    void renderFrame();
    void clearScreen();
    
    // Palette handling
    void updatePalette();
    u32 convertPaletteEntry(u16 entry);
    
    // Layer rendering
    void renderLayers();
    void renderScroll1(const u8* base, s32 scrollX, s32 scrollY);
    void renderScroll2(const u8* base, s32 scrollX, s32 scrollY);
    void renderScroll3(const u8* base, s32 scrollX, s32 scrollY);
    void renderSprites();
    
    // Tile rendering (internal)
    void drawTile8x8(s32 x, s32 y, u32 tileAddr, u32 palette, u32 flip, bool clipCheck);
    void drawTile16x16(s32 x, s32 y, u32 tileAddr, u32 palette, u32 flip, bool clipCheck);
    void drawTile32x32(s32 x, s32 y, u32 tileAddr, u32 palette, u32 flip, bool clipCheck);
    
    // Pixel plotting
    inline void plotPixel(s32 x, s32 y, u32 color);
    inline bool isPixelVisible(s32 x, s32 y);
    
    // Graphics ROM access
    const u8* getGfxRom(u32 address) const;
    s32 gfxRomBankMapper(u32 type, s32 code) const;
    
    // VRAM helpers
    u8* findGfxRam(u32 address, u32 len);
    
    // Setup graphics mapper from board type
    void setupGfxMapper();
};

} // namespace cps1

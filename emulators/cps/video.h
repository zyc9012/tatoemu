#pragma once

#include "../types.h"
#include "db.h"
#include "consts.h"
#include "../components/buffer.h"
#include <array>
#include <vector>

namespace cps {

class CPU;
class Cartridge;
class Memory;

// Palette size (0xC00 bytes = 3072 bytes, 1536 16-bit entries across 6 pages)
constexpr u32 PALETTE_RAM_SIZE = 0xC00;
constexpr u32 VISIBLE_SCANLINES = 224;
constexpr u32 TOTAL_SCANLINES = 262;
constexpr u32 FIRST_VISIBLE_SCANLINE = 0x10;

// Tile type constants
enum TileType {
    CTT_8X8   = 0,
    CTT_16X16 = 8,
    CTT_32X32 = 24,
    CTT_FLIPX = 1,
    CTT_CARE  = 2,  // Need to clip
};

// Video - Handles all video rendering including scroll layers, sprites, and palette
// Unified implementation supporting both CPS1 and CPS2
class Video {
public:
    Video();
    ~Video() = default;

    void reset();
    void step(u32 cycles);
    
    void setCPU(CPU* cpu) { m_cpu = cpu; }
    void setCartridge(Cartridge* cartridge);
    void setMemory(Memory* memory) { m_memory = memory; }
    void setVideoDevice(::VideoDevice* videoDevice) { m_videoDevice = videoDevice; }
    void setDecodedGraphics(const std::vector<u8>& decodedGfx);
    
    // VRAM access (from Memory class)
    u8 readVRAM8(u32 address);
    u16 readVRAM16(u32 address);
    u32 readVRAM32(u32 address);
    void writeVRAM8(u32 address, u8 value);
    void writeVRAM16(u32 address, u16 value);
    void writeVRAM32(u32 address, u32 value);
    
    // CPS Register access
    u8 readRegister8(u8 reg);
    u16 readRegister16(u8 reg);
    void writeRegister8(u8 reg, u8 value);
    
    // CPS2 Raster interrupt control
    void setRasterLine(u32 zone, s32 scanline);
    void copyRegistersToZone(u32 zone);
    void copyFrgRegistersToZone(u32 zone);
    u32 getScanline() const { return m_scanline; }
    
    // Save/Load state
    void saveState(Buffer* buf);
    void loadState(Buffer* buf);

private:
    CPU* m_cpu;
    Cartridge* m_cartridge;
    Memory* m_memory;
    VideoDevice* m_videoDevice;
    
    // Frame buffer (ARGB8888 format)
    std::array<u32, SCREEN_WIDTH * SCREEN_HEIGHT> m_frameBuffer;

    // Whether the screen is vertical
    bool m_isVertical;
    
    // VRAM (192KB) - organized as:
    // 0x00000-0x2FFFF: Scroll/tile maps, sprite data, palette, etc.
    std::array<u8, VRAM_SIZE> m_vram;
    
    // Decoded graphics ROM (4bpp tiles unpacked for fast rendering)
    // Set by Cartridge after decoding
    std::vector<u8> m_decodedGfx;
    u32 m_gfxLen;
    u32 m_gfxMask;
    
    // 32-bit palette (converted from 16-bit VRAM palette entries)
    std::array<u32, 0xC00> m_palette;
    
    // CPS registers (0x00-0xFF, mapped from 0x800100-0x8001FF)
    std::array<u8, 256> m_cpsRegs;
    
    // Board configuration
    BoardConfig m_boardConfig;
    
    // Scroll offsets (can be adjusted per-game)
    s32 m_layer1XOffs, m_layer1YOffs;
    s32 m_layer2XOffs, m_layer2YOffs;
    s32 m_layer3XOffs, m_layer3YOffs;
    
    // Graphics bank offsets
    u32 m_gfxScroll[4];  // Offsets to scroll tiles
    
    // Graphics ROM bank mapper (CPS1 only, CPS2 uses direct addressing)
    const GfxRange* m_gfxMapper;
    u32 m_gfxBankSizes[4];
    
    // Frame state
    u32 m_scanline;
    u32 m_cycles;
    u32 m_cyclesPerFrame;
    u32 m_cyclesPerScanline;
    // Number of CPS2 beam-synced raster IRQs recorded this frame.
    s32 m_rasterIrqCount;
    
    // Palette dirty flag
    bool m_paletteNeedsUpdate;
    
    // CPS2 Z-buffer for sprite priority (16-bit per pixel)
    std::vector<u16> m_zBuffer;
    s32 m_maxZValue;
    s32 m_maxZMask;
    s32 m_zOffset;
    u16 m_currentZValue;
    
    // CPS2 Raster interrupt support
    static constexpr int MAX_RASTER = 10;
    std::array<s32, MAX_RASTER + 2> m_rasterLines;  // Scanline boundaries
    std::array<std::array<u8, 256>, MAX_RASTER> m_rasterRegs;  // Register set per raster zone
    std::array<std::array<u8, 16>, MAX_RASTER> m_rasterFrg;    // CpsSaveFrg per zone
    
    // CPS1 tile masking (BgHi mode)
    bool m_bgHiMode;
    
    // Game specific hacks
    bool m_is_xmcota;
    bool m_is_ssf2;
    bool m_is_ssf2t;
    
    // Helper functions
    void renderFrame();
    void clearScreen();

    // Process CPS2 raster interrupts
    void processCPS2RasterInterrupts();
    
    // Palette handling
    void updatePalette();
    u32 convertPaletteEntry(u16 entry);
    
    // Layer rendering - dispatches to CPS1 or CPS2 versions
    void renderLayers();
    
    // CPS1-specific rendering
    void renderLayersCPS1();
    void renderSpritesCPS1();

    // CPS2-specific rendering
    void renderLayersCPS2();
    void renderSpritesCPS2(s32 levelFrom, s32 levelTo);
    void initCPS2ZBuffer();
    
    // Common (Scroll 2 is same for both CPS1 and CPS2)
    void renderScroll1(const u8* base, s32 scrollX, s32 scrollY, s32 startLine, s32 endLine);
    void renderScroll2(const u8* base, s32 scrollX, s32 scrollY, s32 startLine, s32 endLine);
    void renderScroll3(const u8* base, s32 scrollX, s32 scrollY, s32 startLine, s32 endLine);
    
    // Tile rendering (internal)
    void drawTile8x8(s32 x, s32 y, u32 tileAddr, u32 palette, u32 flip, bool clipCheck, u16 mask = 0);
    void drawTile16x16(s32 x, s32 y, u32 tileAddr, u32 palette, u32 flip, bool clipCheck, u16 mask = 0, bool useZ = false);
    void drawTile32x32(s32 x, s32 y, u32 tileAddr, u32 palette, u32 flip, bool clipCheck, u16 mask = 0);
    
    // Pixel plotting
    inline void plotPixel(s32 x, s32 y, u32 color);
    inline void plotPixelWithZ(s32 x, s32 y, u32 color);
    inline bool isPixelVisible(s32 x, s32 y);
    
    // Graphics ROM access
    const u8* getGfxRom(u32 address) const;
    s32 gfxRomBankMapper(u32 type, s32 code) const;
    
    // VRAM helpers
    u8* findGfxRam(u32 address, u32 len);
    
    // CPS2 Memory access helpers
    u8 readObjRAM8(u32 offset);
    u16 readObjRAM16(u32 offset);
    u8 readFrgReg8(u8 reg);
    u16 readFrgReg16(u8 reg);
    
    // Setup graphics mapper from board type
    void setupGraphicsMapper();
};

} // namespace cps

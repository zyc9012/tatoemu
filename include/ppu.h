#pragma once

#include "types.h"
#include <array>
#include <fstream>

class CPU;
class MMU;

// PPU Modes
enum class PPUMode {
    HBLANK = 0,
    VBLANK = 1,
    OAM_SCAN = 2,
    DRAWING = 3
};

// LCD Control flags
enum LCDControl {
    LCDC_BG_WINDOW_ENABLE = 0x01,
    LCDC_OBJ_ENABLE = 0x02,
    LCDC_OBJ_SIZE = 0x04,
    LCDC_BG_TILEMAP = 0x08,
    LCDC_BG_WINDOW_TILES = 0x10,
    LCDC_WINDOW_ENABLE = 0x20,
    LCDC_WINDOW_TILEMAP = 0x40,
    LCDC_LCD_ENABLE = 0x80
};

// LCD Status flags
enum LCDStatus {
    STAT_LYC_INTERRUPT = 0x40,
    STAT_OAM_INTERRUPT = 0x20,
    STAT_VBLANK_INTERRUPT = 0x10,
    STAT_HBLANK_INTERRUPT = 0x08,
    STAT_LYC_EQUAL = 0x04
};

// Sprite attributes
struct Sprite {
    u8 y;
    u8 x;
    u8 tileIndex;
    u8 flags;
};

// Sprite flags
enum SpriteFlags {
    SPRITE_PRIORITY = 0x80,
    SPRITE_Y_FLIP = 0x40,
    SPRITE_X_FLIP = 0x20,
    SPRITE_PALETTE_DMG = 0x10,
    SPRITE_BANK_GBC = 0x08,      // GBC: VRAM bank
    SPRITE_PALETTE_GBC = 0x07     // GBC: palette number (0-7)
};

class PPU {
public:
    PPU();
    ~PPU();

    void setCPU(CPU* cpu);
    void setMMU(MMU* mmu);
    void reset(bool useBootrom = false);
    void setGBCMode(bool enabled);
    void step(u32 cycles);

    // Memory access
    u8 readVRAM(u16 address) const;
    void writeVRAM(u16 address, u8 value);
    u8 readOAM(u16 address) const;
    void writeOAM(u16 address, u8 value);

    // Register access
    u8 readRegister(u16 address) const;
    void writeRegister(u16 address, u8 value);

    // Framebuffer access
    const u32* getFramebuffer() const { return m_framebuffer.data(); }
    bool isFrameReady() const { return m_frameReady; }
    void clearFrameReady() { m_frameReady = false; }
    
    // DMA cycle accounting
    u32 getDMACycles() const { return m_dmaCycles; }
    void clearDMACycles() { m_dmaCycles = 0; }

    // Save/Load state
    void saveState(std::ofstream& file) const;
    void loadState(std::ifstream& file);

private:
    void setMode(PPUMode mode);
    void updateStatInterrupt();
    void renderScanline();
    void renderBackground();
    void renderWindow();
    void renderSprites();
    
    // GBC specific
    void performHDMA();
    void performGDMA();
    u16 readColorPalette(u8 paletteIndex, u8 colorIndex) const;
    u32 convertGBCColorToRGB(u16 color) const;
    
    // Tile and pixel helpers
    u8 getTilePixel(u16 tileAddress, u8 x, u8 y, bool useBank1) const;
    u32 getDMGColor(u8 paletteValue, u8 colorIndex) const;

    CPU* m_cpu;
    MMU* m_mmu;

    // VRAM (8KB x 2 banks for GBC)
    std::array<u8, 0x4000> m_vram;
    u8 m_vramBank;

    // OAM (Object Attribute Memory)
    std::array<u8, 0xA0> m_oam;

    // LCD Registers
    u8 m_lcdc;      // LCD Control (0xFF40)
    u8 m_stat;      // LCD Status (0xFF41)
    u8 m_scy;       // Scroll Y (0xFF42)
    u8 m_scx;       // Scroll X (0xFF43)
    u8 m_ly;        // LCD Y coordinate (0xFF44)
    u8 m_lyc;       // LY Compare (0xFF45)
    u8 m_dma;       // DMA Transfer (0xFF46)
    u8 m_bgp;       // BG Palette Data (0xFF47)
    u8 m_obp0;      // Object Palette 0 (0xFF48)
    u8 m_obp1;      // Object Palette 1 (0xFF49)
    u8 m_wy;        // Window Y (0xFF4A)
    u8 m_wx;        // Window X (0xFF4B)

    // GBC Color Palettes
    u8 m_bgpi;      // Background Palette Index (0xFF68)
    u8 m_obpi;      // Object Palette Index (0xFF6A)
    std::array<u8, 64> m_bgPaletteData;   // Background palette memory (8 palettes x 4 colors x 2 bytes)
    std::array<u8, 64> m_objPaletteData;  // Object palette memory (8 palettes x 4 colors x 2 bytes)

    // HDMA Registers (GBC)
    u8 m_hdma1;     // HDMA Source High (0xFF51)
    u8 m_hdma2;     // HDMA Source Low (0xFF52)
    u8 m_hdma3;     // HDMA Dest High (0xFF53)
    u8 m_hdma4;     // HDMA Dest Low (0xFF54)
    u8 m_hdma5;     // HDMA Length/Mode/Start (0xFF55)
    bool m_hdmaActive;
    u16 m_hdmaSource;
    u16 m_hdmaDest;
    u16 m_hdmaRemaining;

    // Mode and timing
    PPUMode m_mode;
    u32 m_modeCycles;
    bool m_frameReady;
    u8 m_windowLineCounter;
    bool m_windowRenderedThisFrame;

    // Framebuffer (160x144 pixels, ARGB8888)
    std::array<u32, SCREEN_WIDTH * SCREEN_HEIGHT> m_framebuffer;

    // Scanline buffers for priority handling
    std::array<u8, SCREEN_WIDTH> m_bgPriority;  // BG color indices for sprite priority
    std::array<bool, SCREEN_WIDTH> m_bgPriorityFlag;  // GBC BG-to-OBJ priority flag
    
    bool m_gbcMode;
    bool m_statInterruptLine;
    u8 m_modeChangeDelay;

    // DMA cycle tracking (for proper timing)
    u32 m_dmaCycles;
};


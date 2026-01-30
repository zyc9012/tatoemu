#pragma once

#include "../types.h"
#include "consts.h"
#include <array>
#include "../components/buffer.h"

namespace nes {

class CPU;
class Cartridge;

// PPUCTRL ($2000) register bits
enum PPUCtrlFlags : u8 {
    PPUCTRL_NAMETABLE_X     = 0x01,  // Nametable select bit 0
    PPUCTRL_NAMETABLE_Y     = 0x02,  // Nametable select bit 1
    PPUCTRL_INCREMENT       = 0x04,  // VRAM address increment (0: +1, 1: +32)
    PPUCTRL_SPRITE_TABLE    = 0x08,  // Sprite pattern table (0: $0000, 1: $1000)
    PPUCTRL_BG_TABLE        = 0x10,  // Background pattern table (0: $0000, 1: $1000)
    PPUCTRL_SPRITE_SIZE     = 0x20,  // Sprite size (0: 8x8, 1: 8x16)
    PPUCTRL_MASTER_SLAVE    = 0x40,  // PPU master/slave (not used on NES)
    PPUCTRL_NMI_ENABLE      = 0x80   // Generate NMI at VBlank
};

// PPUMASK ($2001) register bits
enum PPUMaskFlags : u8 {
    PPUMASK_GRAYSCALE       = 0x01,  // Grayscale mode
    PPUMASK_SHOW_BG_LEFT    = 0x02,  // Show background in leftmost 8 pixels
    PPUMASK_SHOW_SPR_LEFT   = 0x04,  // Show sprites in leftmost 8 pixels
    PPUMASK_SHOW_BG         = 0x08,  // Show background
    PPUMASK_SHOW_SPR        = 0x10,  // Show sprites
    PPUMASK_EMPHASIZE_R     = 0x20,  // Emphasize red (green on PAL)
    PPUMASK_EMPHASIZE_G     = 0x40,  // Emphasize green (red on PAL)
    PPUMASK_EMPHASIZE_B     = 0x80   // Emphasize blue
};

// PPUSTATUS ($2002) register bits
enum PPUStatusFlags : u8 {
    PPUSTATUS_SPRITE_OVERFLOW = 0x20,  // Sprite overflow (buggy)
    PPUSTATUS_SPRITE0_HIT     = 0x40,  // Sprite 0 hit
    PPUSTATUS_VBLANK          = 0x80   // In VBlank
};

// OAM sprite entry (4 bytes each, 64 sprites = 256 bytes)
struct OAMEntry {
    u8 y;           // Y position (top of sprite) - 1
    u8 tileIndex;   // Tile index
    u8 attributes;  // Attributes (palette, priority, flip)
    u8 x;           // X position (left of sprite)
};

// OAM attribute bits
enum OAMAttributes : u8 {
    OAM_PALETTE     = 0x03,  // Palette number (4-7)
    OAM_PRIORITY    = 0x20,  // Priority (0: in front of background, 1: behind)
    OAM_FLIP_H      = 0x40,  // Horizontal flip
    OAM_FLIP_V      = 0x80   // Vertical flip
};

// Internal sprite data for rendering (secondary OAM)
struct SpriteRenderData {
    u8 patternLow;      // Low byte of pattern data
    u8 patternHigh;     // High byte of pattern data
    u8 attributes;      // Sprite attributes
    u8 x;               // X position
    u8 oamIndex;        // Original OAM index (for sprite 0 hit)
};

class PPU {
public:
    PPU();
    ~PPU() = default;

    void setCPU(CPU* cpu) { m_cpu = cpu; }
    void setCartridge(Cartridge* cartridge) { m_cartridge = cartridge; }
    void setVideoDevice(VideoDevice* videoDevice) { m_videoDevice = videoDevice; }
    
    void reset();
    void step();
    
    // CPU register access ($2000-$2007, mirrored)
    u8 readRegister(u16 address);
    void writeRegister(u16 address, u8 value);
    
    // Frame completion tracking
    bool isFrameComplete() const { return m_frameComplete; }
    void clearFrameComplete() { m_frameComplete = false; }
    
    // Timing access (for mapper IRQs)
    u16 getCycle() const { return m_cycle; }
    u16 getScanline() const { return m_scanline; }
    
    // Rendering state (for mapper IRQ clocking - MMC3 only clocks when rendering)
    bool isRenderingEnabled() const { return (m_ppuMask & (PPUMASK_SHOW_BG | PPUMASK_SHOW_SPR)) != 0; }
    
    // Sprite helpers
    u8 getSpriteHeight() const;
    
    // Helpers for MMC5 CHR banking
    bool isFetchingBackgroundPattern() const;
    bool isFetchingSpritePattern() const;
    
    // Framebuffer access
    const u32* getFramebuffer() const { return m_framebuffer.data(); }
    
    // Internal RAM access for mappers (MMC5)
    u8 readCIRAM(u16 address) const { return m_vram[address & 0x07FF]; }
    void writeCIRAM(u16 address, u8 value) { m_vram[address & 0x07FF] = value; }

    // Save/Load state
    void saveState(Buffer* buf);
    void loadState(Buffer* buf);

private:
    // PPU internal memory access
    u8 ppuRead(u16 address);
    void ppuWrite(u16 address, u8 value);
    
    // Nametable mirroring
    u16 mirrorNametableAddress(u16 address) const;
    
    // Rendering pipeline
    void renderPixel();
    void evaluateSprites();
    void loadSpriteTiles();
    void fetchBackgroundTile();
    void loadBackgroundShifters();
    void updateShifters();
    
    // Sprite helpers
    bool isSpriteInRange(const OAMEntry& sprite, u16 scanline) const;
    void fetchSpritePattern(u8 spriteIndex);
    
    // Background pixel
    u8 getBackgroundPixel() const;
    
    // Sprite pixel (returns pixel color and sprite index)
    u8 getSpritePixel(u8& spriteIndex, bool& priority) const;
    
    // Rendering state checks (private helpers)
    bool isBackgroundEnabled() const;
    bool isSpriteEnabled() const;
    
    // Address increment/manipulation
    void incrementX();
    void incrementY();
    void transferX();
    void transferY();
    
    // Component pointers
    CPU* m_cpu;
    Cartridge* m_cartridge;
    VideoDevice* m_videoDevice;
    
    // Timing
    u16 m_cycle;        // Current PPU cycle (0-340)
    u16 m_scanline;     // Current scanline (0-261)
    bool m_frameComplete;
    bool m_oddFrame;    // Odd/even frame flag (for skip cycle)
    
    // Registers
    u8 m_ppuCtrl;       // $2000 - PPUCTRL
    u8 m_ppuMask;       // $2001 - PPUMASK
    u8 m_ppuStatus;     // $2002 - PPUSTATUS
    u8 m_oamAddr;       // $2003 - OAMADDR
    
    // Internal registers (loopy registers)
    u16 m_vramAddr;     // Current VRAM address (15 bits)
    u16 m_tempAddr;     // Temporary VRAM address (15 bits)
    u8 m_fineX;         // Fine X scroll (3 bits)
    bool m_writeToggle; // First/second write toggle
    
    // Read buffer for PPUDATA
    u8 m_dataBuffer;
    
    // Background rendering shift registers
    u16 m_bgShiftPatternLow;
    u16 m_bgShiftPatternHigh;
    u16 m_bgShiftAttrLow;
    u16 m_bgShiftAttrHigh;
    
    // Background latches (fetched during tile fetch)
    u8 m_bgNextTileId;
    u8 m_bgNextTileAttr;
    u8 m_bgNextTileLow;
    u8 m_bgNextTileHigh;
    
    // Memory
    std::array<u8, VRAM_SIZE> m_vram;            // 2KB nametable RAM
    std::array<u8, PALETTE_SIZE> m_palette;      // 32-byte palette RAM
    std::array<u8, OAM_SIZE> m_oam;              // 256-byte OAM
    std::array<u8, OAM_SECONDARY_SIZE> m_secondaryOam;  // 32-byte secondary OAM
    
    // Sprite rendering
    u8 m_spriteCount;       // Number of sprites on current scanline
    std::array<SpriteRenderData, 8> m_spriteRenderData;  // Sprites to render
    bool m_sprite0OnScanline;   // Is sprite 0 on current scanline?
    bool m_sprite0HitPossible;  // Can sprite 0 hit occur?
    
    // Sprite evaluation state
    u8 m_spriteEvalN;       // OAM entry being evaluated
    u8 m_spriteEvalM;       // OAM byte offset within entry
    u8 m_secondaryOamAddr;  // Write pointer to secondary OAM
    bool m_spriteEvalComplete;
    
    // NMI state
    bool m_nmiOccurred;
    bool m_nmiOutput;
    
    // Open bus behavior
    u8 m_openBus;
    
    // Framebuffer
    std::array<u32, SCREEN_WIDTH * SCREEN_HEIGHT> m_framebuffer;
    
    // NES system palette (RGB values)
    static const std::array<u32, 64> s_nesPalette;
};

} // namespace nes

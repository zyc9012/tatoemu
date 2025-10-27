#pragma once

#include "types.h"
#include <array>
#include <fstream>

class MMU;
class CPU;

// PPU Modes
enum PPUMode {
    MODE_HBLANK = 0,
    MODE_VBLANK = 1,
    MODE_OAM_SCAN = 2,
    MODE_DRAWING = 3
};

class PPU {
public:
    PPU();
    ~PPU();

    void setCPU(CPU* cpu);
    void setMMU(MMU* mmu);
    void step(u32 cycles);
    void reset();

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
    
    // Save/Load state
    void saveState(std::ofstream& file) const;
    void loadState(std::ifstream& file);

private:
    void setMode(PPUMode mode);
    void renderScanline();
    void renderBackground(u8 line);
    void renderWindow(u8 line);
    void renderSprites(u8 line);
    
    u32 getColor(u8 colorId, u8 palette) const;
    void performDMA(u8 value);

    CPU* m_cpu;
    MMU* m_mmu;

    // VRAM and OAM
    std::array<u8, 0x2000> m_vram;  // 8KB Video RAM
    std::array<u8, 0xA0> m_oam;     // 160 bytes OAM (sprite attributes)
    
    // LCD Registers
    u8 m_lcdc;  // LCD Control (0xFF40)
    u8 m_stat;  // LCD Status (0xFF41)
    u8 m_scy;   // Scroll Y (0xFF42)
    u8 m_scx;   // Scroll X (0xFF43)
    u8 m_ly;    // LCD Y coordinate (0xFF44)
    u8 m_lyc;   // LY Compare (0xFF45)
    u8 m_dma;   // DMA Transfer (0xFF46)
    u8 m_bgp;   // Background Palette (0xFF47)
    u8 m_obp0;  // Object Palette 0 (0xFF48)
    u8 m_obp1;  // Object Palette 1 (0xFF49)
    u8 m_wy;    // Window Y (0xFF4A)
    u8 m_wx;    // Window X (0xFF4B)
    
    // State
    PPUMode m_mode;
    u32 m_modeCycles;
    std::array<u32, SCREEN_WIDTH * SCREEN_HEIGHT> m_framebuffer;
    std::array<u32, SCREEN_WIDTH> m_scanlineBuffer;
    bool m_frameReady;
};


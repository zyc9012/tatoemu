#pragma once

#include "../types.h"

namespace nes {

// NES specifications
constexpr u16 SCREEN_WIDTH = 256;
constexpr u16 SCREEN_HEIGHT = 240;

// NTSC timing
constexpr u32 CPU_CLOCK_SPEED = 1789773;  // ~1.79 MHz (NTSC)
constexpr double TARGET_FPS = 60.0988;     // NTSC frame rate
constexpr u32 PPU_CYCLES_PER_CPU = 3;      // PPU runs 3x faster than CPU

// PPU timing (NTSC)
constexpr u32 SCANLINES_PER_FRAME = 262;   // 0-261 (including pre-render)
constexpr u32 CYCLES_PER_SCANLINE = 341;   // PPU cycles per scanline
constexpr u32 VISIBLE_SCANLINES = 240;     // Visible scanlines (0-239)
constexpr u32 PRE_RENDER_SCANLINE = 261;   // Pre-render scanline

// CPU cycles per frame (accurate)
// Total PPU cycles: 341 * 262 = 89342
// CPU cycles: 89342 / 3 = 29780.67 (we handle fractional cycles)
constexpr u32 CPU_CYCLES_PER_FRAME = 29781;

// Memory map sizes
constexpr u32 RAM_SIZE = 0x800;            // 2KB internal RAM
constexpr u32 VRAM_SIZE = 0x800;           // 2KB VRAM (nametables)
constexpr u32 PALETTE_SIZE = 0x20;         // 32 bytes palette
constexpr u32 OAM_SIZE = 0x100;            // 256 bytes OAM
constexpr u32 OAM_SECONDARY_SIZE = 0x20;   // 32 bytes secondary OAM

// PPU registers (addresses relative to $2000)
constexpr u16 PPUCTRL = 0x2000;
constexpr u16 PPUMASK = 0x2001;
constexpr u16 PPUSTATUS = 0x2002;
constexpr u16 OAMADDR = 0x2003;
constexpr u16 OAMDATA = 0x2004;
constexpr u16 PPUSCROLL = 0x2005;
constexpr u16 PPUADDR = 0x2006;
constexpr u16 PPUDATA = 0x2007;
constexpr u16 OAMDMA = 0x4014;

// APU registers
constexpr u16 APU_START = 0x4000;
constexpr u16 APU_END = 0x4017;

// Controller registers
constexpr u16 JOY1 = 0x4016;
constexpr u16 JOY2 = 0x4017;

// iNES header constants
constexpr u32 INES_HEADER_SIZE = 16;
constexpr u32 PRG_ROM_BANK_SIZE = 16384;   // 16KB
constexpr u32 CHR_ROM_BANK_SIZE = 8192;    // 8KB

// Nametable mirroring modes
enum class MirrorMode {
    HORIZONTAL = 0,
    VERTICAL = 1,
    SINGLE_SCREEN_A = 2,
    SINGLE_SCREEN_B = 3,
    FOUR_SCREEN = 4
};

// Controller buttons (bit positions)
enum ControllerButton : u8 {
    BUTTON_A = 0,
    BUTTON_B = 1,
    BUTTON_SELECT = 2,
    BUTTON_START = 3,
    BUTTON_UP = 4,
    BUTTON_DOWN = 5,
    BUTTON_LEFT = 6,
    BUTTON_RIGHT = 7
};

} // namespace nes

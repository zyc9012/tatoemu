#pragma once

#include "../types.h"

namespace md {

constexpr u32 MASTER_CLOCK_NTSC = 53693175;
constexpr u32 MASTER_CLOCK_PAL  = 53203424;

constexpr u32 MASTER_CYCLES_PER_LINE = 3420;

constexpr u32 M68K_CLOCK_DIVIDER = 7;
constexpr u32 Z80_CLOCK_DIVIDER  = 15;

constexpr u32 M68K_CLOCK = MASTER_CLOCK_NTSC / M68K_CLOCK_DIVIDER;  // ~7.67 MHz
constexpr u32 Z80_CLOCK  = MASTER_CLOCK_NTSC / Z80_CLOCK_DIVIDER;   // ~3.58 MHz

// Z80 cycles per scanline is exact (3420 / 15)
constexpr u32 Z80_CYCLES_PER_LINE = MASTER_CYCLES_PER_LINE / Z80_CLOCK_DIVIDER;  // 228

// ---------------------------------------------------------------------------
// Video timing
// ---------------------------------------------------------------------------
constexpr u32 NTSC_TOTAL_SCANLINES = 262;
constexpr u32 PAL_TOTAL_SCANLINES  = 313;

constexpr double TARGET_FPS_NTSC =
    static_cast<double>(MASTER_CLOCK_NTSC) / (MASTER_CYCLES_PER_LINE * NTSC_TOTAL_SCANLINES);
constexpr double TARGET_FPS_PAL =
    static_cast<double>(MASTER_CLOCK_PAL) / (MASTER_CYCLES_PER_LINE * PAL_TOTAL_SCANLINES);

// The framebuffer is always H40-sized.  H32 (256px) modes are scaled up to
// 320px on output, which matches how both modes filled the same CRT width.
constexpr u16 SCREEN_WIDTH  = 320;
constexpr u16 SCREEN_HEIGHT = 224;

constexpr u32 H32_WIDTH = 256;
constexpr u32 H40_WIDTH = 320;

// ---------------------------------------------------------------------------
// Memory sizes
// ---------------------------------------------------------------------------
constexpr u32 WORK_RAM_SIZE = 0x10000;   // 64 KB 68000 work RAM
constexpr u32 Z80_RAM_SIZE  = 0x2000;    // 8 KB Z80 sound RAM
constexpr u32 VRAM_SIZE     = 0x10000;   // 64 KB video RAM
constexpr u32 CRAM_ENTRIES  = 64;        // 4 palettes x 16 colours
constexpr u32 VSRAM_ENTRIES = 40;        // one entry per 2-cell column (H40)

constexpr u32 MAX_ROM_SIZE  = 0x400000;  // 4 MB directly addressable

// ---------------------------------------------------------------------------
// Interrupt levels (68000 autovector)
// ---------------------------------------------------------------------------
constexpr u8 IRQ_EXTERNAL = 2;
constexpr u8 IRQ_HBLANK   = 4;
constexpr u8 IRQ_VBLANK   = 6;

} // namespace md

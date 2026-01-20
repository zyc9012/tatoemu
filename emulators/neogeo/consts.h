#pragma once

#include "../types.h"

namespace neogeo {

// Frame rate (NeoGeo runs at ~59.18 Hz)
// 15625 Hz horizontal refresh / 264 scanlines = ~59.18 Hz
constexpr double TARGET_FPS = 59.1856;

// CPU frequencies
constexpr u32 CPU_FREQUENCY = 12000000;  // 12 MHz (68000)
constexpr u32 SOUND_CPU_FREQUENCY = 8000000;  // 8 MHz (Z80)

// Cycles per frame
constexpr u32 CPU_CYCLES_PER_FRAME = static_cast<u32>(CPU_FREQUENCY / TARGET_FPS);
constexpr u32 SOUND_CPU_CYCLES_PER_FRAME = static_cast<u32>(SOUND_CPU_FREQUENCY / TARGET_FPS);
constexpr float SOUND_CYCLES_RATIO = static_cast<float>(SOUND_CPU_FREQUENCY) / static_cast<float>(CPU_FREQUENCY);

// Memory sizes
constexpr u32 WORK_RAM_SIZE = 64 * 1024;      // 64 KB work RAM
constexpr u32 SRAM_SIZE = 64 * 1024;          // 64 KB SRAM
constexpr u32 PALETTE_RAM_SIZE = 0x2000;      // 8 KB palette RAM (4096 colors)
constexpr u32 SPRITE_RAM_SIZE = 0x8000;       // 32 KB sprite RAM
constexpr u32 TEXT_RAM_SIZE = 0x2000;         // 8 KB text RAM
constexpr u32 Z80_RAM_SIZE = 0x800;           // 2 KB Z80 RAM (0xF800-0xFFFF)

// Scanline information
constexpr u32 VISIBLE_SCANLINES = 224;
constexpr u32 TOTAL_SCANLINES = 264;
constexpr u32 SCANLINE_OFFSET = 0xF8;  // Offset for scanline counter

} // namespace neogeo

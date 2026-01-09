#pragma once

#include "../types.h"

namespace cps {

// Screen dimensions (shared between CPS1 and CPS2)
constexpr u16 SCREEN_WIDTH = 384;
constexpr u16 SCREEN_HEIGHT = 224;

// Frame rate (CPS systems run at ~59.63 Hz)
constexpr double TARGET_FPS = 59.6294;

// Memory sizes (shared)
constexpr u32 RAM_SIZE = 64 * 1024;  // 64 KB work RAM
constexpr u32 VRAM_SIZE = 192 * 1024;  // 192 KB VRAM
constexpr u32 SOUND_RAM_SIZE = 2 * 1024;  // 2 KB Z80 RAM

// Memory map addresses (base - may differ between CPS1/CPS2)
constexpr u32 RAM_START = 0x000000;
constexpr u32 RAM_END = 0x00FFFF;
constexpr u32 VRAM_START = 0x800000;
constexpr u32 VRAM_END = 0x8FFFFF;
constexpr u32 SOUND_RAM_START = 0x0000;
constexpr u32 SOUND_RAM_END = 0x07FF;

} // namespace cps

#pragma once

#include "../../types.h"

namespace cps1 {

// Screen dimensions
constexpr u16 SCREEN_WIDTH = 384;
constexpr u16 SCREEN_HEIGHT = 224;

// Frame rate (CPS1 runs at ~59.63 Hz)
constexpr double TARGET_FPS = 59.6294;

// CPU frequencies
constexpr u32 CPU_FREQUENCY = 10000000;  // 10 MHz (68000)
constexpr u32 SOUND_CPU_FREQUENCY = 4000000;  // 4 MHz (Z80)

// Cycles per frame
constexpr u32 CPU_CYCLES_PER_FRAME = static_cast<u32>(CPU_FREQUENCY / TARGET_FPS);
constexpr u32 SOUND_CPU_CYCLES_PER_FRAME = static_cast<u32>(SOUND_CPU_FREQUENCY / TARGET_FPS);
constexpr float SOUND_CYCLES_RATIO = static_cast<float>(SOUND_CPU_FREQUENCY) / static_cast<float>(CPU_FREQUENCY);

// Memory sizes
constexpr u32 RAM_SIZE = 64 * 1024;  // 64 KB work RAM
constexpr u32 VRAM_SIZE = 192 * 1024;  // 192 KB VRAM
constexpr u32 SOUND_RAM_SIZE = 2 * 1024;  // 2 KB Z80 RAM

// Memory map addresses
constexpr u32 RAM_START = 0x000000;
constexpr u32 RAM_END = 0x00FFFF;
constexpr u32 VRAM_START = 0x800000;
constexpr u32 VRAM_END = 0x8FFFFF;
constexpr u32 SOUND_RAM_START = 0x0000;
constexpr u32 SOUND_RAM_END = 0x07FF;

} // namespace cps1

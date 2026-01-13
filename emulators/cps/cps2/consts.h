#pragma once

#include "../../types.h"

namespace cps2 {

// Screen dimensions (same as CPS1)
constexpr u16 SCREEN_WIDTH = 384;
constexpr u16 SCREEN_HEIGHT = 224;

// Frame rate (CPS2 runs at ~59.63 Hz)
constexpr double TARGET_FPS = 59.6294;

// CPU frequencies
constexpr u32 CPU_FREQUENCY = 16000000;  // 16 MHz (68000 - faster than CPS1)
constexpr u32 SOUND_CPU_FREQUENCY = 4000000;  // 4 MHz (Z80 - same as CPS1)

// Cycles per frame
constexpr u32 CPU_CYCLES_PER_FRAME = static_cast<u32>(CPU_FREQUENCY / TARGET_FPS);
constexpr u32 SOUND_CPU_CYCLES_PER_FRAME = static_cast<u32>(SOUND_CPU_FREQUENCY / TARGET_FPS);
constexpr float SOUND_CYCLES_RATIO = static_cast<float>(SOUND_CPU_FREQUENCY) / static_cast<float>(CPU_FREQUENCY);

// Memory sizes
constexpr u32 RAM_SIZE = 64 * 1024;  // 64 KB work RAM
constexpr u32 VRAM_SIZE = 192 * 1024;  // 192 KB VRAM
constexpr u32 SOUND_RAM_SIZE = 2 * 1024;  // 2 KB Z80 RAM
constexpr u32 EXTRA_RAM_SIZE = 16 * 1024;  // 16 KB extra RAM at 0x660000
constexpr u32 OBJ_RAM_SIZE = 64 * 1024;  // 64 KB object RAM at 0x708000

// Memory map addresses
constexpr u32 RAM_START = 0xFF0000;
constexpr u32 RAM_END = 0xFFFFFF;
constexpr u32 VRAM_START = 0x900000;
constexpr u32 VRAM_END = 0x92FFFF;
constexpr u32 EXTRA_RAM_START = 0x660000;
constexpr u32 EXTRA_RAM_END = 0x663FFF;
constexpr u32 OBJ_RAM_START = 0x708000;
constexpr u32 OBJ_RAM_END = 0x717FFF;
constexpr u32 REGISTERS_START = 0x400000;  // CPS2 registers (0x400000-0x40000F)
constexpr u32 REGISTERS_END = 0x40000F;
constexpr u32 SOUND_RAM_START = 0x0000;
constexpr u32 SOUND_RAM_END = 0x07FF;

} // namespace cps2

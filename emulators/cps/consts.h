#pragma once

#include "../types.h"

namespace cps {

// Screen dimensions (shared between CPS1 and CPS2)
constexpr u16 SCREEN_WIDTH = 384;
constexpr u16 SCREEN_HEIGHT = 224;

// Frame rate (CPS systems run at ~59.63 Hz)
constexpr double TARGET_FPS = 59.6294;

// Memory sizes (shared)
constexpr u32 VRAM_SIZE = 192 * 1024;  // 192 KB VRAM

constexpr u32 CPU_CYCLES_PER_STEP = 50;

} // namespace cps

namespace cps1 {

// Frame rate (CPS1 runs at ~59.63 Hz)
constexpr double TARGET_FPS = 59.6294;

// CPU frequencies
constexpr u32 CPU_FREQUENCY = 10000000;  // 10 MHz (68000)
constexpr u32 SOUND_CPU_FREQUENCY = 4000000;  // 4 MHz (Z80)

// Cycles per frame
constexpr u32 CPU_CYCLES_PER_FRAME = static_cast<u32>(CPU_FREQUENCY / TARGET_FPS);
constexpr u32 SOUND_CPU_CYCLES_PER_FRAME = static_cast<u32>(SOUND_CPU_FREQUENCY / TARGET_FPS);

} // namespace cps1

namespace cps1qs {

// Frame rate (CPS1 runs at ~59.63 Hz)
constexpr double TARGET_FPS = 59.6294;

// CPU frequencies
constexpr u32 CPU_FREQUENCY = 10000000;  // 10 MHz (68000)
constexpr u32 SOUND_CPU_FREQUENCY = 8000000;  // 8 MHz (Z80)

// Cycles per frame
constexpr u32 CPU_CYCLES_PER_FRAME = static_cast<u32>(CPU_FREQUENCY / TARGET_FPS);
constexpr u32 SOUND_CPU_CYCLES_PER_FRAME = static_cast<u32>(SOUND_CPU_FREQUENCY / TARGET_FPS);

} // namespace cps1qs

namespace cps2 {

// Frame rate (CPS2 runs at ~59.63 Hz)
constexpr double TARGET_FPS = 59.6294;

// CPU frequencies
constexpr u32 CPU_FREQUENCY = 16000000;  // 16 MHz (68000 - faster than CPS1)
constexpr u32 SOUND_CPU_FREQUENCY = 8000000;  // 8 MHz (Z80)

// Cycles per frame
constexpr u32 CPU_CYCLES_PER_FRAME = static_cast<u32>(CPU_FREQUENCY / TARGET_FPS);
constexpr u32 SOUND_CPU_CYCLES_PER_FRAME = static_cast<u32>(SOUND_CPU_FREQUENCY / TARGET_FPS);

} // namespace cps2

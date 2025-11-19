#pragma once

#include <cstdint>
#include <filesystem>

namespace fs = std::filesystem;

namespace gb {

// Type definitions for clarity
using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using s8 = int8_t;
using s16 = int16_t;
using s32 = int32_t;

// GameBoy specifications
constexpr u16 SCREEN_WIDTH = 160;
constexpr u16 SCREEN_HEIGHT = 144;
constexpr u32 CLOCK_SPEED = 4194304; // 4.194304 MHz (normal speed)
constexpr u32 CLOCK_SPEED_DOUBLE = 8388608; // 8.388608 MHz (double speed for GBC)
// Accurate Game Boy frame rate: 4194304 / 70224 ≈ 59.7275 Hz
constexpr u32 CYCLES_PER_FRAME = 70224; // Exact cycles per frame
constexpr double TARGET_FPS = 59.7275;

// GBC Mode
enum class GBCMode {
    DMG_ONLY = 0x00,        // DMG (original Game Boy) only
    GBC_COMPATIBLE = 0x80,   // GBC compatible (works on both)
    GBC_ONLY = 0xC0          // GBC only
};

} // namespace gb


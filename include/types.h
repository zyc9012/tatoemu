#pragma once

#include <cstdint>

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
constexpr u32 CLOCK_SPEED = 4194304; // 4.194304 MHz
constexpr u8 TARGET_FPS = 60;
constexpr u32 CYCLES_PER_FRAME = CLOCK_SPEED / TARGET_FPS; // ~69905 cycles per frame


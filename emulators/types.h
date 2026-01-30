#pragma once

#include <cstdint>
#include <filesystem>

namespace fs = std::filesystem;

// Type definitions for clarity
using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using s8 = int8_t;
using s16 = int16_t;
using s32 = int32_t;

#define log_info_nn(fmt, ...) fprintf(stdout, fmt __VA_OPT__(,) __VA_ARGS__) // no newline
#define log_info(fmt, ...) fprintf(stdout, fmt "\n" __VA_OPT__(,) __VA_ARGS__)
#define log_error(fmt, ...) fprintf(stderr, fmt "\n" __VA_OPT__(,) __VA_ARGS__)

// AudioDevice interface for platform-specific audio output
class AudioDevice {
    public:
        virtual ~AudioDevice() = default;
        virtual void writeSamples(void* stream, u32 length) = 0;
};

// VideoDevice interface for platform-specific video output
class VideoDevice {
    public:
        virtual ~VideoDevice() = default;
        virtual void render(u32* buffer) = 0;
};
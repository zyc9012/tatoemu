// OKI MSM6295 module
// Emulation by Jan Klaassen

#pragma once

#include "../../../types.h"
#include "../../buffer.h"

// Four-channel ADPCM sample player. The real chip has a single analogue
// output pin, so rendering is mono.
class Msm6295 {
public:
    // chipRate is the chip's own sample rate (set by its pin 7 divider),
    // hostRate the rate render() should resample to.
    void init(u32 chipRate, u32 hostRate);
    void reset();
    void setSampleRate(u32 chipRate, u32 hostRate);
    void setVolume(double volume);

    // Point part of the 256 KB sample address space at ROM. Both bounds are
    // byte offsets into that space, rounded down to a 256-byte page.
    void setBank(const u8* romData, u32 start, u32 end);

    u8 readStatus() const { return static_cast<u8>(m_status); }
    void write(u8 command);
    void render(s16* out, u32 samples);

    void saveState(Buffer* buf);
    void loadState(Buffer* buf);

private:
    struct Channel {
        s32 output;
        s32 volume;
        s32 position;
        s32 sampleCount;
        s32 sample;
        s32 step;
        s32 delta;
        s32 playing;
    };

    static constexpr u32 PAGE_SHIFT = 8;
    static constexpr u32 PAGE_COUNT = 0x40000 >> PAGE_SHIFT;

    u8 readData(u32 addr) const;
    void stepChannels();
    template <typename Visit> void visitState(Visit visit);

    Channel m_channel[4] = {};
    const u8* m_page[PAGE_COUNT] = {};

    // A sample-start command arrives as two bytes; this holds the first one.
    bool m_isCommand = false;
    s32 m_sampleInfo = 0;

    u32 m_status = 0;

    s32 m_volume = 256;
    s32 m_sampleSize = 0;
    s32 m_fractionalPosition = 0;
    s32 m_currentSample = 0;
};

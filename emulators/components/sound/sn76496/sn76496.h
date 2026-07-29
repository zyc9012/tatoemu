#pragma once

#include "../../../types.h"
#include "../../buffer.h"

// ---------------------------------------------------------------------------
// Texas Instruments SN76489 programmable sound generator.
//
// Three square-wave tone channels plus one noise channel, each with a 4-bit
// attenuator.  This is the PSG integrated into the Mega Drive's VDP, clocked at
// the Z80 frequency and divided internally by 16.
// ---------------------------------------------------------------------------
class SN76496 {
public:
    SN76496();

    void init(u32 clock, u32 sampleRate);
    void reset();

    // Single write-only data port.
    void write(u8 value);

    // Renders `samples` mono samples, adding nothing (overwrites the buffer).
    void update(s16* buffer, u32 samples);

    void saveState(Buffer* buf);
    void loadState(Buffer* buf);

private:
    template <typename Visit> void visitState(Visit visit);

    void tick();
    s32 mix() const;

    u32 m_clock = 0;
    u32 m_sampleRate = 44100;
    u32 m_step = 0;       // 16.16 fixed point internal clocks per output sample
    u32 m_accumulator = 0;

    s32 m_period[4] = {};
    s32 m_counter[4] = {};
    u8  m_volume[4] = {};
    u8  m_output[4] = {};

    u16 m_noiseShift = 0x8000;
    u8  m_noiseMode = 0;

    u8  m_latchedChannel = 0;
    bool m_latchedVolume = false;

    s16 m_volumeTable[16] = {};
};

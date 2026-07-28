// Software emulation of the AY-3-8910 / YM2149 PSG.
//
// Three square wave tone channels, a shared noise generator and a shared
// envelope generator. TatoEmu only uses this as the SSG section of the YM2610,
// so the two 8-bit general purpose I/O ports are not implemented.
//
// Derived from the MAME driver, which is based on code by Ville Hallik,
// Michael Cuddy, Tatsuyuki Satoh, Fabrice Frances and Nicola Salmoria.
#pragma once

#include <array>

#include "../../types.h"
#include "../buffer.h"

class Ay8910 {
public:
    // Clears all state and recomputes the clock-derived step values.
    void init(s32 clock, s32 sampleRate);
    void setClock(s32 clock);
    void reset();

    // The chip has a single address line: even writes latch a register index,
    // odd writes store a value into the latched register.
    void write(s32 address, s32 data);
    s32 read() const;

    // Renders one sample per entry into three separate channel buffers; the
    // caller mixes them.
    void update(s16* channelA, s16* channelB, s16* channelC, s32 length);

    void saveState(Buffer* buf);
    void loadState(Buffer* buf);

private:
    static constexpr s32 NUM_TONES = 3;

    void writeReg(s32 reg, s32 value);
    void buildMixerTable();

    template <typename Visit>
    void visitState(Visit visit);

    s32 m_registerLatch = 0;
    std::array<u8, 16> m_regs = {};
    s32 m_lastEnable = 0;

    // Per tone channel state, indexed 0..2 for channels A, B and C. The noise
    // and envelope generators are shared, hence the separate N and E members.
    std::array<s32, NUM_TONES> m_period = {};
    s32 m_periodN = 0, m_periodE = 0;
    std::array<s32, NUM_TONES> m_count = {};
    s32 m_countN = 0, m_countE = 0;
    std::array<u32, NUM_TONES> m_vol = {};
    u32 m_volE = 0;
    std::array<u8, NUM_TONES> m_envelope = {};
    std::array<u8, NUM_TONES> m_output = {};
    u8 m_outputN = 0;

    s8 m_countEnv = 0;
    u8 m_hold = 0, m_alternate = 0, m_attack = 0, m_holding = 0;
    s32 m_rng = 0;

    // Derived from the clock and the host sample rate, so not part of the
    // saved state.
    u32 m_updateStep = 0;
    u32 m_updateStepN = 0;
    s32 m_sampleRate = 0;
    std::array<u32, 32> m_volTable = {};
};

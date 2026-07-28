// AY-3-8910 / YM2149 PSG. See ay8910.h for the overview.

#include "ay8910.h"

namespace {

constexpr s32 MAX_OUTPUT = 0x7fff;
constexpr s32 STEP = 0x8000;

enum {
    AY_AFINE    = 0,
    AY_ACOARSE  = 1,
    AY_BFINE    = 2,
    AY_BCOARSE  = 3,
    AY_CFINE    = 4,
    AY_CCOARSE  = 5,
    AY_NOISEPER = 6,
    AY_ENABLE   = 7,
    AY_AVOL     = 8,
    AY_BVOL     = 9,
    AY_CVOL     = 10,
    AY_EFINE    = 11,
    AY_ECOARSE  = 12,
    AY_ESHAPE   = 13,
    AY_PORTA    = 14,
    AY_PORTB    = 15,
};

} // namespace

// For speed we count a period down to zero, but the chip really counts up from
// zero until the counter reaches the period. That only matters while a program
// rapidly modulates the period, so adjust the counter whenever the period
// changes to compensate.
//
// A tone or noise period of 0 behaves as 1, as documented in the YM2203 data
// sheet. That does not hold for the envelope, where 0 is half of 1.
void Ay8910::writeReg(s32 reg, s32 value) {
    m_regs[reg] = static_cast<u8>(value);

    switch (reg) {
    case AY_AFINE:
    case AY_ACOARSE:
    case AY_BFINE:
    case AY_BCOARSE:
    case AY_CFINE:
    case AY_CCOARSE: {
        const s32 chan = reg / 2;
        const s32 coarse = AY_ACOARSE + chan * 2;
        m_regs[coarse] &= 0x0f;

        s32 old = m_period[chan];
        m_period[chan] = static_cast<s32>(
            (m_regs[coarse - 1] + 256 * m_regs[coarse]) * m_updateStep);
        if (m_period[chan] == 0) {
            m_period[chan] = static_cast<s32>(m_updateStep);
        }
        m_count[chan] += m_period[chan] - old;
        if (m_count[chan] <= 0) {
            m_count[chan] = 1;
        }
        break;
    }

    case AY_NOISEPER: {
        m_regs[AY_NOISEPER] &= 0x1f;
        s32 old = m_periodN;
        m_periodN = static_cast<s32>(m_regs[AY_NOISEPER] * m_updateStepN);
        if (m_periodN == 0) {
            m_periodN = static_cast<s32>(m_updateStepN);
        }
        m_countN += m_periodN - old;
        if (m_countN <= 0) {
            m_countN = 1;
        }
        break;
    }

    case AY_ENABLE:
        m_lastEnable = m_regs[AY_ENABLE];
        break;

    case AY_AVOL:
    case AY_BVOL:
    case AY_CVOL: {
        const s32 chan = reg - AY_AVOL;
        m_regs[reg] &= 0x1f;
        m_envelope[chan] = m_regs[reg] & 0x10;
        m_vol[chan] = m_envelope[chan] ? m_volE
                                       : m_volTable[m_regs[reg] ? m_regs[reg] * 2 + 1 : 0];
        break;
    }

    case AY_EFINE:
    case AY_ECOARSE: {
        s32 old = m_periodE;
        m_periodE = static_cast<s32>(
            (m_regs[AY_EFINE] + 256 * m_regs[AY_ECOARSE]) * m_updateStep);
        if (m_periodE == 0) {
            m_periodE = static_cast<s32>(m_updateStep / 2);
        }
        m_countE += m_periodE - old;
        if (m_countE <= 0) {
            m_countE = 1;
        }
        break;
    }

    case AY_ESHAPE:
        // Envelope shapes, by Continue / Attack / Alternate / Hold:
        //   0 0 x x  \___      1 1 0 0  ////
        //   0 1 x x  /___      1 1 0 1  /^^^
        //   1 0 0 0  \\\\      1 1 1 0  /\/\
        //   1 0 0 1  \___      1 1 1 1  /___
        //   1 0 1 0  \/\/
        //   1 0 1 1  \^^^
        //
        // The AY-3-8910 envelope counter has 16 steps and the YM2149 has twice
        // as many running twice as fast. The end result is just a smoother
        // curve, so always use the YM2149 behaviour.
        m_regs[AY_ESHAPE] &= 0x0f;
        m_attack = (m_regs[AY_ESHAPE] & 0x04) ? 0x1f : 0x00;
        if ((m_regs[AY_ESHAPE] & 0x08) == 0) {
            // With Continue = 0, map onto the equivalent Continue = 1 shape.
            m_hold = 1;
            m_alternate = m_attack;
        } else {
            m_hold = m_regs[AY_ESHAPE] & 0x01;
            m_alternate = m_regs[AY_ESHAPE] & 0x02;
        }
        m_countE = m_periodE;
        m_countEnv = 0x1f;
        m_holding = 0;
        m_volE = m_volTable[m_countEnv ^ m_attack];
        for (s32 chan = 0; chan < NUM_TONES; chan++) {
            if (m_envelope[chan]) {
                m_vol[chan] = m_volE;
            }
        }
        break;
    }
}

void Ay8910::write(s32 address, s32 data) {
    if (address & 1) {
        writeReg(m_registerLatch, data);
    } else {
        m_registerLatch = data & 0x0f;
    }
}

s32 Ay8910::read() const {
    return m_regs[m_registerLatch];
}

void Ay8910::update(s16* channelA, s16* channelB, s16* channelC, s32 length) {
    s16* out[NUM_TONES] = { channelA, channelB, channelC };

    // Each channel mixes as (ToneOn | ToneDisable) & (NoiseOn | NoiseDisable)
    // before the DAC, so with both tone and noise disabled the output sits at 1
    // rather than 0 and can still be modulated by changing the volume.
    //
    // A disabled channel is locked into that ON state, so force the output high
    // and skip the counter forward past this update. When the volume is zero
    // the output does not matter, but the counter still has to advance: adding
    // length (rather than assigning length + 1) avoids interference when a
    // program rapidly modulates the volume.
    for (s32 chan = 0; chan < NUM_TONES; chan++) {
        if (m_regs[AY_ENABLE] & (0x01 << chan)) {
            if (m_count[chan] <= length * STEP) {
                m_count[chan] += length * STEP;
            }
            m_output[chan] = 1;
        } else if (m_regs[AY_AVOL + chan] == 0) {
            if (m_count[chan] <= length * STEP) {
                m_count[chan] += length * STEP;
            }
        }
    }

    // The noise channel uses outn instead, so m_outputN must be left alone.
    if ((m_regs[AY_ENABLE] & 0x38) == 0x38 && m_countN <= length * STEP) {
        m_countN += length * STEP;
    }

    s32 outn = m_outputN | m_regs[AY_ENABLE];

    while (length) {
        // Tracks how long each square wave stays high during this sample.
        s32 vol[NUM_TONES] = {};

        s32 left = STEP;
        do {
            const s32 nextevent = (m_countN < left) ? m_countN : left;

            for (s32 chan = 0; chan < NUM_TONES; chan++) {
                const bool audible = (outn & (0x08 << chan)) != 0;

                if (audible && m_output[chan]) {
                    vol[chan] += m_count[chan];
                }
                m_count[chan] -= nextevent;

                // m_period holds the half period of the square wave, so adding
                // it twice per iteration leaves the wave in the same state it
                // started in and contributes exactly half the elapsed time to
                // vol. Exiting mid-way inverts the output instead, and only
                // counts towards vol when the wave ends up high.
                while (m_count[chan] <= 0) {
                    m_count[chan] += m_period[chan];
                    if (m_count[chan] > 0) {
                        m_output[chan] ^= 1;
                        if (audible && m_output[chan]) {
                            vol[chan] += m_period[chan];
                        }
                        break;
                    }
                    m_count[chan] += m_period[chan];
                    if (audible) {
                        vol[chan] += m_period[chan];
                    }
                }
                if (audible && m_output[chan]) {
                    vol[chan] -= m_count[chan];
                }
            }

            m_countN -= nextevent;
            if (m_countN <= 0) {
                if ((m_rng + 1) & 2) {      // is bit0 ^ bit1 set?
                    m_outputN = static_cast<u8>(~m_outputN);
                    outn = m_outputN | m_regs[AY_ENABLE];
                }

                // The RNG is a 17-bit shift register fed by bit0 XOR bit2, with
                // bit0 as the output. Rather than computing that directly, only
                // bit0 is tested: after two more shifts the current bit2 becomes
                // bit0 and inverts bit15, which was bit17 before the shifts.
                if (m_rng & 1) {
                    m_rng ^= 0x24000;
                }
                m_rng >>= 1;
                m_countN += m_periodN;
            }

            left -= nextevent;
        } while (left > 0);

        if (m_holding == 0) {
            m_countE -= STEP;
            if (m_countE <= 0) {
                do {
                    m_countEnv--;
                    m_countE += m_periodE;
                } while (m_countE <= 0);

                if (m_countEnv < 0) {
                    if (m_hold) {
                        if (m_alternate) {
                            m_attack ^= 0x1f;
                        }
                        m_holding = 1;
                        m_countEnv = 0;
                    } else {
                        // Invert the output if the counter has looped an odd
                        // number of times, which is usually once.
                        if (m_alternate && (m_countEnv & 0x20)) {
                            m_attack ^= 0x1f;
                        }
                        m_countEnv &= 0x1f;
                    }
                }

                m_volE = m_volTable[m_countEnv ^ m_attack];
                for (s32 chan = 0; chan < NUM_TONES; chan++) {
                    if (m_envelope[chan]) {
                        m_vol[chan] = m_volE;
                    }
                }
            }
        }

        for (s32 chan = 0; chan < NUM_TONES; chan++) {
            *(out[chan]++) = static_cast<s16>((vol[chan] * m_vol[chan]) / STEP);
        }

        length--;
    }
}

void Ay8910::setClock(s32 clock) {
    // The tone and noise generators step at the chip clock divided by 8. The
    // AY-3-8910 envelope steps at half that, but the YM2149 envelope runs twice
    // as fast, so it is clock/8 again. Work out how many steps happen during
    // one output sample; STEP turns that fraction into a fixed point number.
    //
    // The noise generator gets an extra /2 divider. That gives kncljoe's punch
    // the right timbre and fixes the "pepper" sound pitch in btime, without
    // affecting anything else.
    m_updateStep = static_cast<u32>((static_cast<double>(STEP) * m_sampleRate * 8 + clock / 2) / clock);
    m_updateStepN = static_cast<u32>((static_cast<double>(STEP) * m_sampleRate * 8 + clock / 2) / (clock / 2));
}

void Ay8910::buildMixerTable() {
    // The AY-3-8910 has 16 logarithmic levels at 3dB per step. The YM2149 keeps
    // 16 levels for the tone generators but has 32 for the envelope generator,
    // at 1.5dB per step.
    double out = MAX_OUTPUT;
    for (s32 i = 31; i > 0; i--) {
        m_volTable[i] = static_cast<u32>(out + 0.5);
        out /= 1.188502227;     // 10 ^ (1.5/20), ie. 1.5dB
    }
    m_volTable[0] = 0;
}

void Ay8910::reset() {
    m_registerLatch = 0;
    m_rng = 1;
    m_output = {};
    m_outputN = 0xff;
    m_lastEnable = -1;      // force a write
    for (s32 i = 0; i < AY_PORTA; i++) {
        writeReg(i, 0);
    }
}

void Ay8910::init(s32 clock, s32 sampleRate) {
    *this = Ay8910{};
    m_sampleRate = sampleRate;
    setClock(clock);
    buildMixerTable();
}

template <typename Visit>
void Ay8910::visitState(Visit visit) {
    visit(m_registerLatch);
    visit(m_regs);
    visit(m_lastEnable);
    visit(m_period);   visit(m_periodN); visit(m_periodE);
    visit(m_count);    visit(m_countN);  visit(m_countE);
    visit(m_vol);      visit(m_volE);
    visit(m_envelope);
    visit(m_output);   visit(m_outputN);
    visit(m_countEnv);
    visit(m_hold); visit(m_alternate); visit(m_attack); visit(m_holding);
    visit(m_rng);
}

void Ay8910::saveState(Buffer* buf) {
    if (!buf) {
        return;
    }
    visitState(StateWriter{buf});
}

void Ay8910::loadState(Buffer* buf) {
    if (!buf) {
        return;
    }
    visitState(StateReader{buf});
}

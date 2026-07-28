// Yamaha Delta-T (ADPCM-B) synthesis, used by the Y8950, the YM2608 and the
// YM2610/B.
//
// Base program is the YM2610 emulator by Hiromitsu Shioya, written by
// Tatsuyuki Satoh, with improvements by Jarek Burczynski.

#include "../../../types.h"
#include "../../buffer.h"
#include "ymdeltat.h"

static constexpr s32 DELTA_MAX = 24576;
static constexpr s32 DELTA_MIN = 127;
static constexpr s32 DELTA_DEF = 127;

static constexpr s32 DECODE_RANGE = 32768;
static constexpr s32 DECODE_MIN = -DECODE_RANGE;
static constexpr s32 DECODE_MAX = DECODE_RANGE - 1;

// Forecast to next forecast (rate = *8):
// 1/8, 3/8, 5/8, 7/8, 9/8, 11/8, 13/8, 15/8
static constexpr s32 DECODE_TABLE_B1[16] = {
     1,   3,   5,   7,   9,  11,  13,  15,
    -1,  -3,  -5,  -7,  -9, -11, -13, -15,
};

// Delta to next delta (rate = *64):
// 0.9, 0.9, 0.9, 0.9, 1.2, 1.6, 2.0, 2.4
static constexpr s32 DECODE_TABLE_B2[16] = {
    57,  57,  57,  57, 77, 102, 128, 153,
    57,  57,  57,  57, 77, 102, 128, 153,
};

// 0 = DRAM x1, 1 = ROM, 2 = DRAM x8, 3 = ROM.
// 3 is a bad setting, not allowed by the manual.
static constexpr u8 DRAM_RIGHT_SHIFT[4] = { 3, 0, 0, 0 };

static inline void limit(s32& value, s32 max, s32 min) {
    if (value > max) {
        value = max;
    } else if (value < min) {
        value = min;
    }
}

u8 YmDeltaT::read() {
    u8 v = 0;

    // External memory read
    if ((m_portState & 0xe0) == 0x20) {
        // Two dummy reads
        if (m_memRead) {
            m_nowAddr = m_start << 1;
            m_memRead--;
            return 0;
        }

        if (m_nowAddr != (m_end << 1)) {
            v = m_memory[m_nowAddr >> 1];

            m_nowAddr += 2;     // two nibbles at a time

            // Clear BRDY while the read is in flight, then set it again. On
            // hardware the gap is about 10 master clocks; doing it in zero
            // time is enough for the IRQ to fire.
            if (m_statusResetHandler && m_statusChangeBrdyBit) {
                m_statusResetHandler(m_statusChangeBrdyBit);
            }
            if (m_statusSetHandler && m_statusChangeBrdyBit) {
                m_statusSetHandler(m_statusChangeBrdyBit);
            }
        } else {
            if (m_statusSetHandler && m_statusChangeEosBit) {
                m_statusSetHandler(m_statusChangeEosBit);
            }
        }
    }

    return v;
}



// START (D7)
//   Accessing external memory starts when this is set, so every other
//   condition has to be in place first. For CPU-managed memory, playback
//   starts on the first read or write of the ADPCM data register 0x08.
// REC
//   0 = ADPCM synthesis (playback), 1 = ADPCM analysis (record).
// MEMDATA
//   0 = processor memory (register 0x08), 1 = external memory (using the
//   start/end/limit registers).
// SPOFF
//   Disables the speaker while recording.
// RESET and REPEAT only work with external memory.
//
// value:   START, REC, MEMDAT, REPEAT, SPOFF, x,x,RESET
//   0xC8   1      1    0       0       1                 record AUDIO -> CPU
//   0xE8   1      1    1       0       1                 record AUDIO -> memory
//   0x80   1      0    0       0       0                 play CPU -> AUDIO
//   0xA0   1      0    1       0       0                 play memory -> AUDIO
//   0x60   0      1    1       0       0                 memory write via 0x08
//   0x20   0      0    1       0       0                 memory read via 0x08
void YmDeltaT::write(s32 reg, s32 value) {
    if (reg >= 0x10) {
        return;
    }
    m_reg[reg] = static_cast<u8>(value);

    switch (reg) {
    case 0x00:
        // The YM2610 always uses external memory and does not even have the
        // memory flag bit.
        if (m_emulationMode == EmulationMode::Ym2610) {
            value |= 0x20;
        }

        // START, REC, memory mode, repeat flag copy, reset
        m_portState = static_cast<u8>(value & (0x80 | 0x40 | 0x20 | 0x10 | 0x01));

        if (m_portState & 0x80) {
            m_pcmBusy = 1;

            // Start ADPCM
            m_nowStep = 0;
            m_acc = 0;
            m_prevAcc = 0;
            m_adpcmL = 0;
            m_adpcmD = DELTA_DEF;
            m_nowData = 0;
        }

        if (m_portState & 0x20) {   // external memory
            m_nowAddr = m_start << 1;
            m_memRead = 2;          // two dummy reads before register 0x08 works

            // Check that ADPCM memory is mapped and big enough.
            if (m_memory == nullptr) {
                m_portState = 0x00;
                m_pcmBusy = 0;
            } else {
                if (m_end >= m_memorySize) {
                    m_end = m_memorySize - 1;
                }
                if (m_start >= m_memorySize) {
                    m_portState = 0x00;
                    m_pcmBusy = 0;
                }
            }
        } else {
            // CPU memory (register 0x08), so only the address is reset here.
            m_nowAddr = 0;
        }

        if (m_portState & 0x01) {
            m_portState = 0x00;
            m_pcmBusy = 0;

            if (m_statusSetHandler && !m_postloading && m_statusChangeBrdyBit) {
                m_statusSetHandler(m_statusChangeBrdyBit);
            }
        }
        break;

    case 0x01:  // L,R,-,-,SAMPLE,DA/AD,RAMTYPE,ROM
        // The YM2610 always uses ROM as external memory and has no ROM/RAM bit.
        if (m_emulationMode == EmulationMode::Ym2610) {
            value |= 0x01;
        }

        m_pan = &m_outputPointer[(value >> 6) & 0x03];
        if ((m_control2 & 3) != (value & 3)) {
            if (m_dramPortShift != DRAM_RIGHT_SHIFT[value & 3]) {
                m_dramPortShift = DRAM_RIGHT_SHIFT[value & 3];

                // The final shift depends on the chip and the memory type:
                // 8 for the YM2610 (ROM only), 5 for ROM and x8bit DRAM on the
                // Y8950 and YM2608, 2 for their x1bit DRAM.
                m_start = (m_reg[0x3] * 0x0100 | m_reg[0x2]) << (m_portShift - m_dramPortShift);
                m_end = (m_reg[0x5] * 0x0100 | m_reg[0x4]) << (m_portShift - m_dramPortShift);
                m_end += (1 << (m_portShift - m_dramPortShift)) - 1;
                m_limit = (m_reg[0xd] * 0x0100 | m_reg[0xc]) << (m_portShift - m_dramPortShift);
            }
        }
        m_control2 = static_cast<u8>(value);
        break;

    case 0x02:  // start address L
    case 0x03:  // start address H
        m_start = (m_reg[0x3] * 0x0100 | m_reg[0x2]) << (m_portShift - m_dramPortShift);
        break;

    case 0x04:  // stop address L
    case 0x05:  // stop address H
        m_end = (m_reg[0x5] * 0x0100 | m_reg[0x4]) << (m_portShift - m_dramPortShift);
        m_end += (1 << (m_portShift - m_dramPortShift)) - 1;
        break;

    case 0x06:  // prescale L (ADPCM and record frequency)
    case 0x07:  // prescale H
        break;

    case 0x08:  // ADPCM data
        // External memory write
        if ((m_portState & 0xe0) == 0x60) {
            if (m_memRead) {
                m_nowAddr = m_start << 1;
                m_memRead = 0;
            }

            if (m_nowAddr != (m_end << 1)) {
                m_memory[m_nowAddr >> 1] = static_cast<u8>(value);
                m_nowAddr += 2;     // two nibbles at a time

                // Clear BRDY while the write is in flight, then set it again.
                if (m_statusResetHandler && !m_postloading && m_statusChangeBrdyBit) {
                    m_statusResetHandler(m_statusChangeBrdyBit);
                }
                if (m_statusSetHandler && !m_postloading && m_statusChangeBrdyBit) {
                    m_statusSetHandler(m_statusChangeBrdyBit);
                }
            } else {
                if (m_statusSetHandler && !m_postloading && m_statusChangeEosBit) {
                    m_statusSetHandler(m_statusChangeEosBit);
                }
            }

            return;
        }

        // ADPCM synthesis from the CPU
        if ((m_portState & 0xe0) == 0x80) {
            m_cpuData = static_cast<u8>(value);

            // Clear BRDY, meaning the unit is full of data.
            if (m_statusResetHandler && !m_postloading && m_statusChangeBrdyBit) {
                m_statusResetHandler(m_statusChangeBrdyBit);
            }
            return;
        }
        break;

    case 0x09:  // DELTA-N L (ADPCM playback prescaler)
    case 0x0a:  // DELTA-N H
        m_delta = (m_reg[0xa] * 0x0100 | m_reg[0x9]);
        m_step = static_cast<u32>(static_cast<double>(m_delta) * m_freqBase);
        break;

    case 0x0b: {  // output level control (volume, linear)
        s32 oldVolume = m_volume;
        // v * ((1 << 23) >> 8) >> 15, so the output range has to be at least
        // 1 << (15 + 8).
        m_volume = (value & 0xff) * (m_outputRange / 256) / DECODE_RANGE;
        if (oldVolume != 0) {
            m_adpcmL = static_cast<s32>(static_cast<double>(m_adpcmL) /
                                        static_cast<double>(oldVolume) *
                                        static_cast<double>(m_volume));
        }
        break;
    }

    case 0x0c:  // limit address L
    case 0x0d:  // limit address H
        m_limit = (m_reg[0xd] * 0x0100 | m_reg[0xc]) << (m_portShift - m_dramPortShift);
        break;
    }
}


void YmDeltaT::reset(s32 pan, EmulationMode mode) {
    m_nowAddr = 0;
    m_nowStep = 0;
    m_step = 0;
    m_start = 0;
    m_end = 0;

    // Neither the YM2610 nor the Y8950 has a limit address register, so this
    // way they both still work.
    m_limit = ~0u;

    m_volume = 0;
    m_pan = &m_outputPointer[pan];
    m_acc = 0;
    m_prevAcc = 0;
    m_adpcmD = 127;
    m_adpcmL = 0;
    m_emulationMode = mode;
    m_portState = (mode == EmulationMode::Ym2610) ? 0x20 : 0;

    // The default depends on the emulation mode: the MSX demo "facdemo_4"
    // never sets up control2 and still has to work.
    m_control2 = (mode == EmulationMode::Ym2610) ? 0x01 : 0;
    m_dramPortShift = DRAM_RIGHT_SHIFT[m_control2 & 3];

    // The flag mask register disables BRDY after a reset, but as soon as the
    // mask is enabled the flag needs to be set.
    if (m_statusSetHandler && m_statusChangeBrdyBit) {
        m_statusSetHandler(m_statusChangeBrdyBit);
    }
}

void YmDeltaT::synthesisFromExternalMemory() {
    m_nowStep += m_step;
    if (m_nowStep >= (1 << SHIFT)) {
        u32 step = m_nowStep >> SHIFT;
        m_nowStep &= (1 << SHIFT) - 1;
        do {
            if (m_nowAddr == (m_limit << 1)) {
                m_nowAddr = 0;
            }

            if (m_nowAddr >= (m_memorySize << 1)) {
                // Guards against a crash in gwar when a state is loaded while
                // the player death sample is playing.
                if (m_statusSetHandler && m_statusChangeEosBit) {
                    m_statusSetHandler(m_statusChangeEosBit);
                }

                m_pcmBusy = 0;
                m_portState = 0;
                m_adpcmL = 0;
                m_prevAcc = 0;
                return;
            }

            if (m_nowAddr == (m_end << 1)) {
                if (m_portState & 0x10) {
                    // Repeat from the start
                    m_nowAddr = m_start << 1;
                    m_acc = 0;
                    m_adpcmD = DELTA_DEF;
                    m_prevAcc = 0;
                } else {
                    if (m_statusSetHandler && m_statusChangeEosBit) {
                        m_statusSetHandler(m_statusChangeEosBit);
                    }

                    m_pcmBusy = 0;
                    m_portState = 0;
                    m_adpcmL = 0;
                    m_prevAcc = 0;
                    return;
                }
            }

            s32 data;
            if (m_nowAddr & 1) {
                data = m_nowData & 0x0f;
            } else {
                m_nowData = m_memory[m_nowAddr >> 1];
                data = m_nowData >> 4;
            }

            m_nowAddr++;

            // The YM2610 address register is 24 bits wide; the extra bit is
            // the nibble select. This ignores the size of the mapped ROM.
            m_nowAddr &= (1 << (24 + 1)) - 1;

            m_prevAcc = m_acc;

            // Forecast to next forecast
            m_acc += DECODE_TABLE_B1[data] * m_adpcmD / 8;
            limit(m_acc, DECODE_MAX, DECODE_MIN);

            // Delta to next delta
            m_adpcmD = m_adpcmD * DECODE_TABLE_B2[data] / 64;
            limit(m_adpcmD, DELTA_MAX, DELTA_MIN);
        } while (--step);
    }

    // Interpolate between the previous and the current accumulator.
    m_adpcmL = m_prevAcc * static_cast<s32>((1 << SHIFT) - m_nowStep);
    m_adpcmL += m_acc * static_cast<s32>(m_nowStep);
    m_adpcmL = (m_adpcmL >> SHIFT) * m_volume;

    *m_pan += m_adpcmL;
}

void YmDeltaT::synthesisFromCpuMemory() {
    m_nowStep += m_step;
    if (m_nowStep >= (1 << SHIFT)) {
        u32 step = m_nowStep >> SHIFT;
        m_nowStep &= (1 << SHIFT) - 1;
        do {
            s32 data;
            if (m_nowAddr & 1) {
                data = m_nowData & 0x0f;

                m_nowData = m_cpuData;

                // The byte from register 0x08 has been consumed, so set BRDY
                // to ask for another one.
                if (m_statusSetHandler && m_statusChangeBrdyBit) {
                    m_statusSetHandler(m_statusChangeBrdyBit);
                }
            } else {
                data = m_nowData >> 4;
            }

            m_nowAddr++;

            m_prevAcc = m_acc;

            // Forecast to next forecast
            m_acc += DECODE_TABLE_B1[data] * m_adpcmD / 8;
            limit(m_acc, DECODE_MAX, DECODE_MIN);

            // Delta to next delta
            m_adpcmD = m_adpcmD * DECODE_TABLE_B2[data] / 64;
            limit(m_adpcmD, DELTA_MAX, DELTA_MIN);
        } while (--step);
    }

    // Interpolate between the previous and the current accumulator.
    m_adpcmL = m_prevAcc * static_cast<s32>((1 << SHIFT) - m_nowStep);
    m_adpcmL += m_acc * static_cast<s32>(m_nowStep);
    m_adpcmL = (m_adpcmL >> SHIFT) * m_volume;

    *m_pan += m_adpcmL;
}

void YmDeltaT::calc() {
    if ((m_portState & 0xe0) == 0xa0) {
        synthesisFromExternalMemory();
        return;
    }

    if ((m_portState & 0xe0) == 0x80) {
        // Playing from CPU-managed memory (register 0x08)
        synthesisFromCpuMemory();
        return;
    }

    // ADPCM analysis (recording, port states 0xc0 and 0xe0) is not emulated.
}

// Software emulation of the Yamaha OPN FM sound generator.
//
// Copyright (C) 2001, 2002, 2003 Jarek Burczynski
// Copyright (C) 1998 Tatsuyuki Satoh, MultiArcadeMachineEmulator development
//
// YM2610  (OPNB) : SSG 3ch, FM 4ch, ADPCM-A 6ch, ADPCM-B 1ch
// YM2612  (OPN2) : FM 6ch with a DAC on channel 6

#include <array>
#include <cmath>

#include "fm.h"

// The busy flag would need the present time in seconds with double precision.
// TatoEmu runs the chips a frame at a time and has no such clock, so the busy
// flag always reads as "not busy".
static constexpr double timeNow() { return 0.0; }

// Chip capability flags.
static constexpr u8 TYPE_SSG    = 0x01;     // SSG support
static constexpr u8 TYPE_LFOPAN = 0x02;     // OPN type LFO and PAN
static constexpr u8 TYPE_6CH    = 0x04;     // FM 6CH / 3CH
static constexpr u8 TYPE_DAC    = 0x08;     // YM2612's DAC device
static constexpr u8 TYPE_ADPCM  = 0x10;     // two ADPCM units
static constexpr u8 TYPE_2610   = 0x20;     // tells the 2610 apart from the 2608

static constexpr u8 TYPE_YM2608 = TYPE_SSG | TYPE_LFOPAN | TYPE_6CH | TYPE_ADPCM;
static constexpr u8 TYPE_YM2610 = TYPE_SSG | TYPE_LFOPAN | TYPE_6CH | TYPE_ADPCM | TYPE_2610;
static constexpr u8 TYPE_YM2612 = TYPE_DAC | TYPE_LFOPAN | TYPE_6CH;

static constexpr s32 FREQ_SH  = 16;     // 16.16 fixed point (frequency calculations)
static constexpr s32 EG_SH    = 16;     // 16.16 fixed point (envelope generator timing)
static constexpr s32 LFO_SH   = 24;     //  8.24 fixed point (LFO calculations)
static constexpr s32 FREQ_MASK = (1 << FREQ_SH) - 1;

static constexpr s32 ENV_BITS = 10;
static constexpr s32 ENV_LEN  = 1 << ENV_BITS;
static constexpr double ENV_STEP = 128.0 / ENV_LEN;

static constexpr s32 MAX_ATT_INDEX = ENV_LEN - 1;   // 1023
static constexpr s32 MIN_ATT_INDEX = 0;

// Envelope generator phases.
static constexpr u8 EG_ATT = 4;
static constexpr u8 EG_DEC = 3;
static constexpr u8 EG_SUS = 2;
static constexpr u8 EG_REL = 1;
static constexpr u8 EG_OFF = 0;

static constexpr s32 SIN_BITS = 10;
static constexpr s32 SIN_LEN  = 1 << SIN_BITS;
static constexpr s32 SIN_MASK = SIN_LEN - 1;

static constexpr s32 TL_RES_LEN = 256;      // 8 bits addressing (real chip)

static constexpr s32 MAXOUT = +32767;
static constexpr s32 MINOUT = -32768;

// 13 sinus amplitude bits and 2 sign bits on the Y axis, TL_RES_LEN on the X.
static constexpr s32 TL_TAB_LEN = 13 * 2 * TL_RES_LEN;

// pow() and floor() are not constexpr, so this runs once at static init.
static std::array<s32, TL_TAB_LEN> makeTlTab() {
    std::array<s32, TL_TAB_LEN> tab{};

    for (s32 x = 0; x < TL_RES_LEN; x++) {
        double m = (1 << 16) / pow(2, (x + 1) * (ENV_STEP / 4.0) / 8.0);
        m = floor(m);

        // The (x + 1) keeps this below (1 << 16), so it fits in 16 bits.
        s32 n = static_cast<s32>(m);    // 16 bits here
        n >>= 4;                        // 12 bits here
        if (n & 1) {                    // round to nearest
            n = (n >> 1) + 1;
        } else {
            n = n >> 1;
        }
                                        // 11 bits here (rounded)
        n <<= 2;                        // 13 bits here (as in the real chip)
        tab[x * 2 + 0] = n;
        tab[x * 2 + 1] = -tab[x * 2 + 0];

        for (s32 i = 1; i < 13; i++) {
            tab[x * 2 + 0 + i * 2 * TL_RES_LEN] =  tab[x * 2 + 0] >> i;
            tab[x * 2 + 1 + i * 2 * TL_RES_LEN] = -tab[x * 2 + 0 + i * 2 * TL_RES_LEN];
        }
    }
    return tab;
}
static const std::array<s32, TL_TAB_LEN> tlTab = makeTlTab();

static constexpr s32 ENV_QUIET = TL_TAB_LEN >> 3;

// Sine waveform table in 'decibel' scale.
// sin() and log() are not constexpr, so this runs once at static init.
static std::array<u32, SIN_LEN> makeSinTab() {
    std::array<u32, SIN_LEN> tab{};

    constexpr double pi = 3.14159265358979323846;

    for (s32 i = 0; i < SIN_LEN; i++) {
        // Non-standard sinus, checked against the real chip. The ((i * 2) + 1)
        // keeps this away from zero.
        double m = sin(((i * 2) + 1) * pi / SIN_LEN);

        // Convert to 'decibels'
        double o = (m > 0.0) ? 8 * log(1.0 / m) / log(2.0)
                             : 8 * log(-1.0 / m) / log(2.0);

        o = o / (ENV_STEP / 4);

        s32 n = static_cast<s32>(2.0 * o);
        if (n & 1) {                    // round to nearest
            n = (n >> 1) + 1;
        } else {
            n = n >> 1;
        }

        tab[i] = n * 2 + (m >= 0.0 ? 0 : 1);
    }
    return tab;
}
static const std::array<u32, SIN_LEN> sinTab = makeSinTab();

// Sustain level table, 3dB per step:
//   value  1     2    4  8   16  32  64
//   dB     0.75  1.5  3  6   12  24  48
// Entries 0-15 are 0, 3, 6, 9, 12, 15, 18, 21, 24, 27, 30, 33, 36, 39, 42, 93 dB.
#define SC(db) (u32) ( db * (4.0/ENV_STEP) )
static constexpr u32 slTable[16] = {
    SC( 0), SC( 1), SC( 2), SC( 3), SC( 4), SC( 5), SC( 6), SC( 7),
    SC( 8), SC( 9), SC(10), SC(11), SC(12), SC(13), SC(14), SC(31),
};
#undef SC


static constexpr s32 RATE_STEPS = 8;

// Envelope increment per cycle, for cycles 0..7.
static constexpr u8 egInc[19 * RATE_STEPS] = {

/*cycle:0 1  2 3  4 5  6 7*/

/* 0 */ 0,1, 0,1, 0,1, 0,1, /* rates 00..11 0 (increment by 0 or 1) */
/* 1 */ 0,1, 0,1, 1,1, 0,1, /* rates 00..11 1 */
/* 2 */ 0,1, 1,1, 0,1, 1,1, /* rates 00..11 2 */
/* 3 */ 0,1, 1,1, 1,1, 1,1, /* rates 00..11 3 */

/* 4 */ 1,1, 1,1, 1,1, 1,1, /* rate 12 0 (increment by 1) */
/* 5 */ 1,1, 1,2, 1,1, 1,2, /* rate 12 1 */
/* 6 */ 1,2, 1,2, 1,2, 1,2, /* rate 12 2 */
/* 7 */ 1,2, 2,2, 1,2, 2,2, /* rate 12 3 */

/* 8 */ 2,2, 2,2, 2,2, 2,2, /* rate 13 0 (increment by 2) */
/* 9 */ 2,2, 2,4, 2,2, 2,4, /* rate 13 1 */
/*10 */ 2,4, 2,4, 2,4, 2,4, /* rate 13 2 */
/*11 */ 2,4, 4,4, 2,4, 4,4, /* rate 13 3 */

/*12 */ 4,4, 4,4, 4,4, 4,4, /* rate 14 0 (increment by 4) */
/*13 */ 4,4, 4,8, 4,4, 4,8, /* rate 14 1 */
/*14 */ 4,8, 4,8, 4,8, 4,8, /* rate 14 2 */
/*15 */ 4,8, 8,8, 4,8, 8,8, /* rate 14 3 */

/*16 */ 8,8, 8,8, 8,8, 8,8, /* rates 15 0, 15 1, 15 2, 15 3 (increment by 8) */
/*17 */ 16,16,16,16,16,16,16,16, /* rates 15 2, 15 3 for attack */
/*18 */ 0,0, 0,0, 0,0, 0,0, /* infinity rates for attack and decay(s) */
};


#define O(a) (a*RATE_STEPS)

// Envelope generator rates: 32 infinite + 64 rates + 32 RKS.
// Note that there is no O(17) in this table - it is applied directly in the code.
static constexpr u8 egRateSelect[32+64+32]={
/* 32 infinite time rates */
O(18),O(18),O(18),O(18),O(18),O(18),O(18),O(18),
O(18),O(18),O(18),O(18),O(18),O(18),O(18),O(18),
O(18),O(18),O(18),O(18),O(18),O(18),O(18),O(18),
O(18),O(18),O(18),O(18),O(18),O(18),O(18),O(18),

/* rates 00-11 */
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),

/* rate 12 */
O( 4),O( 5),O( 6),O( 7),

/* rate 13 */
O( 8),O( 9),O(10),O(11),

/* rate 14 */
O(12),O(13),O(14),O(15),

/* rate 15 */
O(16),O(16),O(16),O(16),

/* 32 dummy rates (same as 15 3) */
O(16),O(16),O(16),O(16),O(16),O(16),O(16),O(16),
O(16),O(16),O(16),O(16),O(16),O(16),O(16),O(16),
O(16),O(16),O(16),O(16),O(16),O(16),O(16),O(16),
O(16),O(16),O(16),O(16),O(16),O(16),O(16),O(16)

};

// The YM2612 differs from the YM2610 at the bottom of the rate table.
static constexpr u8 egRateSelect2612[32+64+32]={
/* 32 infinite time rates */
O(18),O(18),O(18),O(18),O(18),O(18),O(18),O(18),
O(18),O(18),O(18),O(18),O(18),O(18),O(18),O(18),
O(18),O(18),O(18),O(18),O(18),O(18),O(18),O(18),
O(18),O(18),O(18),O(18),O(18),O(18),O(18),O(18),

/* rates 00-11 */
O( 18),O( 18),O( 0),O( 0),
O( 0),O( 0),O( 2),O( 2),  // Nemesis's tests

O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),

/* rate 12 */
O( 4),O( 5),O( 6),O( 7),

/* rate 13 */
O( 8),O( 9),O(10),O(11),

/* rate 14 */
O(12),O(13),O(14),O(15),

/* rate 15 */
O(16),O(16),O(16),O(16),

/* 32 dummy rates (same as 15 3) */
O(16),O(16),O(16),O(16),O(16),O(16),O(16),O(16),
O(16),O(16),O(16),O(16),O(16),O(16),O(16),O(16),
O(16),O(16),O(16),O(16),O(16),O(16),O(16),O(16),
O(16),O(16),O(16),O(16),O(16),O(16),O(16),O(16)

};
#undef O

// rate   0     1     2    3    4    5   6   7   8  9  10  11  12  13  14  15
// shift  11    10    9    8    7    6   5   4   3  2  1   0   0   0   0   0
// mask   2047  1023  511  255  127  63  31  15  7  3  1   0   0   0   0   0

#define O(a) (a*1)

// Envelope generator counter shifts: 32 infinite + 64 rates + 32 RKS.
static constexpr u8 egRateShift[32+64+32]={
/* 32 infinite time rates */
O(0),O(0),O(0),O(0),O(0),O(0),O(0),O(0),
O(0),O(0),O(0),O(0),O(0),O(0),O(0),O(0),
O(0),O(0),O(0),O(0),O(0),O(0),O(0),O(0),
O(0),O(0),O(0),O(0),O(0),O(0),O(0),O(0),

/* rates 00-11 */
O(11),O(11),O(11),O(11),
O(10),O(10),O(10),O(10),
O( 9),O( 9),O( 9),O( 9),
O( 8),O( 8),O( 8),O( 8),
O( 7),O( 7),O( 7),O( 7),
O( 6),O( 6),O( 6),O( 6),
O( 5),O( 5),O( 5),O( 5),
O( 4),O( 4),O( 4),O( 4),
O( 3),O( 3),O( 3),O( 3),
O( 2),O( 2),O( 2),O( 2),
O( 1),O( 1),O( 1),O( 1),
O( 0),O( 0),O( 0),O( 0),

/* rate 12 */
O( 0),O( 0),O( 0),O( 0),

/* rate 13 */
O( 0),O( 0),O( 0),O( 0),

/* rate 14 */
O( 0),O( 0),O( 0),O( 0),

/* rate 15 */
O( 0),O( 0),O( 0),O( 0),

/* 32 dummy rates (same as 15 3) */
O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),
O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),
O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),
O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),O( 0)

};
#undef O

// YM2151 and YM2612 detune phase increments, in 10.10 fixed point.
static constexpr u8 dtTab[4 * 32]={
/* FD=0 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* FD=1 */
    0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2,
    2, 3, 3, 3, 4, 4, 4, 5, 5, 6, 6, 7, 8, 8, 8, 8,
/* FD=2 */
    1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5,
    5, 6, 6, 7, 8, 8, 9,10,11,12,13,14,16,16,16,16,
/* FD=3 */
    2, 2, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5, 6, 6, 7,
    8 , 8, 9,10,11,12,13,14,16,17,19,20,22,22,22,22
};


// OPN key frequency number -> key code.
// The top 4 bits of fnum select the bottom 2 bits of the key code.
static constexpr u8 opnFkTable[16] = {0,0,0,0,0,0,0,1,2,3,3,3,3,3,3,3};


// 8 LFO speeds, each the number of samples that one LFO level lasts for.
static constexpr u32 lfoSamplesPerStep[8] = {108, 77, 71, 67, 62, 44, 8, 5};



// There are 4 LFO AM depths: 0 dB, 1.4 dB, 5.9 dB and 11.8 dB. In EG steps:
//
//   11.8 dB = 0, 2, 4, 6, 8, 10,12,14,16...126,126,124,122,120,118,....4,2,0
//    5.9 dB = 0, 1, 2, 3, 4, 5, 6, 7, 8....63, 63, 62, 61, 60, 59,.....2,1,0
//    1.4 dB = 0, 0, 0, 0, 1, 1, 1, 1, 2,...15, 15, 15, 15, 14, 14,.....0,0,0
//
// (1.4 dB loses precision, as you can see.)
//
// It is implemented as a generator from 0..126 with step 2, then shifted right
// by the amount below.
static constexpr u8 lfoAmsDepthShift[4] = {8, 3, 1, 0};



// There are 8 LFO PM depths: 0, 3.4, 6.7, 10, 14, 20, 40 and 80 cents.
//
// The modulation level at each depth depends on F-NUMBER bits 4..10, where bits
// 8..10 are the FNUM MSBs from the OCT/FNUM register.
//
// Only the first (positive) quarter of the waveform is stored here; the full
// 128-waveform lfoPmTable is built at init time.
//
// One value below represents 4 basic LFO steps, since 1 PM step = 4 AM steps.
// For example at LFO SPEED=0, which is 108 samples per basic LFO step, one
// value lasts 432 samples (4*108) and one full cycle lasts 13824 samples
// (32*432; 32 because only a quarter of the waveform is stored).
//
// 7 meaningful bits of F-NUMBER, 8 LFO output levels per depth (out of 32),
// 8 LFO depths.
static constexpr u8 lfoPmOutput[7*8][8]={
/* FNUM BIT 4: 000 0001xxxx */
/* DEPTH 0 */ {0,   0,   0,   0,   0,   0,   0,   0},
/* DEPTH 1 */ {0,   0,   0,   0,   0,   0,   0,   0},
/* DEPTH 2 */ {0,   0,   0,   0,   0,   0,   0,   0},
/* DEPTH 3 */ {0,   0,   0,   0,   0,   0,   0,   0},
/* DEPTH 4 */ {0,   0,   0,   0,   0,   0,   0,   0},
/* DEPTH 5 */ {0,   0,   0,   0,   0,   0,   0,   0},
/* DEPTH 6 */ {0,   0,   0,   0,   0,   0,   0,   0},
/* DEPTH 7 */ {0,   0,   0,   0,   1,   1,   1,   1},

/* FNUM BIT 5: 000 0010xxxx */
/* DEPTH 0 */ {0,   0,   0,   0,   0,   0,   0,   0},
/* DEPTH 1 */ {0,   0,   0,   0,   0,   0,   0,   0},
/* DEPTH 2 */ {0,   0,   0,   0,   0,   0,   0,   0},
/* DEPTH 3 */ {0,   0,   0,   0,   0,   0,   0,   0},
/* DEPTH 4 */ {0,   0,   0,   0,   0,   0,   0,   0},
/* DEPTH 5 */ {0,   0,   0,   0,   0,   0,   0,   0},
/* DEPTH 6 */ {0,   0,   0,   0,   1,   1,   1,   1},
/* DEPTH 7 */ {0,   0,   1,   1,   2,   2,   2,   3},

/* FNUM BIT 6: 000 0100xxxx */
/* DEPTH 0 */ {0,   0,   0,   0,   0,   0,   0,   0},
/* DEPTH 1 */ {0,   0,   0,   0,   0,   0,   0,   0},
/* DEPTH 2 */ {0,   0,   0,   0,   0,   0,   0,   0},
/* DEPTH 3 */ {0,   0,   0,   0,   0,   0,   0,   0},
/* DEPTH 4 */ {0,   0,   0,   0,   0,   0,   0,   1},
/* DEPTH 5 */ {0,   0,   0,   0,   1,   1,   1,   1},
/* DEPTH 6 */ {0,   0,   1,   1,   2,   2,   2,   3},
/* DEPTH 7 */ {0,   0,   2,   3,   4,   4,   5,   6},

/* FNUM BIT 7: 000 1000xxxx */
/* DEPTH 0 */ {0,   0,   0,   0,   0,   0,   0,   0},
/* DEPTH 1 */ {0,   0,   0,   0,   0,   0,   0,   0},
/* DEPTH 2 */ {0,   0,   0,   0,   0,   0,   1,   1},
/* DEPTH 3 */ {0,   0,   0,   0,   1,   1,   1,   1},
/* DEPTH 4 */ {0,   0,   0,   1,   1,   1,   1,   2},
/* DEPTH 5 */ {0,   0,   1,   1,   2,   2,   2,   3},
/* DEPTH 6 */ {0,   0,   2,   3,   4,   4,   5,   6},
/* DEPTH 7 */ {0,   0,   4,   6,   8,   8, 0xa, 0xc},

/* FNUM BIT 8: 001 0000xxxx */
/* DEPTH 0 */ {0,   0,   0,   0,   0,   0,   0,   0},
/* DEPTH 1 */ {0,   0,   0,   0,   1,   1,   1,   1},
/* DEPTH 2 */ {0,   0,   0,   1,   1,   1,   2,   2},
/* DEPTH 3 */ {0,   0,   1,   1,   2,   2,   3,   3},
/* DEPTH 4 */ {0,   0,   1,   2,   2,   2,   3,   4},
/* DEPTH 5 */ {0,   0,   2,   3,   4,   4,   5,   6},
/* DEPTH 6 */ {0,   0,   4,   6,   8,   8, 0xa, 0xc},
/* DEPTH 7 */ {0,   0,   8, 0xc,0x10,0x10,0x14,0x18},

/* FNUM BIT 9: 010 0000xxxx */
/* DEPTH 0 */ {0,   0,   0,   0,   0,   0,   0,   0},
/* DEPTH 1 */ {0,   0,   0,   0,   2,   2,   2,   2},
/* DEPTH 2 */ {0,   0,   0,   2,   2,   2,   4,   4},
/* DEPTH 3 */ {0,   0,   2,   2,   4,   4,   6,   6},
/* DEPTH 4 */ {0,   0,   2,   4,   4,   4,   6,   8},
/* DEPTH 5 */ {0,   0,   4,   6,   8,   8, 0xa, 0xc},
/* DEPTH 6 */ {0,   0,   8, 0xc,0x10,0x10,0x14,0x18},
/* DEPTH 7 */ {0,   0,0x10,0x18,0x20,0x20,0x28,0x30},

/* FNUM BIT10: 100 0000xxxx */
/* DEPTH 0 */ {0,   0,   0,   0,   0,   0,   0,   0},
/* DEPTH 1 */ {0,   0,   0,   0,   4,   4,   4,   4},
/* DEPTH 2 */ {0,   0,   0,   4,   4,   4,   8,   8},
/* DEPTH 3 */ {0,   0,   4,   4,   8,   8, 0xc, 0xc},
/* DEPTH 4 */ {0,   0,   4,   8,   8,   8, 0xc,0x10},
/* DEPTH 5 */ {0,   0,   8, 0xc,0x10,0x10,0x14,0x18},
/* DEPTH 6 */ {0,   0,0x10,0x18,0x20,0x20,0x28,0x30},
/* DEPTH 7 */ {0,   0,0x20,0x30,0x40,0x40,0x50,0x60},

};

// All 128 LFO PM waveforms: 128 combinations of the 7 meaningful F-NUMBER bits,
// 8 LFO depths, 32 LFO output levels per depth.
static std::array<s32, 128 * 8 * 32> makeLfoPmTable() {
    std::array<s32, 128 * 8 * 32> tab{};

    for (u32 depth = 0; depth < 8; depth++) {
        for (u32 fnum = 0; fnum < 128; fnum++) {
            for (u32 step = 0; step < 8; step++) {
                u8 value = 0;
                for (u32 bit = 0; bit < 7; bit++) {
                    if (fnum & (1u << bit)) {
                        value += lfoPmOutput[bit * 8 + depth][step];
                    }
                }
                tab[(fnum * 32 * 8) + (depth * 32) + step       +  0] =  value;
                tab[(fnum * 32 * 8) + (depth * 32) + (step ^ 7) +  8] =  value;
                tab[(fnum * 32 * 8) + (depth * 32) + step       + 16] = -value;
                tab[(fnum * 32 * 8) + (depth * 32) + (step ^ 7) + 24] = -value;
            }
        }
    }
    return tab;
}
static const std::array<s32, 128 * 8 * 32> lfoPmTable = makeLfoPmTable();


// Register number to channel number and slot offset.
static constexpr s32 opnChannel(s32 n) { return n & 3; }
static constexpr s32 opnSlot(s32 n)    { return (n >> 2) & 3; }

// Slot numbers. Note that slots 2 and 3 are swapped relative to the register
// layout, which is how the hardware orders them.
static constexpr s32 SLOT1 = 0;
static constexpr s32 SLOT2 = 2;
static constexpr s32 SLOT3 = 1;
static constexpr s32 SLOT4 = 3;

// bit0 = right enable, bit1 = left enable
static constexpr s32 OUTD_RIGHT  = 1;
static constexpr s32 OUTD_LEFT   = 2;
static constexpr s32 OUTD_CENTER = 3;

static inline void limit(s32& value, s32 max, s32 min) {
    if (value > max) {
        value = max;
    } else if (value < min) {
        value = min;
    }
}

// Raises status flags, and the interrupt line with them.
static inline void statusSet(FmState* st, s32 flag) {
    st->status |= flag;
    if (!st->irq && (st->status & st->irqMask)) {
        st->irq = 1;
        if (st->irqHandler) {
            st->irqHandler(true);
        }
    }
}

// Clears status flags, and the interrupt line with them.
static inline void statusReset(FmState* st, s32 flag) {
    st->status &= ~flag;
    if (st->irq && !(st->status & st->irqMask)) {
        st->irq = 0;
        if (st->irqHandler) {
            st->irqHandler(false);
        }
    }
}

static inline void irqMaskSet(FmState* st, s32 flag) {
    st->irqMask = flag;
    // Re-evaluate the interrupt line against the new mask.
    statusSet(st, 0);
    statusReset(st, 0);
}

// OPN mode register write.
static inline void setTimers(FmState* st, s32 v) {
    // b7 = CSM mode          b3 = timer enable b
    // b6 = 3 slot mode       b2 = timer enable a
    // b5 = reset b           b1 = load b
    // b4 = reset a           b0 = load a
    st->mode = v;

    if (v & 0x20) {
        statusReset(st, 0x02);      // reset timer b flag
    }
    if (v & 0x10) {
        statusReset(st, 0x01);      // reset timer a flag
    }
    if (v & 0x02) {                 // load b
        if (st->timerBCount == 0) {
            st->timerBCount = (256 - st->timerB) << 4;
            // The driver owns the timers.
            if (st->timerHandler) {
                st->timerHandler(1, st->timerBCount, st->timerBase);
            }
        }
    } else {                        // stop timer b
        if (st->timerBCount != 0) {
            st->timerBCount = 0;
            if (st->timerHandler) {
                st->timerHandler(1, 0, st->timerBase);
            }
        }
    }
    if (v & 0x01) {                 // load a
        if (st->timerACount == 0) {
            st->timerACount = 1024 - st->timerA;
            if (st->timerHandler) {
                st->timerHandler(0, st->timerACount, st->timerBase);
            }
        }
    } else {                        // stop timer a
        if (st->timerACount != 0) {
            st->timerACount = 0;
            if (st->timerHandler) {
                st->timerHandler(0, 0, st->timerBase);
            }
        }
    }
}


static inline void timerAOver(FmState* st) {
    if (st->mode & 0x04) {
        statusSet(st, 0x01);
    }
    st->timerACount = 1024 - st->timerA;
    if (st->timerHandler) {
        st->timerHandler(0, st->timerACount, st->timerBase);
    }
}

static inline void timerBOver(FmState* st) {
    if (st->mode & 0x08) {
        statusSet(st, 0x02);
    }
    st->timerBCount = (256 - st->timerB) << 4;
    if (st->timerHandler) {
        st->timerHandler(1, st->timerBCount, st->timerBase);
    }
}


static inline u8 statusFlag(FmState* st) {
    if (st->busyExpire) {
        if ((st->busyExpire - timeNow()) > 0) {
            return st->status | 0x80;   // busy
        }
        st->busyExpire = 0;
    }
    return st->status;
}


static inline void keyOn(u8 type, FmChannel* ch, s32 s) {
    FmSlot* slot = &ch->slot[s];
    if (slot->keyOn) {
        return;
    }

    slot->keyOn = 1;
    slot->phase = 0;                        // restart the phase generator
    slot->ssgNegate = (slot->ssgEg & 0x04) >> 1;

    if ((type == TYPE_YM2612) || (type == TYPE_YM2608)) {
        if ((slot->attackRate + slot->ksr) < 32 + 62) {
            slot->egState = EG_ATT;
        } else {
            // An instantaneous attack switches straight to decay.
            slot->volume = MIN_ATT_INDEX;
            slot->egState = EG_DEC;
        }
    } else {
        slot->egState = EG_ATT;
    }
}

static inline void keyOff(FmChannel* ch, s32 s) {
    FmSlot* slot = &ch->slot[s];
    if (slot->keyOn) {
        slot->keyOn = 0;
        if (slot->egState > EG_REL) {
            slot->egState = EG_REL;
        }
    }
}

// Wires up the operator outputs for the channel's current algorithm.
static void setupConnection(FmOpn* opn, FmChannel* ch, s32 channel) {
    s32* carrier = &opn->outFm[channel];

    s32** om1 = &ch->connect1;
    s32** om2 = &ch->connect3;
    s32** oc1 = &ch->connect2;

    s32** memc = &ch->memConnect;

    switch (ch->algorithm) {
    case 0:
        // M1---C1---MEM---M2---C2---OUT
        *om1  = &opn->c1;
        *oc1  = &opn->mem;
        *om2  = &opn->c2;
        *memc = &opn->m2;
        break;
    case 1:
        // M1------+-MEM---M2---C2---OUT
        //      C1-+
        *om1  = &opn->mem;
        *oc1  = &opn->mem;
        *om2  = &opn->c2;
        *memc = &opn->m2;
        break;
    case 2:
        // M1-----------------+-C2---OUT
        //      C1---MEM---M2-+
        *om1  = &opn->c2;
        *oc1  = &opn->mem;
        *om2  = &opn->c2;
        *memc = &opn->m2;
        break;
    case 3:
        // M1---C1---MEM------+-C2---OUT
        //                 M2-+
        *om1  = &opn->c1;
        *oc1  = &opn->mem;
        *om2  = &opn->c2;
        *memc = &opn->c2;
        break;
    case 4:
        // M1---C1-+-OUT
        // M2---C2-+
        // MEM is not used, so park it somewhere harmless.
        *om1  = &opn->c1;
        *oc1  = carrier;
        *om2  = &opn->c2;
        *memc = &opn->mem;
        break;
    case 5:
        //    +----C1----+
        // M1-+-MEM---M2-+-OUT
        //    +----C2----+
        *om1  = nullptr;            // special mark
        *oc1  = carrier;
        *om2  = carrier;
        *memc = &opn->m2;
        break;
    case 6:
        // M1---C1-+
        //      M2-+-OUT
        //      C2-+
        *om1  = &opn->c1;
        *oc1  = carrier;
        *om2  = carrier;
        *memc = &opn->mem;
        break;
    case 7:
        // M1-+
        // C1-+-OUT
        // M2-+
        // C2-+
        *om1  = carrier;
        *oc1  = carrier;
        *om2  = carrier;
        *memc = &opn->mem;
        break;
    }

    ch->connect4 = carrier;
}

static inline void setDetuneMultiple(FmState* st, FmChannel* ch, FmSlot* slot, s32 v) {
    slot->multiple = (v & 0x0f) ? (v & 0x0f) * 2 : 1;
    slot->detune = st->detuneTab[(v >> 4) & 7];
    ch->slot[SLOT1].phaseIncrement = -1;
}

static inline void setTotalLevel(FmSlot* slot, s32 v) {
    slot->totalLevel = (v & 0x7f) << (ENV_BITS - 7);    // 7 bit TL
}

static inline void setAttackRateKsr(u8 type, FmChannel* ch, FmSlot* slot, s32 v) {
    u8 oldKsrShift = slot->ksrShift;

    slot->attackRate = (v & 0x1f) ? 32 + ((v & 0x1f) << 1) : 0;

    slot->ksrShift = 3 - (v >> 6);
    if (slot->ksrShift != oldKsrShift) {
        ch->slot[SLOT1].phaseIncrement = -1;
    }

    if ((slot->attackRate + slot->ksr) < 32 + 62) {
        slot->egShiftAttack = egRateShift[slot->attackRate + slot->ksr];
        if ((type == TYPE_YM2612) || (type == TYPE_YM2608)) {
            slot->egSelectAttack = egRateSelect2612[slot->attackRate + slot->ksr];
        } else {
            slot->egSelectAttack = egRateSelect[slot->attackRate + slot->ksr];
        }
    } else {
        slot->egShiftAttack  = 0;
        slot->egSelectAttack = 17 * RATE_STEPS;
    }
}

static inline void setDecayRate(u8 type, FmSlot* slot, s32 v) {
    slot->decayRate = (v & 0x1f) ? 32 + ((v & 0x1f) << 1) : 0;

    slot->egShiftDecay = egRateShift[slot->decayRate + slot->ksr];
    if ((type == TYPE_YM2612) || (type == TYPE_YM2608)) {
        slot->egSelectDecay = egRateSelect2612[slot->decayRate + slot->ksr];
    } else {
        slot->egSelectDecay = egRateSelect[slot->decayRate + slot->ksr];
    }
}

static inline void setSustainRate(u8 type, FmSlot* slot, s32 v) {
    slot->sustainRate = (v & 0x1f) ? 32 + ((v & 0x1f) << 1) : 0;

    slot->egShiftSustain = egRateShift[slot->sustainRate + slot->ksr];
    if ((type == TYPE_YM2612) || (type == TYPE_YM2608)) {
        slot->egSelectSustain = egRateSelect2612[slot->sustainRate + slot->ksr];
    } else {
        slot->egSelectSustain = egRateSelect[slot->sustainRate + slot->ksr];
    }
}

static inline void setSustainLevelReleaseRate(u8 type, FmSlot* slot, s32 v) {
    slot->sustainLevel = slTable[v >> 4];

    slot->releaseRate = 34 + ((v & 0x0f) << 2);

    slot->egShiftRelease = egRateShift[slot->releaseRate + slot->ksr];
    if ((type == TYPE_YM2612) || (type == TYPE_YM2608)) {
        slot->egSelectRelease = egRateSelect2612[slot->releaseRate + slot->ksr];
    } else {
        slot->egSelectRelease = egRateSelect[slot->releaseRate + slot->ksr];
    }
}


static inline s32 opCalc(u32 phase, u32 env, s32 pm) {
    u32 p = (env << 3)
          + sinTab[(static_cast<s32>((phase & ~FREQ_MASK) + (pm << 15)) >> FREQ_SH) & SIN_MASK];

    if (p >= TL_TAB_LEN) {
        return 0;
    }
    return tlTab[p];
}

static inline s32 opCalc1(u32 phase, u32 env, s32 pm) {
    u32 p = (env << 3)
          + sinTab[(static_cast<s32>((phase & ~FREQ_MASK) + pm) >> FREQ_SH) & SIN_MASK];

    if (p >= TL_TAB_LEN) {
        return 0;
    }
    return tlTab[p];
}

// Advances the LFO to the next sample.
static inline void advanceLfo(FmOpn* opn) {
    if (!opn->lfoInc) {
        opn->lfoAm = 0;
        opn->lfoPm = 0;
        return;
    }

    opn->lfoCnt += opn->lfoInc;

    u8 pos = (opn->lfoCnt >> LFO_SH) & 127;

    // Triangle: AM runs 0 to 126 in steps of +2, then 126 back to 0 in -2.
    if (pos < 64) {
        opn->lfoAm = (pos & 63) * 2;
    } else {
        opn->lfoAm = 126 - ((pos & 63) * 2);
    }

    // PM runs off a clock four times slower.
    opn->lfoPm = pos >> 2;
}

static inline void advanceEgChannel(FmOpn* opn, FmSlot* slot) {
    u32 i = 4;      // four operators per channel
    do {
        u32 swapFlag = 0;

        switch (slot->egState) {
        case EG_ATT:
            if (!(opn->egCnt & ((1 << slot->egShiftAttack) - 1))) {
                slot->volume += (~slot->volume *
                                 egInc[slot->egSelectAttack +
                                       ((opn->egCnt >> slot->egShiftAttack) & 7)]) >> 4;

                if (slot->volume <= MIN_ATT_INDEX) {
                    slot->volume = MIN_ATT_INDEX;
                    slot->egState = EG_DEC;
                }
            }
            break;

        case EG_DEC:
            if ((opn->type == TYPE_YM2612) || (opn->type == TYPE_YM2608)) {
                if (!(opn->egCnt & ((1 << slot->egShiftDecay) - 1))) {
                    if (slot->ssgEg & 0x08) {   // SSG-EG type envelope
                        slot->volume += 6 * egInc[slot->egSelectDecay +
                                                  ((opn->egCnt >> slot->egShiftDecay) & 7)];
                    } else {
                        slot->volume += egInc[slot->egSelectDecay +
                                              ((opn->egCnt >> slot->egShiftDecay) & 7)];
                    }
                }

                // The transition is checked even when the volume did not update,
                // which handles SL == MIN_ATT_INDEX.
                if (slot->volume >= static_cast<s32>(slot->sustainLevel)) {
                    slot->volume = static_cast<s32>(slot->sustainLevel);
                    slot->egState = EG_SUS;
                }
            } else {
                if (slot->ssgEg & 0x08) {       // SSG-EG type envelope
                    if (!(opn->egCnt & ((1 << slot->egShiftDecay) - 1))) {
                        slot->volume += 4 * egInc[slot->egSelectDecay +
                                                  ((opn->egCnt >> slot->egShiftDecay) & 7)];

                        if (slot->volume >= static_cast<s32>(slot->sustainLevel)) {
                            slot->egState = EG_SUS;
                        }
                    }
                } else {
                    if (!(opn->egCnt & ((1 << slot->egShiftDecay) - 1))) {
                        slot->volume += egInc[slot->egSelectDecay +
                                              ((opn->egCnt >> slot->egShiftDecay) & 7)];

                        if (slot->volume >= static_cast<s32>(slot->sustainLevel)) {
                            slot->egState = EG_SUS;
                        }
                    }
                }
            }
            break;

        case EG_SUS:
            if (slot->ssgEg & 0x08) {           // SSG-EG type envelope
                if (!(opn->egCnt & ((1 << slot->egShiftSustain) - 1))) {
                    if ((opn->type == TYPE_YM2612) || (opn->type == TYPE_YM2608)) {
                        slot->volume += 6 * egInc[slot->egSelectSustain +
                                                  ((opn->egCnt >> slot->egShiftSustain) & 7)];
                    } else {
                        slot->volume += 4 * egInc[slot->egSelectSustain +
                                                  ((opn->egCnt >> slot->egShiftSustain) & 7)];
                    }

                    if (slot->volume >= ENV_QUIET) {
                        if ((opn->type != TYPE_YM2612) && (opn->type != TYPE_YM2608)) {
                            slot->volume = MAX_ATT_INDEX;
                        }

                        if (slot->ssgEg & 0x01) {           // bit 0 = hold
                            // If we already swapped once, just hold the level.
                            if (!(slot->ssgNegate & 1)) {
                                swapFlag = (slot->ssgEg & 0x02) | 1;    // bit 1 = alternate
                            }
                        } else {
                            // Same as a KEY ON: restart the phase generator.
                            slot->phase = 0;

                            if ((opn->type == TYPE_YM2612) || (opn->type == TYPE_YM2608)) {
                                if ((slot->attackRate + slot->ksr) < 94) {   // 32 + 62
                                    slot->egState = EG_ATT;
                                } else {
                                    // Maximal attack rate, so go straight to
                                    // decay (or sustain).
                                    slot->volume = MIN_ATT_INDEX;
                                    slot->egState =
                                        (slot->sustainLevel == MIN_ATT_INDEX) ? EG_SUS : EG_DEC;
                                }
                            } else {
                                slot->volume = 511;
                                slot->egState = EG_ATT;
                            }

                            swapFlag = (slot->ssgEg & 0x02);    // bit 1 = alternate
                        }
                    }
                }
            } else {
                if (!(opn->egCnt & ((1 << slot->egShiftSustain) - 1))) {
                    slot->volume += egInc[slot->egSelectSustain +
                                          ((opn->egCnt >> slot->egShiftSustain) & 7)];

                    if (slot->volume >= MAX_ATT_INDEX) {
                        slot->volume = MAX_ATT_INDEX;
                        // egState deliberately stays put (verified on the real chip).
                    }
                }
            }
            break;

        case EG_REL:
            if (!(opn->egCnt & ((1 << slot->egShiftRelease) - 1))) {
                // SSG-EG affects the release phase too (Nemesis).
                //
                // The assignments below are an upstream MAME typo that is part
                // of the emulated behaviour: they set type rather than compare
                // it, so the condition is always true. Fixing it changes the
                // output of every YM2610 game, so it stays.
                if ((slot->ssgEg & 0x08) &&
                    ((opn->type = TYPE_YM2612) || (opn->type = TYPE_YM2608))) {
                    slot->volume += 6 * egInc[slot->egSelectRelease +
                                              ((opn->egCnt >> slot->egShiftRelease) & 7)];
                } else {
                    slot->volume += egInc[slot->egSelectRelease +
                                          ((opn->egCnt >> slot->egShiftRelease) & 7)];
                }

                if (slot->volume >= MAX_ATT_INDEX) {
                    slot->volume = MAX_ATT_INDEX;
                    slot->egState = EG_OFF;
                }
            }
            break;
        }

        u32 out = static_cast<u32>(slot->volume);

        // Negate the output. Changes come from the alternate bit, the initial
        // state from the attack bit.
        if ((slot->ssgEg & 0x08) && (slot->ssgNegate & 2) && (slot->egState > EG_REL)) {
            out ^= MAX_ATT_INDEX;
        }

        // Store the result before ssgNegate changes below.
        slot->volumeOut = out + slot->totalLevel;

        slot->ssgNegate ^= swapFlag;

        slot++;
        i--;
    } while (i);
}


static inline u32 volumeCalc(const FmSlot* op, u32 am) {
    return op->volumeOut + (am & op->amMask);
}

static inline void updatePhaseLfoSlot(FmOpn* opn, FmSlot* slot, s32 pms, u32 blockFnum) {
    u32 fnumLfo = ((blockFnum & 0x7f0) >> 4) * 32 * 8;
    s32 lfoOffset = lfoPmTable[fnumLfo + pms + opn->lfoPm];

    if (!lfoOffset) {           // LFO phase modulation is zero
        slot->phase += slot->phaseIncrement;
        return;
    }

    blockFnum = blockFnum * 2 + lfoOffset;

    u8  blk = (blockFnum & 0x7000) >> 12;
    u32 fn  = blockFnum & 0xfff;

    s32 kc = (blk << 2) | opnFkTable[fn >> 8];      // key scale code

    s32 fc = (opn->fnTable[fn] >> (7 - blk)) + slot->detune[kc];

    // Detect frequency overflow (credits to Nemesis).
    if (fc < 0) {
        fc += opn->fnMax;
    }

    slot->phase += (fc * slot->multiple) >> 1;
}

static inline void updatePhaseLfoChannel(FmOpn* opn, FmChannel* ch) {
    u32 blockFnum = ch->blockFnum;

    u32 fnumLfo = ((blockFnum & 0x7f0) >> 4) * 32 * 8;
    s32 lfoOffset = lfoPmTable[fnumLfo + ch->pms + opn->lfoPm];

    if (lfoOffset) {            // LFO phase modulation active
        blockFnum = blockFnum * 2 + lfoOffset;

        u8  blk = (blockFnum & 0x7000) >> 12;
        u32 fn  = blockFnum & 0xfff;

        s32 kc = (blk << 2) | opnFkTable[fn >> 8];  // key scale code

        s32 fc = opn->fnTable[fn] >> (7 - blk);

        // Detect frequency overflow (credits to Nemesis).
        s32 finc = fc + ch->slot[SLOT1].detune[kc];
        if (finc < 0) {
            finc += opn->fnMax;
        }
        ch->slot[SLOT1].phase += (finc * ch->slot[SLOT1].multiple) >> 1;

        finc = fc + ch->slot[SLOT2].detune[kc];
        if (finc < 0) {
            finc += opn->fnMax;
        }
        ch->slot[SLOT2].phase += (finc * ch->slot[SLOT2].multiple) >> 1;

        finc = fc + ch->slot[SLOT3].detune[kc];
        if (finc < 0) {
            finc += opn->fnMax;
        }
        ch->slot[SLOT3].phase += (finc * ch->slot[SLOT3].multiple) >> 1;

        finc = fc + ch->slot[SLOT4].detune[kc];
        if (finc < 0) {
            finc += opn->fnMax;
        }
        ch->slot[SLOT4].phase += (finc * ch->slot[SLOT4].multiple) >> 1;
    } else {                    // LFO phase modulation is zero
        ch->slot[SLOT1].phase += ch->slot[SLOT1].phaseIncrement;
        ch->slot[SLOT2].phase += ch->slot[SLOT2].phaseIncrement;
        ch->slot[SLOT3].phase += ch->slot[SLOT3].phaseIncrement;
        ch->slot[SLOT4].phase += ch->slot[SLOT4].phaseIncrement;
    }
}

static inline void chanCalc(FmOpn* opn, FmChannel* ch, s32 chnum) {
    u32 am = opn->lfoAm >> ch->ams;

    opn->m2 = opn->c1 = opn->c2 = opn->mem = 0;

    // Restore the delayed sample (MEM) into opn->m2 or opn->c2.
    *ch->memConnect = ch->memValue;

    u32 egOut = volumeCalc(&ch->slot[SLOT1], am);
    {
        s32 out = ch->op1Out[0] + ch->op1Out[1];
        ch->op1Out[0] = ch->op1Out[1];

        if (!ch->connect1) {
            // Algorithm 5
            opn->mem = opn->c1 = opn->c2 = ch->op1Out[0];
        } else {
            *ch->connect1 += ch->op1Out[0];
        }

        ch->op1Out[1] = 0;
        if (egOut < ENV_QUIET) {        // slot 1
            if (!ch->feedback) {
                out = 0;
            }
            ch->op1Out[1] = opCalc1(ch->slot[SLOT1].phase, egOut, out << ch->feedback);
        }
    }

    egOut = volumeCalc(&ch->slot[SLOT3], am);
    if (egOut < ENV_QUIET) {            // slot 3
        *ch->connect3 += opCalc(ch->slot[SLOT3].phase, egOut, opn->m2);
    }

    egOut = volumeCalc(&ch->slot[SLOT2], am);
    if (egOut < ENV_QUIET) {            // slot 2
        *ch->connect2 += opCalc(ch->slot[SLOT2].phase, egOut, opn->c1);
    }

    egOut = volumeCalc(&ch->slot[SLOT4], am);
    if (egOut < ENV_QUIET) {            // slot 4
        *ch->connect4 += opCalc(ch->slot[SLOT4].phase, egOut, opn->c2);
    }

    ch->memValue = opn->mem;            // store the current MEM

    // Update the phase counters after the output calculations.
    if (ch->pms) {
        if ((opn->st.mode & 0xC0) && (chnum == 2)) {    // 3 slot mode
            updatePhaseLfoSlot(opn, &ch->slot[SLOT1], ch->pms, opn->slot3.blockFnum[1]);
            updatePhaseLfoSlot(opn, &ch->slot[SLOT2], ch->pms, opn->slot3.blockFnum[2]);
            updatePhaseLfoSlot(opn, &ch->slot[SLOT3], ch->pms, opn->slot3.blockFnum[0]);
            updatePhaseLfoSlot(opn, &ch->slot[SLOT4], ch->pms, ch->blockFnum);
        } else {
            updatePhaseLfoChannel(opn, ch);
        }
    } else {                            // no LFO phase modulation
        ch->slot[SLOT1].phase += ch->slot[SLOT1].phaseIncrement;
        ch->slot[SLOT2].phase += ch->slot[SLOT2].phaseIncrement;
        ch->slot[SLOT3].phase += ch->slot[SLOT3].phaseIncrement;
        ch->slot[SLOT4].phase += ch->slot[SLOT4].phaseIncrement;
    }
}

// Updates the phase increment and the envelope generator rates.
static inline void refreshFcEgSlot(FmOpn* opn, FmSlot* slot, s32 fc, s32 kc) {
    s32 ksr = kc >> slot->ksrShift;

    fc += slot->detune[kc];

    // Detect frequency overflow (credits to Nemesis).
    if (fc < 0) {
        fc += opn->fnMax;
    }

    slot->phaseIncrement = (fc * slot->multiple) >> 1;

    if (slot->ksr == ksr) {
        return;
    }
    slot->ksr = ksr;

    if ((slot->attackRate + slot->ksr) < 32 + 62) {
        slot->egShiftAttack = egRateShift[slot->attackRate + slot->ksr];
        if ((opn->type == TYPE_YM2612) || (opn->type == TYPE_YM2608)) {
            slot->egSelectAttack = egRateSelect2612[slot->attackRate + slot->ksr];
        } else {
            slot->egSelectAttack = egRateSelect[slot->attackRate + slot->ksr];
        }
    } else {
        slot->egShiftAttack  = 0;
        slot->egSelectAttack = 17 * RATE_STEPS;
    }

    slot->egShiftDecay   = egRateShift[slot->decayRate   + slot->ksr];
    slot->egShiftSustain = egRateShift[slot->sustainRate + slot->ksr];
    slot->egShiftRelease = egRateShift[slot->releaseRate + slot->ksr];

    if ((opn->type == TYPE_YM2612) || (opn->type == TYPE_YM2608)) {
        slot->egSelectDecay   = egRateSelect2612[slot->decayRate   + slot->ksr];
        slot->egSelectSustain = egRateSelect2612[slot->sustainRate + slot->ksr];
        slot->egSelectRelease = egRateSelect2612[slot->releaseRate + slot->ksr];
    } else {
        slot->egSelectDecay   = egRateSelect[slot->decayRate   + slot->ksr];
        slot->egSelectSustain = egRateSelect[slot->sustainRate + slot->ksr];
        slot->egSelectRelease = egRateSelect[slot->releaseRate + slot->ksr];
    }
}

// Not inline: this works around a gcc 4.2.1 codegen bug.
static void refreshFcEgChan(FmOpn* opn, FmChannel* ch) {
    if (ch->slot[SLOT1].phaseIncrement == -1) {
        s32 fc = ch->fc;
        s32 kc = ch->kcode;
        refreshFcEgSlot(opn, &ch->slot[SLOT1], fc, kc);
        refreshFcEgSlot(opn, &ch->slot[SLOT2], fc, kc);
        refreshFcEgSlot(opn, &ch->slot[SLOT3], fc, kc);
        refreshFcEgSlot(opn, &ch->slot[SLOT4], fc, kc);
    }
}

// Builds the detune table for the current frequency base.
static void initTimeTables(FmState* st, const u8* detuneTable) {
    for (s32 d = 0; d <= 3; d++) {
        for (s32 i = 0; i <= 31; i++) {
            double rate = static_cast<double>(detuneTable[d * 32 + i]) * SIN_LEN *
                          st->freqBase * (1 << FREQ_SH) / static_cast<double>(1 << 20);
            st->detuneTab[d][i]     = static_cast<s32>(rate);
            st->detuneTab[d + 4][i] = -st->detuneTab[d][i];
        }
    }
}


static void resetChannels(FmState* st, FmChannel* ch, s32 num) {
    st->mode        = 0;    // normal mode
    st->timerA      = 0;
    st->timerACount = 0;
    st->timerB      = 0;
    st->timerBCount = 0;

    for (s32 c = 0; c < num; c++) {
        ch[c].fc = 0;
        for (s32 s = 0; s < 4; s++) {
            ch[c].slot[s].ssgEg      = 0;
            ch[c].slot[s].ssgNegate  = 0;
            ch[c].slot[s].egState    = EG_OFF;
            ch[c].slot[s].volume     = MAX_ATT_INDEX;
            ch[c].slot[s].volumeOut  = MAX_ATT_INDEX;
        }
    }
}

// CSM key control: key every operator on and straight back off again, but only
// those that were off to begin with.
static inline void csmKeyControl(u8 type, FmChannel* ch) {
    for (s32 s : {SLOT1, SLOT2, SLOT3, SLOT4}) {
        if (!ch->slot[s].keyOn) {
            keyOn(type, ch, s);
            keyOff(ch, s);
        }
    }
}

// FM channel state, internal state only.
// A single walk drives both saving and loading, so the two directions cannot
// drift apart.
template <typename Visit>
static void visitChannelState(Visit visit, FmChannel* ch, s32 numChannels) {
    for (s32 c = 0; c < numChannels; c++, ch++) {
        visit(ch->op1Out);
        visit(ch->fc);

        for (s32 s = 0; s < 4; s++) {
            FmSlot* slot = &ch->slot[s];

            visit(slot->phase);
            visit(slot->egState);
            visit(slot->volume);

            // Every dynamic register of the channel has to be scanned.
            // - dink (July 20, 2020)
            visit(slot->volumeOut);
            visit(slot->egShiftAttack);
            visit(slot->egSelectAttack);
            visit(slot->egShiftDecay);
            visit(slot->egSelectDecay);
            visit(slot->egShiftSustain);
            visit(slot->egSelectSustain);
            visit(slot->egShiftRelease);
            visit(slot->egSelectRelease);
            visit(slot->ssgEg);         // also set in postload
            visit(slot->ssgNegate);
            visit(slot->keyOn);
        }
    }
}

template <typename Visit>
static void visitStState(Visit visit, FmState* st) {
    visit(st->busyExpire);
    visit(st->clock);
    visit(st->rate);
    visit(st->freqBase);
    visit(st->timerBase);
    visit(st->address);
    visit(st->irq);
    visit(st->irqMask);
    visit(st->status);
    visit(st->mode);
    visit(st->prescalerSel);
    visit(st->fnH);
    visit(st->timerA);
    visit(st->timerACount);
    visit(st->timerB);
    visit(st->timerBCount);
}


// Sets the prescaler and rebuilds the derived time tables.
static void opnSetPrescaler(FmOpn* opn, s32 pres, s32 timerPres) {
    opn->st.freqBase = opn->st.rate
        ? (static_cast<double>(opn->st.clock) / opn->st.rate) / pres
        : 0;

    opn->egTimerAdd      = (1 << EG_SH) * opn->st.freqBase;
    opn->egTimerOverflow = 3 * (1 << EG_SH);

    opn->st.timerBase = 1.0 / (static_cast<double>(opn->st.clock) /
                               static_cast<double>(timerPres));

    initTimeTables(&opn->st, dtTab);

    // FNUM/BLK can generate 2048 FNUMs, but the LFO works with one more bit of
    // precision, so this table needs 4096 entries.
    for (s32 i = 0; i < 4096; i++) {
        // Frequency table for octave 7. The OPN phase increment counter is 20
        // bits. The -10 is because the chip works in 10.10 fixed point while we
        // use 16.16.
        opn->fnTable[i] = static_cast<u32>(
            static_cast<double>(i) * 32 * opn->st.freqBase * (1 << (FREQ_SH - 10)));
    }

    // The maximum frequency is needed for the phase overflow calculation. The
    // register is 17 bits (Nemesis).
    opn->fnMax = static_cast<u32>(
        static_cast<double>(0x20000) * opn->st.freqBase * (1 << (FREQ_SH - 10)));

    // LFO frequency table. Amplitude modulation has 64 output levels forming a
    // triangle, each lasting lfoSamplesPerStep samples; one lfoPmOutput entry
    // lasts four times as long.
    for (s32 i = 0; i < 8; i++) {
        opn->lfoFreq[i] = (1.0 / lfoSamplesPerStep[i]) * (1 << LFO_SH) * opn->st.freqBase;
    }
}


// Writes an OPN mode register, 0x20-0x2f.
static void opnWriteMode(FmOpn* opn, s32 r, s32 v) {
    switch (r) {
    case 0x21:      // test
        break;

    case 0x22:      // LFO freq (YM2608 / YM2610 / YM2610B / YM2612)
        if (opn->type & TYPE_LFOPAN) {
            opn->lfoInc = (v & 0x08) ? opn->lfoFreq[v & 7] : 0;
        }
        break;

    case 0x24:      // timer A, high 8 bits
        opn->st.timerA = (opn->st.timerA & 0x03) | (static_cast<s32>(v) << 2);
        break;

    case 0x25:      // timer A, low 2 bits
        opn->st.timerA = (opn->st.timerA & 0x3fc) | (v & 3);
        break;

    case 0x26:      // timer B
        opn->st.timerB = v;
        break;

    case 0x27:      // mode, timer control
        setTimers(&opn->st, v);
        break;

    case 0x28: {    // key on / off
        u8 c = v & 0x03;
        if (c == 3) {
            break;
        }
        if ((v & 0x04) && (opn->type & TYPE_6CH)) {
            c += 3;
        }
        FmChannel* ch = &opn->channels[c];
        if (v & 0x10) { keyOn(opn->type, ch, SLOT1); } else { keyOff(ch, SLOT1); }
        if (v & 0x20) { keyOn(opn->type, ch, SLOT2); } else { keyOff(ch, SLOT2); }
        if (v & 0x40) { keyOn(opn->type, ch, SLOT3); } else { keyOff(ch, SLOT3); }
        if (v & 0x80) { keyOn(opn->type, ch, SLOT4); } else { keyOff(ch, SLOT4); }
        break;
    }
    }
}

// Writes an OPN register, 0x30-0xff.
static void opnWriteReg(FmOpn* opn, s32 r, s32 v) {
    u8 c = opnChannel(r);

    if (c == 3) {
        return;         // 0xX3, 0xX7, 0xXB, 0xXF
    }

    if (r >= 0x100) {
        c += 3;
    }

    FmChannel* ch = &opn->channels[c];

    FmSlot* slot = &ch->slot[opnSlot(r)];

    switch (r & 0xf0) {
    case 0x30:      // DET, MUL
        setDetuneMultiple(&opn->st, ch, slot, v);
        break;

    case 0x40:      // TL
        setTotalLevel(slot, v);
        break;

    case 0x50:      // KS, AR
        setAttackRateKsr(opn->type, ch, slot, v);
        break;

    case 0x60:      // bit 7 = AM enable, DR
        setDecayRate(opn->type, slot, v);

        if (opn->type & TYPE_LFOPAN) {      // YM2608 / 2610 / 2610B / 2612
            slot->amMask = (v & 0x80) ? ~0u : 0u;
        }
        break;

    case 0x70:      // SR
        setSustainRate(opn->type, slot, v);
        break;

    case 0x80:      // SL, RR
        setSustainLevelReleaseRate(opn->type, slot, v);
        break;

    case 0x90:      // SSG-EG
        slot->ssgEg     = v & 0x0f;
        slot->ssgNegate = (v & 0x04) >> 1;      // bit 1 of ssgNegate = attack

        // SSG-EG envelope shapes, where E is the SSG-EG enable bit:
        //
        //   E AtAlH
        //   1 0 0 0  \\\\
        //   1 0 0 1  \___
        //   1 0 1 0  \/\/
        //   1 0 1 1  \---
        //   1 1 0 0  ////
        //   1 1 0 1  /---
        //   1 1 1 0  /\/\
        //   1 1 1 1  /___
        //
        // The shapes are generated from the attack, decay and sustain phases.
        // Each character above stands for this whole sequence:
        //
        //  - on KEY ON a normal attack phase runs, with no difference from
        //    normal mode;
        //  - once the envelope level reaches minimum level (maximum volume) the
        //    EG switches to decay, which uses bigger steps than normal mode;
        //  - once the envelope level passes SL the EG switches to sustain,
        //    which also uses bigger steps;
        //  - once the envelope level reaches maximum level (minimum volume) the
        //    EG switches back to attack, depending on the waveform.
        //
        // When the switch back to attack happens the operator's phase counter
        // is usually zeroed out as in a normal KEY ON, but not always; the rule
        // for that has never been established. Perhaps only when the output
        // level is low.
        //
        // The resolution in the decay and sustain phases is 4 times lower than
        // in normal mode, so 256 steps instead of 1024. The times between level
        // changes are the same in both modes.
        //
        // Decay 1 Level (SL) is compared against the actual SSG-EG output, so
        // it behaves the same in both modes, with one exception: when SSG-EG is
        // enabled and generating rising levels the EG output is inverted, and
        // SL then lands at the wrong level. With SL=02 for example, 0 - 6 is
        // -6dB in the non-inverted output but 96 - 6 is -90dB in the inverted
        // one. The EG compares its level to SL as usual and the output is
        // simply inverted afterwards.
        //
        // Yamaha's manuals say AR should be set to 0x1f (maximum speed). That
        // is not necessary, but otherwise the EG will generate an attack phase.
        break;

    case 0xa0:
        switch (opnSlot(r)) {
        case 0: {       // 0xa0-0xa2 : FNUM1
            u32 fn  = (static_cast<u32>(opn->st.fnH & 7) << 8) + v;
            u8  blk = opn->st.fnH >> 3;

            ch->kcode = (blk << 2) | opnFkTable[fn >> 7];       // key scale code
            ch->fc = opn->fnTable[fn * 2] >> (7 - blk);         // phase increment

            // Store fnum in clear form for the LFO PM calculations.
            ch->blockFnum = (blk << 11) | fn;

            ch->slot[SLOT1].phaseIncrement = -1;
            break;
        }
        case 1:         // 0xa4-0xa6 : FNUM2, BLK
            opn->st.fnH = v & 0x3f;
            break;

        case 2: {       // 0xa8-0xaa : 3CH FNUM1
            if (r < 0x100) {
                u32 fn  = (static_cast<u32>(opn->slot3.fnH & 7) << 8) + v;
                u8  blk = opn->slot3.fnH >> 3;

                opn->slot3.kcode[c] = (blk << 2) | opnFkTable[fn >> 7];
                opn->slot3.fc[c] = opn->fnTable[fn * 2] >> (7 - blk);
                opn->slot3.blockFnum[c] = (blk << 11) | fn;
                opn->channels[2].slot[SLOT1].phaseIncrement = -1;
            }
            break;
        }
        case 3:         // 0xac-0xae : 3CH FNUM2, BLK
            if (r < 0x100) {
                opn->slot3.fnH = v & 0x3f;
            }
            break;
        }
        break;

    case 0xb0:
        switch (opnSlot(r)) {
        case 0: {       // 0xb0-0xb2 : FB, ALGO
            s32 feedback = (v >> 3) & 7;
            ch->algorithm = v & 7;
            ch->feedback  = feedback ? feedback + 6 : 0;
            setupConnection(opn, ch, c);
            break;
        }
        case 1:         // 0xb4-0xb6 : L, R, AMS, PMS (YM2612 / 2610B / 2610 / 2608)
            if (opn->type & TYPE_LFOPAN) {
                // b0-2: PM depth * 32, which indexes lfoPmTable
                ch->pms = (v & 7) * 32;

                // b4-5: AMS
                ch->ams = lfoAmsDepthShift[(v >> 4) & 0x03];

                // Pan: b7 = left, b6 = right
                opn->pan[c * 2    ] = (v & 0x80) ? ~0u : 0u;
                opn->pan[c * 2 + 1] = (v & 0x40) ? ~0u : 0u;
            }
            break;
        }
        break;
    }
}


// YM2610 ADPCM
static constexpr u32 ADPCM_SHIFT          = 16;     // frequency step rate
static constexpr u32 ADPCMA_ADDRESS_SHIFT = 8;

// Algorithm and tables verified on real YM2608 and YM2610 hardware.

// The usual ADPCM table, 16 * 1.1^N.
static constexpr s32 steps[49] = {
     16,  17,   19,   21,   23,   25,   28,
     31,  34,   37,   41,   45,   50,   55,
     60,  66,   73,   80,   88,   97,  107,
    118, 130,  143,  157,  173,  190,  209,
    230, 253,  279,  307,  337,  371,  408,
    449, 494,  544,  598,  658,  724,  796,
    876, 963, 1060, 1166, 1282, 1411, 1552,
};

// Different from the usual ADPCM table.
static constexpr s32 stepInc[8] = { -1*16, -1*16, -1*16, -1*16, 2*16, 5*16, 7*16, 9*16 };

// Decoding lookup table, for speed only.
// Not constexpr for the same reason as makeLfoPmTable: folding it would move
// 3 KB out of BSS and into the binary.
static std::array<s32, 49 * 16> makeJediTable() {
    std::array<s32, 49 * 16> tab{};

    // Loop over every step and nibble and compute the difference.
    for (s32 step = 0; step < 49; step++) {
        for (s32 nib = 0; nib < 16; nib++) {
            s32 value = (2 * (nib & 0x07) + 1) * steps[step] / 8;
            tab[step * 16 + nib] = (nib & 0x08) ? -value : value;
        }
    }
    return tab;
}
static const std::array<s32, 49 * 16> jediTable = makeJediTable();

// ADPCM-A, non control type: calculates one channel's output.
void Ym2610::adpcmaCalcChannel(AdpcmChannel* ch) {
    ch->nowStep += ch->step;
    if (ch->nowStep >= (1 << ADPCM_SHIFT)) {
        u32 step = ch->nowStep >> ADPCM_SHIFT;
        ch->nowStep &= (1 << ADPCM_SHIFT) - 1;

        do {
            // The YM2610 checks the low 20 bits only; the 4 MSBs are the sample
            // bank. 1<<21 here compensates for the nibble addressing.
            // - 11-06-2001 JB: corrected comparison, was > instead of ==
            if ((ch->nowAddr & ((1 << 21) - 1)) == ((ch->end << 1) & ((1 << 21) - 1))) {
                ch->flag = 0;
                m_adpcmArrivedEndAddress |= ch->flagMask;
                return;
            }

            u8 data;
            if (ch->nowAddr & 1) {
                data = ch->nowData & 0x0f;
            } else {
                ch->nowData = *(m_pcmBuf + (ch->nowAddr >> 1));
                data = (ch->nowData >> 4) & 0x0f;
            }

            ch->nowAddr++;

            ch->adpcmAcc += jediTable[ch->adpcmStep + data];

            // Sign-extend the 12-bit accumulator.
            if (ch->adpcmAcc & ~0x7ff) {
                ch->adpcmAcc |= ~0xfff;
            } else {
                ch->adpcmAcc &= 0xfff;
            }

            ch->adpcmStep += stepInc[data & 7];
            limit(ch->adpcmStep, 48 * 16, 0 * 16);
        } while (--step);

        // Multiply by the volume, shift, and mask out the 2 LSBs.
        ch->adpcmOut = ((ch->adpcmAcc * ch->volMul) >> ch->volShift) & ~3;
    }

    *(ch->pan) += ch->adpcmOut;
}

// Recomputes the volume multiplier and shift for one ADPCM-A channel.
static void adpcmaUpdateVolume(AdpcmChannel* ch, u8 totalLevel) {
    s32 volume = totalLevel + ch->instrumentLevel;

    if (volume >= 63) {         // 63 is silence, which is correct
        ch->volMul   = 0;
        ch->volShift = 0;
    } else {
        ch->volMul   = 15 - (volume & 7);   // so-called 0.75 dB
        // Yamaha's engineers approximated each -6 dB step as a halving.
        ch->volShift = 1 + (volume >> 3);
    }

    // Multiply by the volume, shift, and mask out the 2 LSBs.
    ch->adpcmOut = ((ch->adpcmAcc * ch->volMul) >> ch->volShift) & ~3;
}

// ADPCM type A write.
void Ym2610::adpcmaWrite(s32 r, s32 v) {
    AdpcmChannel* adpcm = m_adpcm;
    u8 c = r & 0x07;

    m_adpcmReg[r] = v & 0xff;

    switch (r) {
    case 0x00:      // DM,--,C5,C4,C3,C2,C1,C0
        if (!(v & 0x80)) {
            for (c = 0; c < 6; c++) {           // key on
                if (!((v >> c) & 1)) {
                    continue;
                }
                adpcm[c].step = static_cast<u32>(
                    static_cast<float>(1 << ADPCM_SHIFT) *
                    static_cast<float>(m_opn.st.freqBase) / 3.0f);
                adpcm[c].nowAddr   = adpcm[c].start << 1;
                adpcm[c].nowStep   = 0;
                adpcm[c].adpcmAcc  = 0;
                adpcm[c].adpcmStep = 0;
                adpcm[c].adpcmOut  = 0;
                adpcm[c].flag      = 1;

                if (m_pcmBuf == nullptr) {      // is the ROM mapped?
                    adpcm[c].flag = 0;
                } else if (adpcm[c].start >= m_pcmSize) {
                    // JB: do NOT clamp adpcm[c].end to m_pcmSize here, that
                    // would break the comparison in adpcmaCalcChannel().
                    adpcm[c].flag = 0;
                }
            }
        } else {
            for (c = 0; c < 6; c++) {           // key off
                if ((v >> c) & 1) {
                    adpcm[c].flag = 0;
                }
            }
        }
        break;

    case 0x01:      // b0-5 = TL
        m_adpcmTL = (v & 0x3f) ^ 0x3f;
        for (c = 0; c < 6; c++) {
            adpcmaUpdateVolume(&adpcm[c], m_adpcmTL);
        }
        break;

    default:
        c = r & 0x07;
        if (c >= 0x06) {
            return;
        }
        switch (r & 0x38) {
        case 0x08:      // b7 = L, b6 = R, b4-0 = IL
            adpcm[c].instrumentLevel = (v & 0x1f) ^ 0x1f;
            adpcm[c].pan    = &m_outAdpcm[(v >> 6) & 0x03];
            adpcm[c].panRaw = (v >> 6) & 0x03;
            adpcmaUpdateVolume(&adpcm[c], m_adpcmTL);
            break;

        case 0x10:
        case 0x18:
            adpcm[c].start = (m_adpcmReg[0x18 + c] * 0x0100 | m_adpcmReg[0x10 + c])
                             << ADPCMA_ADDRESS_SHIFT;
            // KOF98AE sample banking support
            if (m_pcmSize > 0x1000000 && m_adpcmReg[0x08 + c] >= 0xf0) {
                adpcm[c].start += 0x1000000;
            }
            break;

        case 0x20:
        case 0x28:
            adpcm[c].end = (m_adpcmReg[0x28 + c] * 0x0100 | m_adpcmReg[0x20 + c])
                           << ADPCMA_ADDRESS_SHIFT;
            adpcm[c].end += (1 << ADPCMA_ADDRESS_SHIFT) - 1;
            // KOF98AE sample banking support
            if (m_pcmSize > 0x1000000 && m_adpcmReg[0x08 + c] >= 0xf0) {
                adpcm[c].end += 0x1000000;
            }
            break;
        }
    }
}

// ADPCM-A channel state, internal state only.
template <typename Visit>
static void visitAdpcmaState(Visit visit, AdpcmChannel* adpcm) {
    for (s32 ch = 0; ch < 6; ch++, adpcm++) {
        visit(adpcm->flag);
        visit(adpcm->flagMask);
        visit(adpcm->nowData);
        visit(adpcm->nowAddr);
        visit(adpcm->nowStep);
        visit(adpcm->start);
        visit(adpcm->end);
        visit(adpcm->instrumentLevel);
        visit(adpcm->adpcmAcc);
        visit(adpcm->adpcmStep);
        visit(adpcm->adpcmOut);
        visit(adpcm->volMul);
        visit(adpcm->volShift);
        visit(adpcm->panRaw);
    }
}


// Generates samples for the YM2610.
void Ym2610::update(s16* bufL, s16* bufR, s32 length) {
    FmOpn* opn = &m_opn;
    FmState* state = &opn->st;

    // The YM2610 only has four FM channels: 1, 2, 4 and 5 in OPN numbering.
    FmChannel* cch[4] = { &m_ch[1], &m_ch[2], &m_ch[4], &m_ch[5] };

    // Refresh the phase and envelope generators.
    refreshFcEgChan(opn, cch[0]);
    if (state->mode & 0xc0) {           // 3 slot mode
        if (cch[1]->slot[SLOT1].phaseIncrement == -1) {
            refreshFcEgSlot(opn, &cch[1]->slot[SLOT1], opn->slot3.fc[1], opn->slot3.kcode[1]);
            refreshFcEgSlot(opn, &cch[1]->slot[SLOT2], opn->slot3.fc[2], opn->slot3.kcode[2]);
            refreshFcEgSlot(opn, &cch[1]->slot[SLOT3], opn->slot3.fc[0], opn->slot3.kcode[0]);
            refreshFcEgSlot(opn, &cch[1]->slot[SLOT4], cch[1]->fc, cch[1]->kcode);
        }
    } else {
        refreshFcEgChan(opn, cch[1]);
    }
    refreshFcEgChan(opn, cch[2]);
    refreshFcEgChan(opn, cch[3]);

    for (s32 i = 0; i < length; i++) {
        advanceLfo(opn);

        // Clear the output accumulators.
        m_outAdpcm[OUTD_LEFT] = m_outAdpcm[OUTD_RIGHT] = m_outAdpcm[OUTD_CENTER] = 0;
        m_outDelta[OUTD_LEFT] = m_outDelta[OUTD_RIGHT] = m_outDelta[OUTD_CENTER] = 0;
        opn->outFm[1] = 0;
        opn->outFm[2] = 0;
        opn->outFm[4] = 0;
        opn->outFm[5] = 0;

        // Advance the envelope generator.
        opn->egTimer += opn->egTimerAdd;
        while (opn->egTimer >= opn->egTimerOverflow) {
            opn->egTimer -= opn->egTimerOverflow;
            opn->egCnt++;

            advanceEgChannel(opn, &cch[0]->slot[SLOT1]);
            advanceEgChannel(opn, &cch[1]->slot[SLOT1]);
            advanceEgChannel(opn, &cch[2]->slot[SLOT1]);
            advanceEgChannel(opn, &cch[3]->slot[SLOT1]);
        }

        // Calculate FM. The channel numbers are remapped to 1, 2, 4 and 5.
        chanCalc(opn, cch[0], 1);
        chanCalc(opn, cch[1], 2);
        chanCalc(opn, cch[2], 4);
        chanCalc(opn, cch[3], 5);

        // Delta-T ADPCM
        if (m_deltaT.portState() & 0x80) {
            m_deltaT.calc();
        }

        // ADPCM-A
        for (s32 j = 0; j < 6; j++) {
            if (m_adpcm[j].flag) {
                adpcmaCalcChannel(&m_adpcm[j]);
            }
        }

        s32 lt = m_outAdpcm[OUTD_LEFT]  + m_outAdpcm[OUTD_CENTER];
        s32 rt = m_outAdpcm[OUTD_RIGHT] + m_outAdpcm[OUTD_CENTER];
        lt += (m_outDelta[OUTD_LEFT]  + m_outDelta[OUTD_CENTER]) >> 9;
        rt += (m_outDelta[OUTD_RIGHT] + m_outDelta[OUTD_CENTER]) >> 9;

        // The shift right on each FM channel was verified on the real chip.
        lt += ((opn->outFm[1] >> 1) & opn->pan[2]);
        rt += ((opn->outFm[1] >> 1) & opn->pan[3]);
        lt += ((opn->outFm[2] >> 1) & opn->pan[4]);
        rt += ((opn->outFm[2] >> 1) & opn->pan[5]);

        lt += ((opn->outFm[4] >> 1) & opn->pan[8]);
        rt += ((opn->outFm[4] >> 1) & opn->pan[9]);
        lt += ((opn->outFm[5] >> 1) & opn->pan[10]);
        rt += ((opn->outFm[5] >> 1) & opn->pan[11]);

        limit(lt, MAXOUT, MINOUT);
        limit(rt, MAXOUT, MINOUT);

        bufL[i] = lt;
        bufR[i] = rt;
    }
}


void Ym2610::postload() {
    // Replaying the registers below must not raise status flags that the saved
    // state has already accounted for.
    m_deltaT.setPostloading(true);

    // SSG registers
    for (s32 r = 0; r < 16; r++) {
        m_ssg.write(0, r);
        m_ssg.write(1, m_regs[r]);
    }

    // OPN registers: DT/MULTI, TL, KS/AR, AMON/DR, SR, SL/RR, SSG-EG
    for (s32 r = 0x30; r < 0x9e; r++) {
        if ((r & 3) != 3) {
            opnWriteReg(&m_opn, r, m_regs[r]);
            opnWriteReg(&m_opn, r | 0x100, m_regs[r | 0x100]);
        }
    }

    // FB/CONNECT, L/R/AMS/PMS
    for (s32 r = 0xb0; r < 0xb6; r++) {
        if ((r & 3) != 3) {
            opnWriteReg(&m_opn, r, m_regs[r]);
            opnWriteReg(&m_opn, r | 0x100, m_regs[r | 0x100]);
        }
    }

    // Rhythm (ADPCM-A)
    adpcmaWrite(1, m_regs[0x101]);
    for (s32 r = 0; r < 6; r++) {
        adpcmaWrite(r + 0x08, m_regs[r + 0x108]);
        adpcmaWrite(r + 0x10, m_regs[r + 0x110]);
        adpcmaWrite(r + 0x18, m_regs[r + 0x118]);
        adpcmaWrite(r + 0x20, m_regs[r + 0x120]);
        adpcmaWrite(r + 0x28, m_regs[r + 0x128]);
    }

    m_deltaT.setPostloading(false);
}

template <typename Visit>
void Ym2610::visitState(Visit visit) {
    visit(m_regs);
    visitStState(visit, &m_opn.st);
    visitChannelState(visit, m_ch, 6);

    // 3 slot mode
    visit(m_opn.slot3.fc);
    visit(m_opn.slot3.fnH);
    visit(m_opn.slot3.kcode);

    visit(m_addrA1);

    visit(m_adpcmArrivedEndAddress);
    visitAdpcmaState(visit, m_adpcm);
    m_deltaT.visitState(visit);
}

void Ym2610::saveState(Buffer* buf)
{
    if (!buf) return;

    m_ssg.saveState(buf);
    visitState(StateWriter{buf});
}

void Ym2610::loadState(Buffer* buf)
{
    if (!buf) return;

    m_ssg.loadState(buf);
    visitState(StateReader{buf});

    postload();
}

void Ym2610::init(s32 clock, s32 rate,
                  u8* pcmRomA, u32 pcmSizeA, u8* pcmRomB, u32 pcmSizeB,
                  FmTimerHandler timerHandler, FmIrqHandler irqHandler) {
    m_opn.type = TYPE_YM2610;
    m_opn.channels = m_ch;
    m_opn.st.clock = clock;
    m_opn.st.rate = rate;
    m_opn.st.timerHandler = std::move(timerHandler);
    m_opn.st.irqHandler   = std::move(irqHandler);

    // ADPCM-A
    m_pcmBuf  = pcmRomA;
    m_pcmSize = pcmSizeA;

    // ADPCM-B (Delta-T)
    m_deltaT.setMemory(pcmRomB, pcmSizeB);
    m_deltaT.setStatusHandlers(
        [this](u8 changebits) { m_adpcmArrivedEndAddress |= changebits; },
        [this](u8 changebits) { m_adpcmArrivedEndAddress &= ~changebits; });
    m_deltaT.setEosBit(0x80);       // status flag: set bit 7 on end of sample

    m_ssg.init(clock * 2 / (4 * 2), rate);

    reset();
}

void Ym2610::reset() {
    FmOpn* opn = &m_opn;

    opnSetPrescaler(opn, 6 * 24, 6 * 24);       // OPN 1/6
    m_ssg.reset();

    irqMaskSet(&opn->st, 0x03);
    opn->st.busyExpire = 0;
    opnWriteMode(opn, 0x27, 0x30);              // mode 0, timer reset

    opn->egTimer = 0;
    opn->egCnt   = 0;

    statusReset(&opn->st, 0xff);

    resetChannels(&opn->st, m_ch, 6);

    // Reset the operator parameters.
    for (s32 i = 0xb6; i >= 0xb4; i--) {
        opnWriteReg(opn, i,         0xc0);
        opnWriteReg(opn, i | 0x100, 0xc0);
    }
    for (s32 i = 0xb2; i >= 0x30; i--) {
        opnWriteReg(opn, i,         0);
        opnWriteReg(opn, i | 0x100, 0);
    }
    for (s32 i = 0x26; i >= 0x20; i--) {
        opnWriteReg(opn, i, 0);
    }

    // ADPCM-A
    for (s32 i = 0; i < 6; i++) {
        m_adpcm[i].step = static_cast<u32>(
            static_cast<float>(1 << ADPCM_SHIFT) *
            static_cast<float>(m_opn.st.freqBase) / 3.0f);
        m_adpcm[i].nowAddr   = 0;
        m_adpcm[i].nowStep   = 0;
        m_adpcm[i].start     = 0;
        m_adpcm[i].end       = 0;
        m_adpcm[i].volMul    = 0;
        m_adpcm[i].pan       = &m_outAdpcm[OUTD_CENTER];    // default to centre
        m_adpcm[i].flagMask  = 1 << i;
        m_adpcm[i].flag      = 0;
        m_adpcm[i].adpcmAcc  = 0;
        m_adpcm[i].adpcmStep = 0;
        m_adpcm[i].adpcmOut  = 0;
    }
    m_adpcmTL = 0x3f;

    m_adpcmArrivedEndAddress = 0;

    // Delta-T unit
    m_deltaT.setFreqBase(opn->st.freqBase);
    m_deltaT.setOutput(m_outDelta, 1 << 23);
    m_deltaT.setPortShift(8);                   // always an 8 bit shift
    m_deltaT.reset(OUTD_CENTER, YmDeltaT::EmulationMode::Ym2610);
}

s32 Ym2610::write(s32 a, u8 v) {
    FmOpn* opn = &m_opn;

    v &= 0xff;      // adjust to the 8 bit bus

    switch (a & 3) {
    case 0:         // address port 0
        opn->st.address = v;
        m_addrA1 = 0;

        if (v < 16) {
            m_ssg.write(0, v);
        }
        break;

    case 1: {       // data port 0
        if (m_addrA1 != 0) {
            break;              // verified on a real YM2608
        }

        s32 addr = opn->st.address;
        m_regs[addr] = v;

        switch (addr & 0xf0) {
        case 0x00:              // SSG section
            m_ssg.write(a, v);
            break;

        case 0x10:              // Delta-T ADPCM
            if (m_updateRequest) {
                m_updateRequest();
            }

            switch (addr) {
            case 0x10:          // control 1
            case 0x11:          // control 2
            case 0x12:          // start address L
            case 0x13:          // start address H
            case 0x14:          // stop address L
            case 0x15:          // stop address H
            case 0x19:          // delta-n L
            case 0x1a:          // delta-n H
            case 0x1b:          // volume
                m_deltaT.write(addr - 0x10, v);
                break;

            case 0x1c: {        // flag control: extended status clear/mask
                u8 statusMask = ~v;
                for (s32 ch = 0; ch < 6; ch++) {
                    m_adpcm[ch].flagMask = statusMask & (1 << ch);
                }

                // Status flag: set bit 7 on end of sample.
                m_deltaT.setEosBit(statusMask & 0x80);

                m_adpcmArrivedEndAddress &= statusMask;
                break;
            }

            default:
                break;
            }
            break;

        case 0x20:              // mode register
            if (m_updateRequest) {
                m_updateRequest();
            }
            opnWriteMode(opn, addr, v);
            break;

        default:                // OPN section
            if (m_updateRequest) {
                m_updateRequest();
            }
            opnWriteReg(opn, addr, v);
        }
        break;
    }

    case 2:         // address port 1
        opn->st.address = v;
        m_addrA1 = 1;
        break;

    case 3: {       // data port 1
        if (m_addrA1 != 1) {
            break;              // verified on a real YM2608
        }

        if (m_updateRequest) {
            m_updateRequest();
        }

        s32 addr = opn->st.address;
        m_regs[addr | 0x100] = v;
        if (addr < 0x30) {
            adpcmaWrite(addr, v);       // 0x100-0x12f : ADPCM-A section
        } else {
            opnWriteReg(opn, addr | 0x100, v);
        }
        break;
    }
    }
    return opn->st.irq;
}

u8 Ym2610::read(s32 a) {
    s32 addr = m_opn.st.address;
    u8 ret = 0;

    switch (a & 3) {
    case 0:         // status 0: YM2203 compatible
        ret = statusFlag(&m_opn.st) & 0x83;
        break;

    case 1:         // data 0
        if (addr < 16) {
            ret = m_ssg.read();
        }
        if (addr == 0xff) {
            ret = 0x01;
        }
        break;

    case 2:
        // ADPCM status, the arrived end address: B,--,A5,A4,A3,A2,A1,A0 where
        // B is ADPCM-B (Delta-T) and A0-A5 are the ADPCM-A channels.
        ret = m_adpcmArrivedEndAddress;
        break;

    case 3:
        ret = 0;
        break;
    }
    return ret;
}

s32 Ym2610::timerOver(s32 c) {
    if (c) {                    // timer B
        timerBOver(&m_opn.st);
    } else {                    // timer A
        if (m_updateRequest) {
            m_updateRequest();
        }
        timerAOver(&m_opn.st);

        // CSM mode total level latch and auto key on
        if (m_opn.st.mode & 0x80) {
            csmKeyControl(m_opn.type, &m_ch[2]);
        }
    }
    return m_opn.st.irq;
}

// The SSG section is rendered separately because the driver mixes it in at its
// own level.
void Ym2610::updateSsg(s16* channelA, s16* channelB, s16* channelC, s32 length) {
    m_ssg.update(channelA, channelB, channelC, length);
}


// YM2612

void Ym2612::update(s16* bufL, s16* bufR, s32 length) {
    FmOpn* opn = &m_opn;
    FmState* state = &opn->st;
    s32 dacout = m_dacout;
    s32 dacen  = m_dacen;

    FmChannel* cch[6] = { &m_ch[0], &m_ch[1], &m_ch[2],
                          &m_ch[3], &m_ch[4], &m_ch[5] };

    // Refresh the phase and envelope generators.
    refreshFcEgChan(opn, cch[0]);
    refreshFcEgChan(opn, cch[1]);
    if (state->mode & 0xc0) {           // 3 slot mode
        if (cch[2]->slot[SLOT1].phaseIncrement == -1) {
            refreshFcEgSlot(opn, &cch[2]->slot[SLOT1], opn->slot3.fc[1], opn->slot3.kcode[1]);
            refreshFcEgSlot(opn, &cch[2]->slot[SLOT2], opn->slot3.fc[2], opn->slot3.kcode[2]);
            refreshFcEgSlot(opn, &cch[2]->slot[SLOT3], opn->slot3.fc[0], opn->slot3.kcode[0]);
            refreshFcEgSlot(opn, &cch[2]->slot[SLOT4], cch[2]->fc, cch[2]->kcode);
        }
    } else {
        refreshFcEgChan(opn, cch[2]);
    }
    refreshFcEgChan(opn, cch[3]);
    refreshFcEgChan(opn, cch[4]);
    refreshFcEgChan(opn, cch[5]);

    for (s32 i = 0; i < length; i++) {
        advanceLfo(opn);

        opn->outFm[0] = 0;
        opn->outFm[1] = 0;
        opn->outFm[2] = 0;
        opn->outFm[3] = 0;
        opn->outFm[4] = 0;
        opn->outFm[5] = 0;

        // Calculate FM.
        chanCalc(opn, cch[0], 0);
        chanCalc(opn, cch[1], 1);
        chanCalc(opn, cch[2], 2);
        chanCalc(opn, cch[3], 3);
        chanCalc(opn, cch[4], 4);
        if (dacen) {
            *cch[5]->connect4 += dacout;
        } else {
            chanCalc(opn, cch[5], 5);
        }

        // Advance the envelope generator.
        opn->egTimer += opn->egTimerAdd;
        while (opn->egTimer >= opn->egTimerOverflow) {
            opn->egTimer -= opn->egTimerOverflow;
            opn->egCnt++;

            advanceEgChannel(opn, &cch[0]->slot[SLOT1]);
            advanceEgChannel(opn, &cch[1]->slot[SLOT1]);
            advanceEgChannel(opn, &cch[2]->slot[SLOT1]);
            advanceEgChannel(opn, &cch[3]->slot[SLOT1]);
            advanceEgChannel(opn, &cch[4]->slot[SLOT1]);
            advanceEgChannel(opn, &cch[5]->slot[SLOT1]);
        }

        s32 lt  = opn->outFm[0] & opn->pan[0];
        s32 rt  = opn->outFm[0] & opn->pan[1];
        lt += opn->outFm[1] & opn->pan[2];
        rt += opn->outFm[1] & opn->pan[3];
        lt += opn->outFm[2] & opn->pan[4];
        rt += opn->outFm[2] & opn->pan[5];
        lt += opn->outFm[3] & opn->pan[6];
        rt += opn->outFm[3] & opn->pan[7];
        lt += opn->outFm[4] & opn->pan[8];
        rt += opn->outFm[4] & opn->pan[9];
        lt += opn->outFm[5] & opn->pan[10];
        rt += opn->outFm[5] & opn->pan[11];

        limit(lt, MAXOUT, MINOUT);
        limit(rt, MAXOUT, MINOUT);

        bufL[i] = lt;
        bufR[i] = rt;
    }
}

void Ym2612::init(s32 clock, s32 rate,
                  FmTimerHandler timerHandler, FmIrqHandler irqHandler) {
    m_opn.type = TYPE_YM2612;
    m_opn.channels = m_ch;
    m_opn.st.clock = clock;
    m_opn.st.rate = rate;
    m_opn.st.timerHandler = std::move(timerHandler);
    m_opn.st.irqHandler   = std::move(irqHandler);

    reset();
}

void Ym2612::reset() {
    FmOpn* opn = &m_opn;

    opnSetPrescaler(opn, 6 * 24, 6 * 24);

    irqMaskSet(&opn->st, 0x03);
    opn->st.busyExpire = 0;
    opnWriteMode(opn, 0x27, 0x30);              // mode 0, timer reset

    opn->egTimer = 0;
    opn->egCnt   = 0;

    statusReset(&opn->st, 0xff);

    resetChannels(&opn->st, &m_ch[0], 6);

    for (s32 i = 0xb6; i >= 0xb4; i--) {
        opnWriteReg(opn, i,         0xc0);
        opnWriteReg(opn, i | 0x100, 0xc0);
    }
    for (s32 i = 0xb2; i >= 0x30; i--) {
        opnWriteReg(opn, i,         0);
        opnWriteReg(opn, i | 0x100, 0);
    }
    for (s32 i = 0x26; i >= 0x20; i--) {
        opnWriteReg(opn, i, 0);
    }

    m_dacen = 0;
}

s32 Ym2612::write(s32 a, u8 v) {
    v &= 0xff;      // adjust to the 8 bit bus

    switch (a & 3) {
    case 0:         // address port 0
        m_opn.st.address = v;
        m_addrA1 = 0;
        break;

    case 1: {       // data port 0
        if (m_addrA1 != 0) {
            break;              // verified on a real YM2608
        }

        s32 addr = m_opn.st.address;
        m_regs[addr] = v;

        switch (addr & 0xf0) {
        case 0x20:              // 0x20-0x2f mode
            switch (addr) {
            case 0x2a:          // DAC data; the level is unknown
                m_dacout = (static_cast<s32>(v) - 0x80) << 6;
                break;

            case 0x2b:          // DAC select, b7 = DAC enable
                m_dacen = v & 0x80;
                break;

            default:            // OPN section
                opnWriteMode(&m_opn, addr, v);
            }
            break;

        default:                // 0x30-0xff OPN section
            opnWriteReg(&m_opn, addr, v);
        }
        break;
    }

    case 2:         // address port 1
        m_opn.st.address = v;
        m_addrA1 = 1;
        break;

    case 3: {       // data port 1
        if (m_addrA1 != 1) {
            break;              // verified on a real YM2608
        }

        s32 addr = m_opn.st.address;
        m_regs[addr | 0x100] = v;
        opnWriteReg(&m_opn, addr | 0x100, v);
        break;
    }
    }
    return m_opn.st.irq;
}

u8 Ym2612::read(s32 a) {
    switch (a & 3) {
    case 0:                     // status 0
    case 1:
    case 2:
    case 3:
        // Reading an unmapped area returns the status flags too.
        return statusFlag(&m_opn.st);
    }
    return 0;
}

s32 Ym2612::timerOver(s32 c) {
    if (c) {                    // timer B
        timerBOver(&m_opn.st);
    } else {                    // timer A
        timerAOver(&m_opn.st);

        // CSM mode total level latch and auto key on
        if (m_opn.st.mode & 0x80) {
            csmKeyControl(m_opn.type, &m_ch[2]);
        }
    }
    return m_opn.st.irq;
}

void Ym2612::postload() {
    // OPN registers: DT/MULTI, TL, KS/AR, AMON/DR, SR, SL/RR, SSG-EG
    for (s32 r = 0x30; r < 0x9e; r++) {
        if ((r & 3) != 3) {
            opnWriteReg(&m_opn, r, m_regs[r]);
            opnWriteReg(&m_opn, r | 0x100, m_regs[r | 0x100]);
        }
    }

    // FB/CONNECT, L/R/AMS/PMS
    for (s32 r = 0xb0; r < 0xb6; r++) {
        if ((r & 3) != 3) {
            opnWriteReg(&m_opn, r, m_regs[r]);
            opnWriteReg(&m_opn, r | 0x100, m_regs[r | 0x100]);
        }
    }
}

template <typename Visit>
void Ym2612::visitState(Visit visit) {
    visit(m_regs);
    visitStState(visit, &m_opn.st);
    visitChannelState(visit, m_ch, 6);

    // 3 slot mode
    visit(m_opn.slot3.fc);
    visit(m_opn.slot3.fnH);
    visit(m_opn.slot3.kcode);

    visit(m_addrA1);

    // DAC
    visit(m_dacen);
    visit(m_dacout);
}

void Ym2612::saveState(Buffer* buf)
{
    if (!buf) return;

    visitState(StateWriter{buf});
}

void Ym2612::loadState(Buffer* buf)
{
    if (!buf) return;

    visitState(StateReader{buf});

    postload();
}


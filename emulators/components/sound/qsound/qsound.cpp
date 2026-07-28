// QSound (DL-1425) emulator
// Adapted from the FBNeo implementation by superctr (Ian Karlsson)

#include "qsound.h"

#include <array>
#include <cstdlib>
#include <cstring>

namespace {

constexpr s32 CLOCK = 60000000;
constexpr s32 CLOCK_DIVIDER = 2496;

constexpr s32 clampValue(s32 x, s32 low, s32 high) {
    return (x > high) ? high : ((x < low) ? low : x);
}

// DSP states
enum : u16 {
    STATE_INIT1 = 0x288,
    STATE_INIT2 = 0x61a,
    STATE_REFRESH1 = 0x039,
    STATE_REFRESH2 = 0x04f,
    STATE_NORMAL1 = 0x314,
    STATE_NORMAL2 = 0x6b2
};

enum { PANTBL_LEFT = 0, PANTBL_RIGHT = 1 };
enum { PANTBL_DRY = 0, PANTBL_WET = 1 };

constexpr std::array<s16, 33> DRY_MIX_TABLE = {
    -16384, -16384, -16384, -16384, -16384, -16384, -16384, -16384,
    -16384, -16384, -16384, -16384, -16384, -16384, -16384, -16384,
    -16384, -14746, -13107, -11633, -10486,  -9175,  -8520,  -7209,
     -6226,  -5226,  -4588,  -3768,  -3277,  -2703,  -2130,  -1802,
         0
};

constexpr std::array<s16, 33> WET_MIX_TABLE = {
         0,  -1638,  -1966,  -2458,  -2949,  -3441,  -4096,  -4669,
     -4915,  -5120,  -5489,  -6144,  -7537,  -8831,  -9339,  -9830,
    -10240, -10322, -10486, -10568, -10650, -11796, -12288, -12288,
    -12534, -12648, -12780, -12829, -12943, -13107, -13418, -14090,
    -16384
};

constexpr std::array<s16, 33> LINEAR_MIX_TABLE = {
    -16379, -16338, -16257, -16135, -15973, -15772, -15531, -15251,
    -14934, -14580, -14189, -13763, -13303, -12810, -12284, -11729,
    -11729, -11144, -10531,  -9893,  -9229,  -8543,  -7836,  -7109,
     -6364,  -5604,  -4829,  -4043,  -3246,  -2442,  -1631,   -817,
         0
};

using PanTables = std::array<std::array<std::array<s16, 98>, 2>, 2>;

constexpr PanTables makePanTables() {
    PanTables tables{};
    for (s32 i = 0; i < 33; i++) {
        // dry mixing levels
        tables[PANTBL_LEFT][PANTBL_DRY][i] = DRY_MIX_TABLE[i];
        tables[PANTBL_RIGHT][PANTBL_DRY][i] = DRY_MIX_TABLE[32 - i];
        // wet mixing levels
        tables[PANTBL_LEFT][PANTBL_WET][i] = WET_MIX_TABLE[i];
        tables[PANTBL_RIGHT][PANTBL_WET][i] = WET_MIX_TABLE[32 - i];
        // linear panning, only for the dry component; the wet component is muted
        tables[PANTBL_LEFT][PANTBL_DRY][i + 0x30] = LINEAR_MIX_TABLE[i];
        tables[PANTBL_RIGHT][PANTBL_DRY][i + 0x30] = LINEAR_MIX_TABLE[32 - i];
    }
    return tables;
}

constexpr PanTables PAN_TABLES = makePanTables();

constexpr std::array<std::array<s16, 95>, 5> FILTER_DATA = {{
    {   // d53 - 0
        0,0,0,6,44,-24,-53,-10,59,-40,-27,1,39,-27,56,127,174,36,-13,49,
        212,142,143,-73,-20,66,-108,-117,-399,-265,-392,-569,-473,-71,95,-319,-218,-230,331,638,
        449,477,-180,532,1107,750,9899,3828,-2418,1071,-176,191,-431,64,117,-150,-274,-97,-238,165,
        166,250,-19,4,37,204,186,-6,140,-77,-1,1,18,-10,-151,-149,-103,-9,55,23,
        -102,-97,-11,13,-48,-27,5,18,-61,-30,64,72,0,0,0,
    },
    {   // db2 - 1 - default left filter
        0,0,0,85,24,-76,-123,-86,-29,-14,-20,-7,6,-28,-87,-89,-5,100,154,160,
        150,118,41,-48,-78,-23,59,83,-2,-176,-333,-344,-203,-66,-39,2,224,495,495,280,
        432,1340,2483,5377,1905,658,0,97,347,285,35,-95,-78,-82,-151,-192,-171,-149,-147,-113,
        -22,71,118,129,127,110,71,31,20,36,46,23,-27,-63,-53,-21,-19,-60,-92,-69,
        -12,25,29,30,40,41,29,30,46,39,-15,-74,0,0,0,
    },
    {   // e11 - 2 - default right filter
        0,0,0,23,42,47,29,10,2,-14,-54,-92,-93,-70,-64,-77,-57,18,94,113,
        87,69,67,50,25,29,58,62,24,-39,-131,-256,-325,-234,-45,58,78,223,485,496,
        127,6,857,2283,2683,4928,1328,132,79,314,189,-80,-90,35,-21,-186,-195,-99,-136,-258,
        -189,82,257,185,53,41,84,68,38,63,77,14,-60,-71,-71,-120,-151,-84,14,29,
        -8,7,66,69,12,-3,54,92,52,-6,-15,-2,0,0,0,
    },
    {   // e70 - 3
        0,0,0,2,-28,-37,-17,0,-9,-22,-3,35,52,39,20,7,-6,2,55,121,
        129,67,8,1,9,-6,-16,16,66,96,118,130,75,-47,-92,43,223,239,151,219,
        440,475,226,206,940,2100,2663,4980,865,49,-33,186,231,103,42,114,191,184,116,29,
        -47,-72,-21,60,96,68,31,32,63,87,76,39,7,14,55,85,67,18,-12,-3,
        21,34,29,6,-27,-49,-37,-2,16,0,-21,-16,0,0,0,
    },
    {   // ecf - 4
        0,0,0,48,7,-22,-29,-10,24,54,59,29,-36,-117,-185,-213,-185,-99,13,90,
        83,24,-5,23,53,47,38,56,67,57,75,107,16,-242,-440,-355,-120,-33,-47,152,
        501,472,-57,-292,544,1937,2277,6145,1240,153,47,200,152,36,64,134,74,-82,-208,-266,
        -268,-188,-42,65,74,56,89,133,114,44,-3,-1,17,29,29,-2,-76,-156,-187,-151,
        -85,-31,-5,7,20,32,24,-5,-20,6,48,62,0,0,0,
    }
}};

constexpr std::array<s16, 209> FILTER_DATA2 = {
    // f2e - following 95 values used for "disable output" filter
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,

    // f73 - following 45 values used for "mode 2" filter (overlaps with f2e)
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,
    -371,-196,-268,-512,-303,-315,-184,-76,276,-256,298,196,990,236,1114,-126,4377,6549,791,

    // fa0 - filtering disabled (for 95-taps) (use fa3 or fa4 for mode2 filters)
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,-16384,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

constexpr std::array<s16, 16> ADPCM_STEP_TABLE = {
    154, 154, 128, 102, 77, 58, 58, 58,
     58,  58,  58,  58, 77, 102, 128, 154
};

const s16* getFilterTable(u16 offset) {
    if (offset >= 0xf2e && offset < 0xfff) {
        return &FILTER_DATA2[offset - 0xf2e];  // overlapping filter data
    }

    s32 index = (offset - 0xd53) / 95;
    if (index >= 0 && index < 5) {
        return FILTER_DATA[index].data();  // normal tables
    }

    return nullptr;  // no filter found
}

}  // namespace

void Qsound::buildRegisterMap() {
    for (s32 i = 0; i < 256; i++) {
        m_registerMap[i] = nullptr;
    }

    // PCM voices
    for (s32 i = 0; i < 16; i++) {
        // The bank register applies to the next channel
        m_registerMap[(i << 3) + 0] = reinterpret_cast<u16*>(&m_voice[(i + 1) % 16].bank);
        // Current sample position and start position
        m_registerMap[(i << 3) + 1] = reinterpret_cast<u16*>(&m_voice[i].addr);
        m_registerMap[(i << 3) + 2] = reinterpret_cast<u16*>(&m_voice[i].rate);
        m_registerMap[(i << 3) + 3] = reinterpret_cast<u16*>(&m_voice[i].phase);
        m_registerMap[(i << 3) + 4] = reinterpret_cast<u16*>(&m_voice[i].loopLen);
        m_registerMap[(i << 3) + 5] = reinterpret_cast<u16*>(&m_voice[i].endAddr);
        m_registerMap[(i << 3) + 6] = reinterpret_cast<u16*>(&m_voice[i].volume);
        m_registerMap[(i << 3) + 7] = nullptr;  // unused
        m_registerMap[i + 0x80] = reinterpret_cast<u16*>(&m_voicePan[i]);
        m_registerMap[i + 0xba] = reinterpret_cast<u16*>(&m_voice[i].echo);
    }

    // ADPCM voices. The ADPCM sample rate is fixed at 8kHz, so one channel is
    // updated every third DSP sample.
    for (s32 i = 0; i < 3; i++) {
        m_registerMap[(i << 2) + 0xca] = reinterpret_cast<u16*>(&m_adpcm[i].startAddr);
        m_registerMap[(i << 2) + 0xcb] = reinterpret_cast<u16*>(&m_adpcm[i].endAddr);
        m_registerMap[(i << 2) + 0xcc] = reinterpret_cast<u16*>(&m_adpcm[i].bank);
        m_registerMap[(i << 2) + 0xcd] = reinterpret_cast<u16*>(&m_adpcm[i].volume);
        // non-zero to start ADPCM playback
        m_registerMap[i + 0xd6] = reinterpret_cast<u16*>(&m_adpcm[i].flag);
        m_registerMap[i + 0x90] = reinterpret_cast<u16*>(&m_voicePan[16 + i]);
    }

    // QSound registers
    m_registerMap[0x93] = reinterpret_cast<u16*>(&m_echo.feedback);
    m_registerMap[0xd9] = reinterpret_cast<u16*>(&m_echo.endPos);
    m_registerMap[0xe2] = reinterpret_cast<u16*>(&m_delayUpdate);
    m_registerMap[0xe3] = reinterpret_cast<u16*>(&m_nextState);

    for (s32 i = 0; i < 2; i++) {  // left, right
        // Wet
        m_registerMap[(i << 1) + 0xda] = reinterpret_cast<u16*>(&m_filter[i].tablePos);
        m_registerMap[(i << 1) + 0xde] = reinterpret_cast<u16*>(&m_wet[i].delay);
        m_registerMap[(i << 1) + 0xe4] = reinterpret_cast<u16*>(&m_wet[i].volume);
        // Dry
        m_registerMap[(i << 1) + 0xdb] = reinterpret_cast<u16*>(&m_altFilter[i].tablePos);
        m_registerMap[(i << 1) + 0xdf] = reinterpret_cast<u16*>(&m_dry[i].delay);
        m_registerMap[(i << 1) + 0xe5] = reinterpret_cast<u16*>(&m_dry[i].volume);
    }
}

s16 Qsound::readSample(u16 bank, u16 address) const {
    u32 romMask = m_sampleRomSize ? m_sampleRomSize - 1 : 0;

    if (!romMask) {
        return 0;  // no ROM loaded
    }
    if (!(bank & 0x8000)) {
        return 0;  // ignore attempts to read from DSP program ROM
    }

    bank &= 0x7FFF;
    u32 romAddr = (bank << 16) | address;

    u8 sampleData = m_sampleRom[romAddr & romMask];

    return static_cast<s16>((sampleData << 8) & 0xff00);
}

// Runs one DSP sample
void Qsound::updateSample() {
    switch (m_state) {
        default:
        case STATE_INIT1:
        case STATE_INIT2:
            stateInit();
            return;
        case STATE_REFRESH1:
            stateRefreshFilter1();
            return;
        case STATE_REFRESH2:
            stateRefreshFilter2();
            return;
        case STATE_NORMAL1:
        case STATE_NORMAL2:
            stateNormalUpdate();
            return;
    }
}

void Qsound::stateInit() {
    s32 mode = (m_state == STATE_INIT2) ? 1 : 0;

    // we're busy for 4 samples, including the filter refresh
    if (m_stateCounter >= 2) {
        m_stateCounter = 0;
        m_state = m_nextState;
        return;
    } else if (m_stateCounter == 1) {
        m_stateCounter++;
        return;
    }

    memset(m_voice, 0, sizeof(m_voice));
    memset(m_adpcm, 0, sizeof(m_adpcm));
    memset(m_filter, 0, sizeof(m_filter));
    memset(m_altFilter, 0, sizeof(m_altFilter));
    memset(m_wet, 0, sizeof(m_wet));
    memset(m_dry, 0, sizeof(m_dry));
    memset(&m_echo, 0, sizeof(m_echo));

    for (s32 i = 0; i < 19; i++) {
        m_voicePan[i] = 0x120;
        m_voiceOutput[i] = 0;
    }

    for (s32 i = 0; i < 16; i++) {
        m_voice[i].bank = 0x8000;
    }
    for (s32 i = 0; i < 3; i++) {
        m_adpcm[i].bank = 0x8000;
    }

    if (mode == 0) {
        // mode 1
        m_wet[0].delay = 0;
        m_dry[0].delay = 46;
        m_wet[1].delay = 0;
        m_dry[1].delay = 48;
        m_filter[0].tablePos = 0xdb2;
        m_filter[1].tablePos = 0xe11;
        m_echo.endPos = 0x554 + 6;
        m_nextState = STATE_REFRESH1;
    } else {
        // mode 2
        m_wet[0].delay = 1;
        m_dry[0].delay = 0;
        m_wet[1].delay = 0;
        m_dry[1].delay = 0;
        m_filter[0].tablePos = 0xf73;
        m_filter[1].tablePos = 0xfa4;
        m_altFilter[0].tablePos = 0xf73;
        m_altFilter[1].tablePos = 0xfa4;
        m_echo.endPos = 0x53c + 6;
        m_nextState = STATE_REFRESH2;
    }

    m_wet[0].volume = 0x3fff;
    m_dry[0].volume = 0x3fff;
    m_wet[1].volume = 0x3fff;
    m_dry[1].volume = 0x3fff;

    m_delayUpdate = 1;
    m_readyFlag = 0;
    m_stateCounter = 1;
}

// Updates the filter parameters for mode 1
void Qsound::stateRefreshFilter1() {
    for (s32 ch = 0; ch < 2; ch++) {
        m_filter[ch].delayPos = 0;
        m_filter[ch].tapCount = 95;

        const s16* table = getFilterTable(static_cast<u16>(m_filter[ch].tablePos));
        if (table != nullptr) {
            memcpy(m_filter[ch].taps, table, 95 * sizeof(s16));
        }
    }

    m_state = m_nextState = STATE_NORMAL1;
}

// Updates the filter parameters for mode 2
void Qsound::stateRefreshFilter2() {
    for (s32 ch = 0; ch < 2; ch++) {
        m_filter[ch].delayPos = 0;
        m_filter[ch].tapCount = 45;

        const s16* table = getFilterTable(static_cast<u16>(m_filter[ch].tablePos));
        if (table != nullptr) {
            memcpy(m_filter[ch].taps, table, 45 * sizeof(s16));
        }

        m_altFilter[ch].delayPos = 0;
        m_altFilter[ch].tapCount = 44;

        table = getFilterTable(static_cast<u16>(m_altFilter[ch].tablePos));
        if (table != nullptr) {
            memcpy(m_altFilter[ch].taps, table, 44 * sizeof(s16));
        }
    }

    m_state = m_nextState = STATE_NORMAL2;
}

// Updates one of the 16 PCM voices. All of them are updated every sample, with
// full rate and volume control.
s16 Qsound::pcmUpdate(s32 voiceNo, s32* echoOut) {
    Voice* v = &m_voice[voiceNo];

    // Read the sample from ROM and apply the volume
    s16 output = static_cast<s16>((v->volume * readSample(v->bank, static_cast<u16>(v->addr))) >> 14);

    *echoOut += (output * v->echo) << 2;

    // Add the delta to the phase and loop back if required
    s32 newPhase = v->rate + ((v->addr << 12) | (v->phase >> 4));

    if ((newPhase >> 12) >= v->endAddr) {
        newPhase -= (v->loopLen << 12);
    }

    newPhase = clampValue(newPhase, -0x8000000, 0x7FFFFFF);
    v->addr = static_cast<s16>(newPhase >> 12);
    v->phase = static_cast<u16>((newPhase << 4) & 0xffff);

    return output;
}

// Updates one of the 3 ADPCM voices. One is updated every sample, effectively
// making the ADPCM rate a third of the DSP sample rate, and the volume is
// latched only when a sample starts.
void Qsound::adpcmUpdate(s32 voiceNo, s32 nibble) {
    Adpcm* v = &m_adpcm[voiceNo];

    s8 step;

    if (!nibble) {
        // Mute the voice when it reaches the end address
        if (v->curAddr == v->endAddr) {
            v->curVol = 0;
        }

        // Playback start flag
        if (v->flag) {
            m_voiceOutput[16 + voiceNo] = 0;
            v->flag = 0;
            v->stepSize = 10;
            v->curVol = v->volume;
            v->curAddr = v->startAddr;
        }

        // get top nibble
        step = static_cast<s8>(readSample(v->bank, v->curAddr) >> 8);
    } else {
        // get bottom nibble
        step = static_cast<s8>(readSample(v->bank, v->curAddr++) >> 4);
    }

    // shift with sign extend
    step = static_cast<s8>(step >> 4);

    // delta = (0.5 + abs(step)) * step_size
    s32 delta = ((1 + abs(step << 1)) * v->stepSize) >> 1;
    if (step <= 0) {
        delta = -delta;
    }
    delta += m_voiceOutput[16 + voiceNo];
    delta = clampValue(delta, -32768, 32767);

    m_voiceOutput[16 + voiceNo] = static_cast<s16>((delta * v->curVol) >> 16);

    v->stepSize = static_cast<s16>((ADPCM_STEP_TABLE[8 + step] * v->stepSize) >> 6);
    v->stepSize = static_cast<s16>(clampValue(v->stepSize, 1, 2000));
}

// The echo effect is pretty simple. A moving average filter is used on the
// output from the delay line to smooth samples over time.
s16 Qsound::applyEcho(s32 input) {
    // get the average of the last 2 samples from the delay line
    s32 oldSample = m_echo.delayLine[m_echo.delayPos];
    s32 lastSample = m_echo.lastSample;

    m_echo.lastSample = static_cast<s16>(oldSample);
    oldSample = (oldSample + lastSample) >> 1;

    // add the current sample to the delay line
    s32 newSample = input + ((oldSample * m_echo.feedback) << 2);
    m_echo.delayLine[m_echo.delayPos++] = static_cast<s16>(newSample >> 16);

    if (m_echo.delayPos >= m_echo.length) {
        m_echo.delayPos = 0;
    }

    return static_cast<s16>(oldSample);
}

void Qsound::stateNormalUpdate() {
    s32 echoInput = 0;

    m_readyFlag = 0x80;

    // recalculate the echo length
    if (m_state == STATE_NORMAL2) {
        m_echo.length = static_cast<s16>(m_echo.endPos - 0x53c);
    } else {
        m_echo.length = static_cast<s16>(m_echo.endPos - 0x554);
    }

    m_echo.length = static_cast<s16>(clampValue(m_echo.length, 0, 1024));

    // update the PCM voices
    for (s32 v = 0; v < 16; v++) {
        m_voiceOutput[v] = pcmUpdate(v, &echoInput);
    }

    // update the ADPCM voices (one every third sample)
    adpcmUpdate(m_stateCounter % 3, m_stateCounter / 3);

    s16 echoOutput = applyEcho(echoInput);

    // now, we do the magic stuff
    for (s32 ch = 0; ch < 2; ch++) {
        // Echo is output on the unfiltered component of the left channel and
        // the filtered component of the right channel.
        s32 wet = (ch == 1) ? echoOutput << 14 : 0;
        s32 dry = (ch == 0) ? echoOutput << 14 : 0;

        for (s32 v = 0; v < 19; v++) {
            u16 panIndex = static_cast<u16>(m_voicePan[v] - 0x110);
            if (panIndex > 97) {
                panIndex = 97;
            }

            // Apply different volume tables to the dry and wet inputs
            dry -= (m_voiceOutput[v] * PAN_TABLES[ch][PANTBL_DRY][panIndex]);
            wet -= (m_voiceOutput[v] * PAN_TABLES[ch][PANTBL_WET][panIndex]);
        }

        // Saturate the accumulated voices
        dry = clampValue(dry, -0x1fffffff, 0x1fffffff) << 2;
        wet = clampValue(wet, -0x1fffffff, 0x1fffffff) << 2;

        // Apply the FIR filter to the 'wet' input
        wet = applyFir(&m_filter[ch], static_cast<s16>(wet >> 16));

        // in mode 2, we do this to the 'dry' input too
        if (m_state == STATE_NORMAL2) {
            dry = applyFir(&m_altFilter[ch], static_cast<s16>(dry >> 16));
        }

        // the output goes through a delay line and attenuation
        s32 output = applyDelay(&m_wet[ch], wet) + applyDelay(&m_dry[ch], dry);

        // DSP round function
        output = ((output + 0x2000) & ~0x3fff) >> 14;
        m_out[ch] = static_cast<s16>(clampValue(output, -0x7fff, 0x7fff));

        if (m_delayUpdate) {
            refreshDelayReadPos(&m_wet[ch]);
            refreshDelayReadPos(&m_dry[ch]);
        }
    }

    m_delayUpdate = 0;

    // after 6 samples, the next state is executed
    m_stateCounter++;
    if (m_stateCounter > 5) {
        m_stateCounter = 0;
        m_state = m_nextState;
    }
}

// Apply the FIR filter used as the Q1 transfer function
s32 Qsound::applyFir(Fir* f, s16 input) {
    s32 output = 0;
    s32 tap = 0;

    for (; tap < (f->tapCount - 1); tap++) {
        output -= (f->taps[tap] * f->delayLine[f->delayPos++]) << 2;

        if (f->delayPos >= f->tapCount - 1) {
            f->delayPos = 0;
        }
    }

    output -= (f->taps[tap] * input) << 2;

    f->delayLine[f->delayPos++] = input;
    if (f->delayPos >= f->tapCount - 1) {
        f->delayPos = 0;
    }

    return output;
}

// Apply the delay line and the component volume
s32 Qsound::applyDelay(Delay* d, s32 input) {
    d->delayLine[d->writePos++] = static_cast<s16>(input >> 16);
    if (d->writePos >= 51) {
        d->writePos = 0;
    }

    s32 output = d->delayLine[d->readPos++] * d->volume;
    if (d->readPos >= 51) {
        d->readPos = 0;
    }

    return output;
}

// Move the delay read position to match the new delay length
void Qsound::refreshDelayReadPos(Delay* d) {
    s16 newReadPos = static_cast<s16>((d->writePos - d->delay) % 51);
    if (newReadPos < 0) {
        newReadPos = static_cast<s16>(newReadPos + 51);
    }

    d->readPos = newReadPos;
}

void Qsound::init(u32 sampleRate) {
    buildRegisterMap();
    setSampleRate(sampleRate);
    reset();
}

void Qsound::reset() {
    m_readyFlag = 0;
    m_out[0] = m_out[1] = 0;
    m_state = 0;
    m_stateCounter = 0;
    m_delta = 0;
}

void Qsound::setSampleRate(u32 sampleRate) {
    // Phase increment of the fixed-rate DSP clock per host sample, 4.12
    m_advance =
        sampleRate ? static_cast<s32>(static_cast<s64>(0x1000) * CLOCK / CLOCK_DIVIDER / sampleRate)
                   : 0;
}

void Qsound::setSampleROM(const u8* rom, u32 size) {
    m_sampleRom = rom;
    m_sampleRomSize = size;
}

void Qsound::write(u8 reg, u16 value) {
    u16* destination = m_registerMap[reg];
    if (destination) {
        *destination = value;
    }
    m_readyFlag = 0;
}

void Qsound::update(u32 samples) {
    for (u32 i = 0; i < samples; i++) {
        m_delta += m_advance;
        while (m_delta > 0xfff) {
            updateSample();
            m_delta -= 0x1000;
        }
    }
}

template <typename Visit>
void Qsound::visitState(Visit visit) {
    visit(m_out);

    for (Voice& v : m_voice) {
        visit(v.bank);
        visit(v.addr);
        visit(v.phase);
        visit(v.rate);
        visit(v.loopLen);
        visit(v.endAddr);
        visit(v.volume);
        visit(v.echo);
    }

    for (Adpcm& a : m_adpcm) {
        visit(a.startAddr);
        visit(a.endAddr);
        visit(a.bank);
        visit(a.volume);
        visit(a.flag);
        visit(a.curVol);
        visit(a.stepSize);
        visit(a.curAddr);
    }

    visit(m_voicePan);
    visit(m_voiceOutput);

    visit(m_echo.endPos);
    visit(m_echo.feedback);
    visit(m_echo.length);
    visit(m_echo.lastSample);
    visit(m_echo.delayLine);
    visit(m_echo.delayPos);

    auto visitFir = [&visit](Fir& f) {
        visit(f.tapCount);
        visit(f.delayPos);
        visit(f.tablePos);
        visit(f.taps);
        visit(f.delayLine);
    };
    for (Fir& f : m_filter) {
        visitFir(f);
    }
    for (Fir& f : m_altFilter) {
        visitFir(f);
    }

    auto visitDelay = [&visit](Delay& d) {
        visit(d.delay);
        visit(d.volume);
        visit(d.writePos);
        visit(d.readPos);
        visit(d.delayLine);
    };
    for (Delay& d : m_wet) {
        visitDelay(d);
    }
    for (Delay& d : m_dry) {
        visitDelay(d);
    }

    visit(m_state);
    visit(m_nextState);
    visit(m_delayUpdate);
    visit(m_stateCounter);
    visit(m_readyFlag);
    visit(m_delta);
}

void Qsound::saveState(Buffer* buf) {
    visitState(StateWriter{buf});
}

void Qsound::loadState(Buffer* buf) {
    visitState(StateReader{buf});
}

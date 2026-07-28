#pragma once

#include "../../../types.h"
#include "../../buffer.h"

// QSound (DL-1425) emulator.
// Adapted from the FBNeo implementation by superctr (Ian Karlsson).
//
// The chip is a DSP running at a fixed 60MHz/2496 sample rate, independent of
// the host sample rate. update() advances it by whole DSP samples and latches
// the last stereo pair, which the caller reads back.
class Qsound {
public:
    Qsound() = default;

    // m_registerMap holds pointers into this object, so a copy would decode
    // writes into the original's state.
    Qsound(const Qsound&) = delete;
    Qsound& operator=(const Qsound&) = delete;

    void init(u32 sampleRate);
    void reset();
    void setSampleRate(u32 sampleRate);

    // The sample ROM stays owned by the cartridge; only the pointer is kept.
    void setSampleROM(const u8* rom, u32 size);

    void write(u8 reg, u16 value);
    u8 readStatus() const { return m_readyFlag; }

    void update(u32 samples);
    s16 getLeftSample() const { return m_out[0]; }
    s16 getRightSample() const { return m_out[1]; }

    void saveState(Buffer* buf);
    void loadState(Buffer* buf);

private:
    struct Voice {
        u16 bank;
        s16 addr;  // top word of the sample address
        u16 phase;
        u16 rate;  // 4.12 fixed point
        s16 loopLen;
        s16 endAddr;
        s16 volume;
        s16 echo;
    };

    struct Adpcm {
        u16 startAddr;
        u16 endAddr;
        u16 bank;
        s16 volume;
        u16 flag;
        s16 curVol;
        s16 stepSize;
        u16 curAddr;
    };

    // Q1 transfer function
    struct Fir {
        s32 tapCount;  // 95 in mode 1, 45 (wet) / 44 (dry) in mode 2
        s32 delayPos;
        s16 tablePos;
        s16 taps[95];
        s16 delayLine[95];
    };

    struct Delay {
        s16 delay;
        s16 volume;
        s16 writePos;
        s16 readPos;
        s16 delayLine[51];
    };

    struct Echo {
        u16 endPos;
        s16 feedback;
        s16 length;
        s16 lastSample;
        s16 delayLine[1024];
        s16 delayPos;
    };

    void buildRegisterMap();

    void updateSample();
    void stateInit();
    void stateRefreshFilter1();
    void stateRefreshFilter2();
    void stateNormalUpdate();

    s16 readSample(u16 bank, u16 address) const;
    s16 pcmUpdate(s32 voiceNo, s32* echoOut);
    void adpcmUpdate(s32 voiceNo, s32 nibble);
    s16 applyEcho(s32 input);
    static s32 applyFir(Fir* f, s16 input);
    static s32 applyDelay(Delay* d, s32 input);
    static void refreshDelayReadPos(Delay* d);

    template <typename Visit>
    void visitState(Visit visit);

    s16 m_out[2] = {};

    Voice m_voice[16] = {};
    Adpcm m_adpcm[3] = {};

    u16 m_voicePan[16 + 3] = {};
    s16 m_voiceOutput[16 + 3] = {};

    Echo m_echo = {};

    Fir m_filter[2] = {};
    Fir m_altFilter[2] = {};  // mode 2 only: also filters the dry component

    Delay m_wet[2] = {};
    Delay m_dry[2] = {};

    u16 m_state = 0;
    u16 m_nextState = 0;
    u16 m_delayUpdate = 0;  // non-zero to recompute the delay read positions
    s32 m_stateCounter = 0;
    u8 m_readyFlag = 0;

    // Address decode: maps each 8-bit register index onto the field it writes.
    u16* m_registerMap[256] = {};

    const u8* m_sampleRom = nullptr;
    u32 m_sampleRomSize = 0;

    s32 m_advance = 0;  // DSP phase increment per host sample, 4.12
    s32 m_delta = 0;
};

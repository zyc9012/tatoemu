// Software emulation of the Yamaha YM2151 (OPM) FM sound generator.
//
// Eight four-operator FM channels sharing one LFO, a noise generator wired to
// channel 7, and two timers driving an interrupt line.
//
// Derived from the MAME driver by Jarek Burczynski, with optimisation ideas by
// Tatsuyuki Satoh (version 2.150 final beta).
#pragma once

#include <functional>

#include "../../../types.h"
#include "../../buffer.h"

class Ym2151 {
public:
    // Called whenever the timer A/B interrupt line changes state.
    using IrqHandler = std::function<void(bool asserted)>;

    void init(u32 clock, u32 sampleRate);
    void reset();

    // Recomputes every rate-dependent table, so it is not cheap.
    void setSampleRate(u32 sampleRate);

    void setIrqHandler(IrqHandler handler) { m_irqHandler = std::move(handler); }

    void write(u8 reg, u8 value);
    u8 readStatus() const { return static_cast<u8>(m_status); }

    // Renders `samples` samples into the two mono buffers (overwrites them).
    void update(s16* left, s16* right, u32 samples);

    void saveState(Buffer* buf);
    void loadState(Buffer* buf);

private:
    // One operator, which the datasheet calls a slot.
    struct Operator {
        u32 phase;              // accumulated operator phase
        u32 freq;               // operator frequency count
        s32 dt1;                // current DT1 (detune 1) phase increment
        u32 multiple;           // frequency count multiply
        u32 dt1Index;           // DT1 index * 32
        u32 dt2;                // current DT2 (detune 2) value

        s32* connect;           // where this operator's output goes

        // Only M1 (operator 0) uses these:
        s32* memConnect;        // where to put the delayed sample (MEM)
        s32  memValue;          // delayed sample (MEM) value

        // Channel wide data, held by operator 0 of each channel:
        u32 feedbackShift;
        s32 feedbackOutCurrent;
        s32 feedbackOutPrevious;
        u32 kc;                 // channel key code, copied to all operators
        u32 kcIndex;            // index into the frequency table, for speed
        u32 pms;                // channel phase modulation sensitivity
        u32 ams;                // channel amplitude modulation sensitivity

        u32 amMask;             // LFO amplitude modulation enable mask
        u32 egState;            // one of EG_ATT / EG_DEC / EG_SUS / EG_REL / EG_OFF
        u8  egShiftAttack;
        u8  egSelectAttack;
        u32 totalLevel;         // total attenuation level
        s32 volume;             // current envelope attenuation level
        u8  egShiftDecay;
        u8  egSelectDecay;
        u32 sustainLevel;       // envelope switches to sustain after reaching this
        u8  egShiftSustain;
        u8  egSelectSustain;
        u8  egShiftRelease;
        u8  egSelectRelease;

        u32 keyOn;              // 0 = last key event was KEY OFF, 1 = KEY ON

        u32 keyScale;
        u32 attackRate;
        u32 decayRate;
        u32 sustainRate;
        u32 releaseRate;
    };

    void initChipTables();
    void setConnect(Operator* om1, u32 chan, u32 algorithm);
    void refreshConnections();
    void keyOn(Operator* op, u32 keySet);
    static void keyOff(Operator* op, u32 keyClear);
    void envelopeKonKoff(Operator* op, u8 v);
    static void refreshEg(Operator* op);
    static s32 opCalc(const Operator* op, u32 env, s32 pm);
    static s32 opCalc1(const Operator* op, u32 env, s32 pm);

    // Envelope level an operator contributes, plus LFO amplitude modulation.
    static u32 volumeCalc(const Operator* op, u32 am) {
        return op->totalLevel + static_cast<u32>(op->volume) + (am & op->amMask);
    }

    void chanCalc(u32 chan);
    void chan7Calc();
    void advanceEg();
    void advance();

    template <typename Visit> void visitState(Visit visit);

    Operator m_oper[32] = {};   // the 32 operators
    u32 m_pan[16] = {};         // channel output masks (0xffffffff = enabled)

    u32 m_egCnt = 0;            // global envelope generator counter
    u32 m_egTimer = 0;          // runs at chipclock/64/3
    u32 m_egTimerAdd = 0;
    u32 m_egTimerOverflow = 0;

    u32 m_lfoPhase = 0;         // accumulated LFO phase (0 to 255)
    u32 m_lfoTimer = 0;
    u32 m_lfoTimerAdd = 0;
    u32 m_lfoOverflow = 0;      // LFO steps when m_lfoTimer reaches this
    u32 m_lfoCounter = 0;
    u32 m_lfoCounterAdd = 0;
    u8  m_lfoWaveform = 0;      // 0-saw, 1-square, 2-triangle, 3-noise
    u8  m_amd = 0;              // LFO amplitude modulation depth
    s8  m_pmd = 0;              // LFO phase modulation depth
    u32 m_lfa = 0;              // LFO current AM output
    s32 m_lfp = 0;              // LFO current PM output

    u8 m_test = 0;              // TEST register
    u8 m_ct = 0;                // output control pins (bit1-CT2, bit0-CT1)

    u32 m_noise = 0;            // bit 7 enable, bits 4-0 period
    u32 m_noiseRng = 0;         // 17 bit noise shift register
    u32 m_noisePhase = 0;
    u32 m_noisePeriod = 0;

    u32 m_csmRequest = 0;       // CSM KEY ON / KEY OFF sequence request

    // Bit 3 - timer B, bit 2 - timer A, bit 7 - CSM mode (key on to all slots
    // every time timer A overflows).
    u32 m_irqEnable = 0;
    u32 m_status = 0;           // chip status (BUSY, IRQ flags)
    u8  m_connect[8] = {};      // channel connection algorithms

    bool m_timerAEnabled = false;
    bool m_timerBEnabled = false;
    double m_timerAValue = 0.0;
    double m_timerBValue = 0.0;
    double m_timerATable[1024] = {};
    double m_timerBTable[256] = {};
    u32 m_timerAIndex = 0;
    u32 m_timerBIndex = 0;

    // Frequency deltas, 11 octaves of 768 'cents'. Eleven because DT2 reaches
    // 950 cents above the base frequency and LFO phase modulation reaches 800
    // cents either side of it, so octaves -1, 8 and 9 are needed as headroom.
    u32 m_freq[11 * 768] = {};

    // Frequency deltas for DT1, applied on top of the value above.
    s32 m_dt1Freq[8 * 32] = {};

    u32 m_noiseTable[32] = {};  // 17-bit noise generator periods

    u32 m_clock = 0;
    u32 m_sampleRate = 44100;

    // Scratch shared by the operator mixing path. The operators hold pointers
    // into these, so they have to live as long as the chip does.
    s32 m_chanOut[8] = {};
    s32 m_m2 = 0, m_c1 = 0, m_c2 = 0;   // phase modulation input for op 2,3,4
    s32 m_mem = 0;                       // one sample delay memory

    IrqHandler m_irqHandler;
};

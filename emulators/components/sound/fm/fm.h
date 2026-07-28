// Software emulation of the Yamaha OPN FM sound generator.
//
// The YM2610 (OPNB, NeoGeo) and the YM2612 (OPN2, Mega Drive) share the OPN
// core; the structures below are that shared state.
#pragma once

#include <functional>

#include "../../../types.h"
#include "../../buffer.h"
#include "../ay8910/ay8910.h"
#include "ymdeltat.h"

// The chip asks the driver to start or stop one of its two timers.
// timer: 0 = Timer A, 1 = Timer B. count == 0 stops the timer, otherwise it is
// the number of counts to run. stepTime is the length of one count in seconds.
using FmTimerHandler = std::function<void(s32 timer, s32 count, double stepTime)>;

// The chip's IRQ output changed level.
using FmIrqHandler = std::function<void(bool asserted)>;

// One operator, which the datasheet calls a slot.
struct FmSlot {
    s32* detune;        // detune table entry: detuneTab[DT]
    u8   ksrShift;      // key scale rate shift: 3 - KSR
    u32  attackRate;
    u32  decayRate;
    u32  sustainRate;
    u32  releaseRate;
    u8   ksr;           // key scale rate: kcode >> ksrShift
    u32  multiple;      // multiple: ML_TABLE[ML]

    // Phase generator
    u32  phase;         // phase counter
    s32  phaseIncrement;

    // Envelope generator
    u8   egState;       // one of EG_ATT / EG_DEC / EG_SUS / EG_REL / EG_OFF
    u32  totalLevel;    // TL << 3
    s32  volume;        // envelope counter
    u32  sustainLevel;  // slTable[SL]
    u32  volumeOut;     // current EG output, before LFO amplitude modulation

    u8   egShiftAttack;
    u8   egSelectAttack;
    u8   egShiftDecay;
    u8   egSelectDecay;
    u8   egShiftSustain;
    u8   egSelectSustain;
    u8   egShiftRelease;
    u8   egSelectRelease;

    u8   ssgEg;         // SSG-EG waveform
    u8   ssgNegate;     // SSG-EG negated output

    u32  keyOn;         // 0 = last key event was KEY OFF, 1 = KEY ON

    u32  amMask;        // LFO amplitude modulation enable flag
};

struct FmChannel {
    FmSlot slot[4];     // four slots (operators)

    u8   algorithm;
    u8   feedback;      // feedback shift
    s32  op1Out[2];     // op1 output, kept for feedback

    s32* connect1;      // slot 1 output pointer
    s32* connect3;      // slot 3 output pointer
    s32* connect2;      // slot 2 output pointer
    s32* connect4;      // slot 4 output pointer

    s32* memConnect;    // where to put the delayed sample (MEM)
    s32  memValue;      // delayed sample (MEM) value

    s32  pms;           // channel phase modulation sensitivity
    u8   ams;           // channel amplitude modulation sensitivity

    u32  fc;            // fnum,blk: adjusted to sample rate
    u8   kcode;         // key code

    // Current blk/fnum. In 3-slot mode the slots of one channel can differ.
    u32  blockFnum;
};

// State shared by every OPN derivative.
struct FmState {
    s32    clock;           // master clock (Hz)
    s32    rate;            // sampling rate (Hz)
    double freqBase;        // frequency base
    double timerBase;       // timer base time
    double busyExpire;      // expiry time of the busy flag
    u8     address;         // address register
    u8     irq;             // interrupt level
    u8     irqMask;
    u8     status;          // status flag
    u32    mode;            // mode CSM / 3SLOT
    u8     prescalerSel;    // prescaler selector
    u8     fnH;             // freq latch
    s32    timerA;
    s32    timerACount;
    u8     timerB;
    s32    timerBCount;

    s32    detuneTab[8][32];    // detune table, derived from the clock

    FmTimerHandler timerHandler;
    FmIrqHandler   irqHandler;
};


// Channel 3 in 3-slot mode, where each slot gets its own frequency.
struct Fm3Slot {
    u32 fc[3];              // fnum3,blk3: calculated
    u8  fnH;                // freq3 latch
    u8  kcode[3];           // key code
    u32 blockFnum[3];       // current fnum value for each slot
};

// State common to OPN, OPNA and OPNB.
struct FmOpn {
    u8         type;        // chip type
    FmState    st;          // general state
    Fm3Slot    slot3;       // 3-slot mode state
    FmChannel* channels;    // pointer to the channel array
    u32        pan[6 * 2];  // channel output masks (0xffffffff = enabled)

    u32 egCnt;              // global envelope generator counter
    u32 egTimer;            // runs at chipclock/64/3
    u32 egTimerAdd;         // step of egTimer
    u32 egTimerOverflow;    // the EG timer overflows every 3 samples

    // FNUM/BLK can generate 2048 FNUMs, but the LFO works with one more bit of
    // precision, so the table needs 4096 entries.
    u32 fnTable[4096];      // fnumber -> increment counter
    u32 fnMax;

    u32 lfoCnt;
    u32 lfoInc;
    u32 lfoFreq[8];         // LFO frequency table

    // Scratch used while rendering a sample. It lives here rather than at file
    // scope so that the two chips cannot clobber each other's work.
    s32 m2, c1, c2;         // phase modulation input for operators 2,3,4
    s32 mem;                // one sample delay memory
    s32 outFm[8];           // outputs of working channels
    u32 lfoAm;              // runtime LFO calculations helper
    s32 lfoPm;              // runtime LFO calculations helper
};

// One ADPCM-A channel.
struct AdpcmChannel {
    u8   flag;              // port state
    u8   flagMask;          // arrived flag mask
    u8   nowData;           // current ROM data
    u32  nowAddr;           // current ROM address
    u32  nowStep;
    u32  step;
    u32  start;             // sample data start address
    u32  end;               // sample data end address
    u8   instrumentLevel;
    s32  adpcmAcc;          // accumulator
    s32  adpcmStep;
    s32  adpcmOut;
    s8   volMul;            // volume in 0.75dB steps
    u8   volShift;          // volume in -6dB steps
    u8   panRaw;            // to re-connect the pan pointer after a load
    s32* pan;               // &m_outAdpcm[OUTD_xxxx]
};


// -------------------- YM2610 (OPNB) --------------------
// FM + SSG + ADPCM-A + ADPCM-B (Delta-T). Used by the NeoGeo.
class Ym2610 {
public:
    // pcmRomA and pcmRomB are borrowed cartridge buffers and have to outlive
    // the chip.
    void init(s32 clock, s32 rate,
              u8* pcmRomA, u32 pcmSizeA, u8* pcmRomB, u32 pcmSizeB,
              FmTimerHandler timerHandler, FmIrqHandler irqHandler);
    void reset();

    // Invoked before a register write lands so that the driver can bring the
    // chip up to date with the samples it has already rendered.
    void setUpdateRequestHandler(std::function<void()> handler) {
        m_updateRequest = std::move(handler);
    }

    // Renders the FM and ADPCM sections; the SSG is rendered separately
    // because the driver mixes it at its own level.
    void update(s16* left, s16* right, s32 length);
    void updateSsg(s16* channelA, s16* channelB, s16* channelC, s32 length);

    s32 write(s32 address, u8 value);   // returns the IRQ line state
    u8 read(s32 address);
    s32 timerOver(s32 timer);           // returns the IRQ line state

    void saveState(Buffer* buf);
    void loadState(Buffer* buf);

private:
    template <typename Visit> void visitState(Visit visit);

    void postload();
    void adpcmaCalcChannel(AdpcmChannel* ch);
    void adpcmaWrite(s32 r, s32 v);

    u8 m_regs[512] = {};
    FmOpn m_opn = {};
    FmChannel m_ch[6] = {};
    u8 m_addrA1 = 0;                // address line A1

    // ADPCM-A unit
    u8* m_pcmBuf = nullptr;         // borrowed sample ROM
    u32 m_pcmSize = 0;
    u8 m_adpcmTL = 0;               // ADPCM-A total level
    AdpcmChannel m_adpcm[6] = {};
    u32 m_adpcmReg[0x30] = {};
    u8 m_adpcmArrivedEndAddress = 0;

    YmDeltaT m_deltaT;              // ADPCM-B unit
    Ay8910 m_ssg;                   // the SSG section

    std::function<void()> m_updateRequest;

    // Per-sample accumulators, indexed NONE/LEFT/RIGHT/CENTER.
    s32 m_outAdpcm[4] = {};
    s32 m_outDelta[4] = {};
};


// -------------------- YM2612 (OPN2) --------------------
// FM + DAC. Used by the Mega Drive.
class Ym2612 {
public:
    void init(s32 clock, s32 rate,
              FmTimerHandler timerHandler, FmIrqHandler irqHandler);
    void reset();

    void update(s16* left, s16* right, s32 length);

    s32 write(s32 address, u8 value);   // returns the IRQ line state
    u8 read(s32 address);
    s32 timerOver(s32 timer);           // returns the IRQ line state

    void saveState(Buffer* buf);
    void loadState(Buffer* buf);

private:
    template <typename Visit> void visitState(Visit visit);

    void postload();

    u8 m_regs[512] = {};
    FmOpn m_opn = {};
    FmChannel m_ch[6] = {};
    u8 m_addrA1 = 0;                // address line A1

    // DAC output
    s32 m_dacen = 0;
    s32 m_dacout = 0;
};

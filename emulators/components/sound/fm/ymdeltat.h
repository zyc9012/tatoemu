#pragma once

#include <functional>

#include "../../../types.h"
#include "../../buffer.h"

// Yamaha Delta-T (ADPCM-B) unit, shared by the YM2608 (OPNA), the YM2610
// (OPNB) and the Y8950. TatoEmu only drives it as the ADPCM-B section of the
// YM2610, which always plays from external ROM.
class YmDeltaT {
public:
    // The owning chip ORs the given bits into, or clears them from, its status
    // register.
    using StatusChangeHandler = std::function<void(u8 statusBits)>;

    enum class EmulationMode : u8 {
        Normal = 0,
        Ym2610 = 1,
    };

    // The sample ROM is borrowed from the cartridge and has to outlive the unit.
    void setMemory(u8* memory, u32 size) {
        m_memory = memory;
        m_memorySize = size;
    }

    void setFreqBase(double freqBase) { m_freqBase = freqBase; }

    // The unit accumulates into outputPointer[pan], where pan is chosen by
    // reset() and by register 0x01.
    void setOutput(s32* outputPointer, s32 range) {
        m_outputPointer = outputPointer;
        m_outputRange = range;
    }

    // Address bits to shift the start/end/limit registers left by: 8 for the
    // YM2610, 5 for the Y8950 and the YM2608.
    void setPortShift(u8 shift) { m_portShift = shift; }

    void setStatusHandlers(StatusChangeHandler set, StatusChangeHandler reset) {
        m_statusSetHandler = std::move(set);
        m_statusResetHandler = std::move(reset);
    }

    // Different chips carry these flags on different bits of the status
    // register, so the owner supplies the masks.
    void setEosBit(u8 bit) { m_statusChangeEosBit = bit; }
    void setBrdyBit(u8 bit) { m_statusChangeBrdyBit = bit; }
    void setZeroBit(u8 bit) { m_statusChangeZeroBit = bit; }

    // Set while the owning chip replays its registers after a state load, so
    // that the replay does not raise spurious status flags.
    void setPostloading(bool postloading) { m_postloading = postloading; }

    u8 portState() const { return m_portState; }

    void reset(s32 pan, EmulationMode mode);

    u8 read();
    void write(s32 reg, s32 value);
    void calc();

    // Defined here rather than in the .cpp because the owning chip folds this
    // walk into its own.
    template <typename Visit> void visitState(Visit visit) {
        // hooked up proper deltaT states -dink july31, 2021
        // hint: mechatt now works with run-ahead! :)
        visit(m_nowData);
        visit(m_cpuData);
        visit(m_portState);
        visit(m_control2);
        visit(m_portShift);
        visit(m_dramPortShift);
        visit(m_memRead);
        visit(m_pcmBusy);
        visit(m_reg);

        visit(m_nowAddr);
        visit(m_nowStep);
        visit(m_step);
        visit(m_start);
        visit(m_limit);
        visit(m_end);
        visit(m_delta);

        visit(m_volume);
        visit(m_acc);
        visit(m_prevAcc);
        visit(m_adpcmD);
        visit(m_adpcmL);
    }

private:
    static constexpr s32 SHIFT = 16;

    void synthesisFromExternalMemory();
    void synthesisFromCpuMemory();

    u8*  m_memory = nullptr;
    s32* m_outputPointer = nullptr;
    s32* m_pan = nullptr;           // &m_outputPointer[pan]
    double m_freqBase = 0.0;
    u32  m_memorySize = 0;
    s32  m_outputRange = 0;

    u32  m_nowAddr = 0;             // current address
    u32  m_nowStep = 0;             // current step
    u32  m_step = 0;
    u32  m_start = 0;               // start address
    u32  m_limit = 0;               // limit address
    u32  m_end = 0;                 // end address
    u32  m_delta = 0;               // delta scale
    s32  m_volume = 0;
    s32  m_acc = 0;                 // shift measurement value
    s32  m_adpcmD = 0;              // next forecast
    s32  m_adpcmL = 0;              // current value
    s32  m_prevAcc = 0;             // leveling value

    u8   m_nowData = 0;             // current ROM data
    u8   m_cpuData = 0;             // current data from register 0x08
    u8   m_portState = 0;
    u8   m_control2 = 0;            // SAMPLE, DA/AD, RAM TYPE (x8bit / x1bit), ROM/RAM
    u8   m_portShift = 0;

    // Address bits to shift right: 0 for ROM and x8bit DRAMs, 3 for x1 DRAMs.
    u8   m_dramPortShift = 0;

    u8   m_memRead = 0;             // needed for reading/writing external memory

    StatusChangeHandler m_statusSetHandler;
    StatusChangeHandler m_statusResetHandler;

    bool m_postloading = false;

    u8   m_statusChangeEosBit = 0;  // set on end of sample
    u8   m_statusChangeBrdyBit = 0; // set after reading or writing one datum
    u8   m_statusChangeZeroBit = 0; // set when a recording stays silent for 290ms

    // Neither the Y8950 nor the YM2608 can raise an IRQ when PCMBSY changes,
    // so instead their status register reads get ORed with this flag.
    u8   m_pcmBusy = 0;             // 1 while ADPCM is playing; Y8950/YM2608 only

    u8   m_reg[16] = {};            // ADPCM registers
    EmulationMode m_emulationMode = EmulationMode::Normal;
};

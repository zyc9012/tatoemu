// arm7tdmi.h — Modern C++ ARM7TDMI CPU Emulator
// Based on Steve Ellenoff's ARM7 core, rewritten for clarity and performance.
//
// The ARM7TDMI is a 32-bit RISC processor with two instruction sets:
//   - ARM: 32-bit instructions, full feature set
//   - Thumb: 16-bit compressed instructions, subset of ARM functionality
// It supports 7 processor modes, each with banked registers.
#pragma once

#include <cstdint>
#include <cstdlib>
#include <cstring>

// ============================================================================
// Type aliases
// ============================================================================
using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using s8  = int8_t;
using s16 = int16_t;
using s32 = int32_t;
using s64 = int64_t;

// ============================================================================
// CPU Mode — bottom 4 bits of CPSR (bit 4 is always 1 for valid modes)
// ============================================================================
enum class Mode : u32 {
    User   = 0x0,   // Normal execution
    FIQ    = 0x1,   // Fast interrupt
    IRQ    = 0x2,   // Normal interrupt
    SVC    = 0x3,   // Supervisor (SWI)
    ABT    = 0x7,   // Abort (memory fault)
    UND    = 0xB,   // Undefined instruction
    System = 0xF,   // Privileged user mode
};

static constexpr u32 kNumModes = 16;

// ============================================================================
// Physical register indices — 37 unique 32-bit registers
// ============================================================================
enum Reg : int {
    // Shared across most modes
    R0, R1, R2, R3, R4, R5, R6, R7,
    R8, R9, R10, R11, R12,
    R13,    // Stack Pointer (SP)
    R14,    // Link Register (LR)
    R15,    // Program Counter (PC)
    CPSR,   // Current Program Status Register

    // FIQ-banked (R8–R14 + SPSR)
    R8_FIQ, R9_FIQ, R10_FIQ, R11_FIQ, R12_FIQ, R13_FIQ, R14_FIQ, SPSR_FIQ,
    // IRQ-banked (R13–R14 + SPSR)
    R13_IRQ, R14_IRQ, SPSR_IRQ,
    // SVC-banked
    R13_SVC, R14_SVC, SPSR_SVC,
    // ABT-banked
    R13_ABT, R14_ABT, SPSR_ABT,
    // UND-banked
    R13_UND, R14_UND, SPSR_UND,

    kRegCount
};

// ============================================================================
// CPSR flag bits
// ============================================================================
namespace Flag {
    constexpr int N_bit = 31;   // Negative
    constexpr int Z_bit = 30;   // Zero
    constexpr int C_bit = 29;   // Carry
    constexpr int V_bit = 28;   // Overflow
    constexpr int I_bit = 7;    // IRQ disable
    constexpr int F_bit = 6;    // FIQ disable
    constexpr int T_bit = 5;    // Thumb state

    constexpr u32 N = 1u << N_bit;
    constexpr u32 Z = 1u << Z_bit;
    constexpr u32 C = 1u << C_bit;
    constexpr u32 V = 1u << V_bit;
    constexpr u32 I = 1u << I_bit;
    constexpr u32 F = 1u << F_bit;
    constexpr u32 T = 1u << T_bit;
}

// ============================================================================
// IRQ line identifiers
// ============================================================================
enum IRQLine {
    IRQ_IRQ = 0,
    IRQ_FIQ,
    IRQ_ABORT_DATA,
    IRQ_ABORT_PREFETCH,
    IRQ_UNDEFINED,
};

// ============================================================================
// Memory interface — the host system must provide these callbacks
// ============================================================================
struct MemoryInterface {
    // Data reads
    u8  (*read8)(u32 addr)  = nullptr;
    u16 (*read16)(u32 addr) = nullptr;
    u32 (*read32)(u32 addr) = nullptr;
    // Data writes
    void (*write8)(u32 addr, u8 data)   = nullptr;
    void (*write16)(u32 addr, u16 data) = nullptr;
    void (*write32)(u32 addr, u32 data) = nullptr;
    // Instruction fetches (separate path for potential HLE interception)
    u16 (*fetch16)(u32 addr) = nullptr;
    u32 (*fetch32)(u32 addr) = nullptr;
    int (*consumeWaitCycles)() = nullptr;
};

// ============================================================================
// Coprocessor interface (CP15 etc.)
// ============================================================================
struct CoprocessorInterface {
    void (*dataOp)(unsigned int, unsigned int) = nullptr;
    unsigned int (*regRead)(unsigned int) = nullptr;
    void (*regWrite)(unsigned int, unsigned int) = nullptr;
    void (*dataTransferRead)(u32 insn, u32* prn, u32 (*read32)(u32)) = nullptr;
    void (*dataTransferWrite)(u32 insn, u32* prn, void (*write32)(u32, u32)) = nullptr;
};

// ============================================================================
// ARM7TDMI CPU class
// ============================================================================
class ARM7TDMI {
public:
    ARM7TDMI() = default;

    // --- Lifecycle ---
    void reset();
    void setMemory(const MemoryInterface& mem) { m_mem = mem; }
    void setCoprocessor(const CoprocessorInterface& cop) { m_cop = cop; }

    // --- Execution ---
    int execute(int cycles);
    int idle(int cycles) { m_totalCycles += cycles; return cycles; }
    int totalCycles() const { return m_totalCycles; }

    // --- Interrupts ---
    void setIRQLine(int line, int state);

    // --- Register access (for HLE, save states, debugging) ---
    u32  getReg(int index) const;
    void setReg(int index, u32 value);
    u32  getCPSR() const { return m_regs[CPSR]; }
    void setCPSR(u32 v)  { m_regs[CPSR] = v; }
    u32  getSPSR() const;

    // --- State serialization ---
    struct State {
        u32 regs[kRegCount];
        u8  pendingIrq, pendingFiq, pendingAbtD, pendingAbtP, pendingUnd, pendingSwi;
        int totalCycles;
    };

    void getContext(void* dst) const;
    void setContext(const void* src);
    static constexpr size_t contextSize() { return sizeof(State); }

private:
    // --- Register bank mapping ---
    // Maps (mode, logical_index) → physical register index
    static const int s_regBank[kNumModes][18];

    // --- Internal helpers ---
    int  modeIndex() const { return static_cast<int>(m_regs[CPSR] & 0xF); }
    void switchMode(Mode m) { m_regs[CPSR] = (m_regs[CPSR] & ~0xFu) | static_cast<u32>(m); }

    u32  reg(int logicalIdx) const { return m_regs[s_regBank[modeIndex()][logicalIdx]]; }
    void setRegBanked(int logicalIdx, u32 v) { m_regs[s_regBank[modeIndex()][logicalIdx]] = v; }

    // Convenience: PC and CPSR
    u32& pc()   { return m_regs[R15]; }
    u32& cpsr() { return m_regs[CPSR]; }
    u32  spsr() const { return m_regs[s_regBank[modeIndex()][17]]; }
    void setSPSR(u32 v) { m_regs[s_regBank[modeIndex()][17]] = v; }

    // Flag helpers
    bool flagN() const { return m_regs[CPSR] & Flag::N; }
    bool flagZ() const { return m_regs[CPSR] & Flag::Z; }
    bool flagC() const { return m_regs[CPSR] & Flag::C; }
    bool flagV() const { return m_regs[CPSR] & Flag::V; }
    bool flagT() const { return m_regs[CPSR] & Flag::T; }
    bool flagI() const { return m_regs[CPSR] & Flag::I; }
    bool flagF() const { return m_regs[CPSR] & Flag::F; }

    // --- Memory access with alignment handling ---
    u8  read8(u32 addr);
    u16 read16(u32 addr);
    u32 read32(u32 addr);
    void write8(u32 addr, u8 data);
    void write16(u32 addr, u16 data);
    void write32(u32 addr, u32 data);
    u32 fetchARM(u32 addr);
    u16 fetchThumb(u32 addr);

    // --- Barrel shifter ---
    u32 decodeShift(u32 insn, u32* carry);

    // --- ALU flag computation ---
    u32  aluNZ(u32 result);
    u64  aluNZ64(u64 result);
    void setAddFlags(u32 rd, u32 rn, u32 op2, bool updatePC4);
    void setSubFlags(u32 rd, u32 rn, u32 op2, bool updatePC4);
    void setLogicFlags(u32 rd, u32 carry, bool updatePC4);

    // --- ARM instruction handlers ---
    void armBranch(u32 insn);
    void armALU(u32 insn);
    void armMul(u32 insn);
    void armMulLong(u32 insn, bool isSigned);
    void armSingleTransfer(u32 insn);
    void armHalfwordTransfer(u32 insn);
    void armBlockTransfer(u32 insn);
    void armSwap(u32 insn);
    void armPSRTransfer(u32 insn);
    void armCoprocDataOp(u32 insn);
    void armCoprocRegTransfer(u32 insn);
    void armCoprocDataTransfer(u32 insn);

    // Block transfer helpers
    int loadIncrement(u32 pat, u32 base, u32 sFlag);
    int loadDecrement(u32 pat, u32 base, u32 sFlag);
    int storeIncrement(u32 pat, u32 base);
    int storeDecrement(u32 pat, u32 base);

    // --- Thumb instruction handlers ---
    void thumbExecute(u32 insn);
    void thumbExecuteHigh(u32 insn); // opcodes 0x6–0xB
    void thumbExecuteHighest(u32 insn); // opcodes 0xC–0xF
    void thumbHiRegBX(u32 insn);     // Hi-register ops + BX

    // --- Exception handling ---
    void checkIRQState();
    bool evalCondition(u32 cond);

    // --- State ---
    u32 m_regs[kRegCount] = {};

    // Pending exception flags
    u8 m_pendingIrq  = 0;
    u8 m_pendingFiq  = 0;
    u8 m_pendingAbtD = 0;
    u8 m_pendingAbtP = 0;
    u8 m_pendingUnd  = 0;
    u8 m_pendingSwi  = 0;

    // Cycle accounting
    int m_totalCycles = 0;
    int m_cycles = 0;  // per-instruction cycle count

    // Interfaces
    MemoryInterface m_mem = {};
    CoprocessorInterface m_cop = {};
};

#include "arm7tdmi.h"

// ============================================================================
// Register bank mapping table
// Maps (mode_index, logical_register) → physical register index.
// Each mode sees 16 GPRs + CPSR, and privileged modes also have an SPSR (slot 17).
// ============================================================================
const int ARM7TDMI::s_regBank[kNumModes][18] = {
    // 0: User
    { R0,R1,R2,R3,R4,R5,R6,R7, R8,R9,R10,R11,R12, R13,R14, R15,CPSR },
    // 1: FIQ — R8–R14 are banked
    { R0,R1,R2,R3,R4,R5,R6,R7, R8_FIQ,R9_FIQ,R10_FIQ,R11_FIQ,R12_FIQ, R13_FIQ,R14_FIQ, R15,CPSR,SPSR_FIQ },
    // 2: IRQ — R13,R14 banked
    { R0,R1,R2,R3,R4,R5,R6,R7, R8,R9,R10,R11,R12, R13_IRQ,R14_IRQ, R15,CPSR,SPSR_IRQ },
    // 3: SVC
    { R0,R1,R2,R3,R4,R5,R6,R7, R8,R9,R10,R11,R12, R13_SVC,R14_SVC, R15,CPSR,SPSR_SVC },
    // 4–6: Invalid modes (zeroed)
    {0},{0},{0},
    // 7: ABT
    { R0,R1,R2,R3,R4,R5,R6,R7, R8,R9,R10,R11,R12, R13_ABT,R14_ABT, R15,CPSR,SPSR_ABT },
    // 8–10: Invalid
    {0},{0},{0},
    // 11 (0xB): UND
    { R0,R1,R2,R3,R4,R5,R6,R7, R8,R9,R10,R11,R12, R13_UND,R14_UND, R15,CPSR,SPSR_UND },
    // 12–14: Invalid
    {0},{0},{0},
    // 15 (0xF): System — same as User (no SPSR)
    { R0,R1,R2,R3,R4,R5,R6,R7, R8,R9,R10,R11,R12, R13,R14, R15,CPSR },
};

// ============================================================================
// Reset — start in SVC mode with IRQ/FIQ disabled, PC at 0
// ============================================================================
void ARM7TDMI::reset() {
    std::memset(m_regs, 0, sizeof(m_regs));
    std::memset(&m_pendingIrq, 0, 6); // zero all 6 pending flags
    switchMode(Mode::SVC);
    m_regs[CPSR] |= Flag::I | Flag::F | 0x10; // bit4 always set for valid modes
    pc() = 0;
    m_totalCycles = 0;
}

// ============================================================================
// State serialization
// ============================================================================
ARM7TDMI::State ARM7TDMI::saveState() const {
    State st;
    std::memcpy(st.regs, m_regs, sizeof(m_regs));
    st.pendingIrq  = m_pendingIrq;  st.pendingFiq  = m_pendingFiq;
    st.pendingAbtD = m_pendingAbtD; st.pendingAbtP = m_pendingAbtP;
    st.pendingUnd  = m_pendingUnd;  st.pendingSwi  = m_pendingSwi;
    st.totalCycles = m_totalCycles;
    return st;
}

void ARM7TDMI::loadState(const State& st) {
    std::memcpy(m_regs, st.regs, sizeof(m_regs));
    m_pendingIrq  = st.pendingIrq;  m_pendingFiq  = st.pendingFiq;
    m_pendingAbtD = st.pendingAbtD; m_pendingAbtP = st.pendingAbtP;
    m_pendingUnd  = st.pendingUnd;  m_pendingSwi  = st.pendingSwi;
    m_totalCycles = st.totalCycles;
}

// ============================================================================
// Register access — goes through the bank table for correct mode
// ============================================================================
u32 ARM7TDMI::getReg(int index) const {
    int mode = m_regs[CPSR] & 0xF;
    if (index == 16) return m_regs[CPSR];
    if (index == 17) return m_regs[s_regBank[mode][17]];
    return m_regs[s_regBank[mode][index]];
}

void ARM7TDMI::setReg(int index, u32 value) {
    int mode = m_regs[CPSR] & 0xF;
    if (index == 16) { m_regs[CPSR] = value; return; }
    if (index == 17) { m_regs[s_regBank[mode][17]] = value; return; }
    m_regs[s_regBank[mode][index]] = value;
}

u32 ARM7TDMI::getSPSR() const {
    int mode = m_regs[CPSR] & 0xF;
    return m_regs[s_regBank[mode][17]];
}

// ============================================================================
// Memory access — handles alignment per ARM7TDMI spec
// ============================================================================

// Read 8-bit—no alignment needed.
u8 ARM7TDMI::read8(u32 addr) {
    return m_mem.read8(addr);
}

// Read 16-bit—mask to halfword boundary, rotate if unaligned.
u16 ARM7TDMI::read16(u32 addr) {
    u16 val = m_mem.read16(addr & ~1u);
    if (addr & 1) val = (val >> 8) | (val << 8); // byte-swap on misalignment
    return val;
}

// Read 32-bit—mask to word boundary, rotate if unaligned.
// ARM7TDMI rotates the word right by 8*misalignment bits.
u32 ARM7TDMI::read32(u32 addr) {
    if (addr & 3) {
        u32 val = m_mem.read32(addr & ~3u);
        int shift = (addr & 3) * 8;
        return (val >> shift) | (val << (32 - shift));
    }
    return m_mem.read32(addr);
}

void ARM7TDMI::write8(u32 addr, u8 data)   { m_mem.write8(addr, data); }
void ARM7TDMI::write16(u32 addr, u16 data) { m_mem.write16(addr & ~1u, data); }
void ARM7TDMI::write32(u32 addr, u32 data) { m_mem.write32(addr & ~3u, data); }

// Instruction fetches — same rotation rules
u32 ARM7TDMI::fetchARM(u32 addr) {
    if (addr & 3) {
        u32 val = m_mem.fetch32(addr & ~3u);
        int shift = (addr & 3) * 8;
        return (val >> shift) | (val << (32 - shift));
    }
    return m_mem.fetch32(addr);
}

u16 ARM7TDMI::fetchThumb(u32 addr) {
    u16 val = m_mem.fetch16(addr & ~1u);
    if (addr & 1) val = (val >> 8) | (val << 8);
    return val;
}

// ============================================================================
// ALU flag helpers
// ============================================================================

// Compute N|Z flags from a 32-bit result
inline u32 ARM7TDMI::aluNZ(u32 result) {
    return (result & (1u << 31))           // N = sign bit
         | (result == 0 ? Flag::Z : 0);   // Z = zero
}

// Compute N|Z flags from a 64-bit result (for long multiply)
inline u64 ARM7TDMI::aluNZ64(u64 result) {
    return ((result >> 32) & (1u << 31))   // N = bit 63 shifted to bit 31
         | (result == 0 ? Flag::Z : 0);
}

// Set NZCV after an ADD-type operation
void ARM7TDMI::setAddFlags(u32 rd, u32 rn, u32 op2, bool updatePC4) {
    bool signsDiffer = ((rn ^ rd) >> 31) & 1;
    bool sameInput   = !((rn ^ op2) >> 31);
    u32 flags = aluNZ(rd)
              | ((sameInput && signsDiffer) ? Flag::V : 0)
              | ((~rn < op2) ? Flag::C : 0);
    cpsr() = (cpsr() & ~(Flag::N | Flag::Z | Flag::V | Flag::C)) | flags;
    if (updatePC4) pc() += 4;
}

// Set NZCV after a SUB-type operation
void ARM7TDMI::setSubFlags(u32 rd, u32 rn, u32 op2, bool updatePC4) {
    bool signsDiffer_input = ((rn ^ op2) >> 31) & 1;
    bool signsDiffer_result = ((rn ^ rd) >> 31) & 1;
    // Carry for subtraction: borrow-free if rn >= op2 (unsigned)
    bool isNeg_rn = rn >> 31;
    bool isPos_op2 = ~op2 >> 31;
    bool isPos_rd = ~rd >> 31;
    u32 carry = ((isNeg_rn && isPos_op2) || (isNeg_rn && isPos_rd) || (isPos_op2 && isPos_rd))
              ? Flag::C : 0;
    u32 flags = aluNZ(rd)
              | ((signsDiffer_input && signsDiffer_result) ? Flag::V : 0)
              | carry;
    cpsr() = (cpsr() & ~(Flag::N | Flag::Z | Flag::V | Flag::C)) | flags;
    if (updatePC4) pc() += 4;
}

// Set NZC after a logical operation (carry from shifter)
void ARM7TDMI::setLogicFlags(u32 rd, u32 carry, bool updatePC4) {
    u32 flags = aluNZ(rd) | (carry ? Flag::C : 0);
    cpsr() = (cpsr() & ~(Flag::N | Flag::Z | Flag::C)) | flags;
    if (updatePC4) pc() += 4;
}

// ============================================================================
// Multiplier cycle count helpers
// ARM7TDMI multiply takes 1-4 internal cycles depending on the multiplier value.
// Signed: checks both all-0 and all-1 patterns in upper bytes.
// Unsigned: checks only all-0 patterns.
// ============================================================================
static int mulCyclesSigned(u32 rs) {
    if ((rs & 0xFFFFFF00) == 0xFFFFFF00 || !(rs & 0xFFFFFF00)) return 1;
    if ((rs & 0xFFFF0000) == 0xFFFF0000 || !(rs & 0xFFFF0000)) return 2;
    if ((rs & 0xFF000000) == 0xFF000000 || !(rs & 0xFF000000)) return 3;
    return 4;
}

static int mulCyclesUnsigned(u32 rs) {
    if (!(rs & 0xFFFFFF00)) return 1;
    if (!(rs & 0xFFFF0000)) return 2;
    if (!(rs & 0xFF000000)) return 3;
    return 4;
}


// ============================================================================
// Barrel Shifter — Decodes operand 2 shift for ARM data processing
//
// The ARM barrel shifter supports 4 shift types (LSL, LSR, ASR, ROR/RRX)
// with either an immediate or register-specified shift amount.
// Returns the shifted value and optionally sets *carry to the shifter carry-out.
// ============================================================================
u32 ARM7TDMI::decodeShift(u32 insn, u32* carry) {
    u32 rm = reg(insn & 0xF);
    u32 shiftType = (insn >> 5) & 3;   // bits[6:5]: 0=LSL, 1=LSR, 2=ASR, 3=ROR
    u32 amount;
    bool byReg = insn & 0x10;          // bit 4: shift amount from register?

    // If Rm is PC, add 8 for pipeline offset
    if ((insn & 0xF) == 15) rm += 8;

    if (byReg) {
        // Register-specified shift: use bottom 8 bits
        amount = reg((insn >> 8) & 0xF) & 0xFF;
        if (amount == 0) {
            if (carry) *carry = cpsr() & Flag::C;
            return rm;
        }
    } else {
        amount = (insn >> 7) & 0x1F; // bits[11:7]: 5-bit immediate
    }

    switch (shiftType) {
    case 0: // LSL (Logical Shift Left)
        if (amount >= 32) {
            if (carry) *carry = (amount == 32) ? (rm & 1) : 0;
            return 0;
        }
        if (amount == 0) {
            if (carry) *carry = cpsr() & Flag::C;
            return rm;
        }
        if (carry) *carry = rm & (1u << (32 - amount));
        return rm << amount;

    case 1: // LSR (Logical Shift Right)
        if (amount == 0 || amount == 32) {
            if (carry) *carry = rm & (1u << 31);
            return 0;
        }
        if (amount > 32) {
            if (carry) *carry = 0;
            return 0;
        }
        if (carry) *carry = rm & (1u << (amount - 1));
        return rm >> amount;

    case 2: // ASR (Arithmetic Shift Right) — preserves sign
        if (amount == 0 || amount > 32) amount = 32;
        if (carry) *carry = rm & (1u << (amount - 1));
        if (amount >= 32)
            return (rm & (1u << 31)) ? 0xFFFFFFFFu : 0;
        if (rm & (1u << 31))
            return (rm >> amount) | (0xFFFFFFFFu << (32 - amount));
        return rm >> amount;

    case 3: // ROR (Rotate Right) / RRX (Rotate Right through Carry)
        if (amount == 0) {
            // RRX: 33-bit rotate through carry
            if (carry) *carry = rm & 1;
            return (rm >> 1) | ((cpsr() & Flag::C) << 2);
        }
        // Normalize rotation to 1–32 range
        while (amount > 32) amount -= 32;
        if (amount == 32) {
            // ROR by 32: value unchanged, carry = bit 31
            if (carry) *carry = rm & (1u << 31);
            return rm;
        }
        if (carry) *carry = rm & (1u << (amount - 1));
        return (rm >> amount) | (rm << (32 - amount));
    }
    return 0;
}

// ============================================================================
// Condition evaluation — ARM instructions are conditionally executed
// based on the top 4 bits (condition field).
// ============================================================================
bool ARM7TDMI::evalCondition(u32 cond) {
    switch (cond) {
    case 0x0: return  flagZ();                          // EQ: Z set
    case 0x1: return !flagZ();                          // NE: Z clear
    case 0x2: return  flagC();                          // CS/HS: C set
    case 0x3: return !flagC();                          // CC/LO: C clear
    case 0x4: return  flagN();                          // MI: N set
    case 0x5: return !flagN();                          // PL: N clear
    case 0x6: return  flagV();                          // VS: V set
    case 0x7: return !flagV();                          // VC: V clear
    case 0x8: return  flagC() && !flagZ();              // HI: C set AND Z clear
    case 0x9: return !flagC() ||  flagZ();              // LS: C clear OR Z set
    case 0xA: return  flagN() == flagV();               // GE: N == V
    case 0xB: return  flagN() != flagV();               // LT: N != V
    case 0xC: return !flagZ() && (flagN() == flagV());  // GT: Z clear AND N==V
    case 0xD: return  flagZ() || (flagN() != flagV());  // LE: Z set OR N!=V
    case 0xE: return true;                              // AL: always
    case 0xF: return false;                             // NV: never (ARMv4)
    }
    return false;
}

// ============================================================================
// Exception handling — checks pending exceptions in priority order
//
// Exception priorities (highest to lowest):
//   Reset > Data Abort > FIQ > IRQ > Prefetch Abort > Undefined > SWI
// ============================================================================
void ARM7TDMI::checkIRQState() {
    u32 savedCPSR = cpsr();
    u32 savedPC = pc() + 4; // pipeline offset

    // Data Abort
    if (m_pendingAbtD) {
        switchMode(Mode::ABT);
        setRegBanked(14, savedPC);
        setSPSR(savedCPSR);
        cpsr() |= Flag::I;
        cpsr() &= ~Flag::T;
        pc() = 0x10;
        m_pendingAbtD = 0;
        return;
    }

    // FIQ — only if F bit is clear
    if (m_pendingFiq && !(savedCPSR & Flag::F)) {
        switchMode(Mode::FIQ);
        setRegBanked(14, savedPC);
        setSPSR(savedCPSR);
        cpsr() |= Flag::I | Flag::F;
        cpsr() &= ~Flag::T;
        pc() = 0x1C;
        return;
    }

    // IRQ — only if I bit is clear
    if (m_pendingIrq && !(savedCPSR & Flag::I)) {
        switchMode(Mode::IRQ);
        setRegBanked(14, savedPC);
        setSPSR(savedCPSR);
        cpsr() |= Flag::I;
        cpsr() &= ~Flag::T;
        pc() = 0x18;
        return;
    }

    // Prefetch Abort
    if (m_pendingAbtP) {
        switchMode(Mode::ABT);
        setRegBanked(14, savedPC);
        setSPSR(savedCPSR);
        cpsr() |= Flag::I;
        cpsr() &= ~Flag::T;
        pc() = 0x0C;
        m_pendingAbtP = 0;
        return;
    }

    // Undefined Instruction
    if (m_pendingUnd) {
        switchMode(Mode::UND);
        setRegBanked(14, savedPC);
        setSPSR(savedCPSR);
        cpsr() |= Flag::I;
        cpsr() &= ~Flag::T;
        pc() = 0x04;
        m_pendingUnd = 0;
        return;
    }

    // Software Interrupt (SWI)
    if (m_pendingSwi) {
        switchMode(Mode::SVC);
        // Compensate for Thumb prefetch
        if (savedCPSR & Flag::T)
            setRegBanked(14, savedPC - 2);
        else
            setRegBanked(14, savedPC);
        setSPSR(savedCPSR);
        cpsr() |= Flag::I;
        cpsr() &= ~Flag::T;
        pc() = 0x08;
        m_pendingSwi = 0;
        return;
    }
}

// ============================================================================
// Set IRQ line — mirrors the original interface
// ============================================================================
void ARM7TDMI::setIRQLine(int line, int state) {
    switch (line) {
    case IRQ_IRQ:            m_pendingIrq  = state & 1; break;
    case IRQ_FIQ:            m_pendingFiq  = state & 1; break;
    case IRQ_ABORT_DATA:     m_pendingAbtD = state & 1; break;
    case IRQ_ABORT_PREFETCH: m_pendingAbtP = state & 1; break;
    case IRQ_UNDEFINED:      m_pendingUnd  = state & 1; break;
    }
    checkIRQState();
}



// ============================================================================
// ARM Branch (B / BL)
// Encoding: cond 101L offset24
// L=1: Branch with Link (saves return address in R14)
// The offset is sign-extended and shifted left 2.
// ============================================================================
void ARM7TDMI::armBranch(u32 insn) {
    u32 offset = (insn & 0x00FFFFFFu) << 2;

    // Save return address if Branch with Link
    if (insn & (1u << 24))
        setRegBanked(14, pc() + 4);

    // Sign-extend the 26-bit offset
    if (offset & 0x02000000u)
        pc() -= ((~(offset | 0xFC000000u)) + 1) - 8;
    else
        pc() += offset + 8;
}

// ============================================================================
// ARM ALU — Data Processing instructions
// Encoding: cond 00I opcode S Rn Rd operand2
// 16 opcodes: AND, EOR, SUB, RSB, ADD, ADC, SBC, RSC,
//             TST, TEQ, CMP, CMN, ORR, MOV, BIC, MVN
// ============================================================================
void ARM7TDMI::armALU(u32 insn) {
    u32 opcode = (insn >> 21) & 0xF;
    bool sFlag = insn & (1u << 20);
    u32 op2, sc = 0;

    // Decode operand 2
    if (insn & (1u << 25)) {
        // Immediate: 8-bit value rotated right by 2*rot
        u32 imm = insn & 0xFF;
        u32 rot = ((insn >> 8) & 0xF) << 1;
        if (rot) {
            op2 = (imm >> rot) | (imm << (32 - rot));
            sc = op2 & (1u << 31);
        } else {
            op2 = imm;
            sc = cpsr() & Flag::C;
        }
    } else {
        op2 = decodeShift(insn, sFlag ? &sc : nullptr);
        if (!sFlag) sc = 0;
    }

    // Read Rn (source register), account for PC pipeline
    u32 rn = 0;
    if ((opcode & 0xD) != 0xD) { // not MOV/MVN which don't use Rn
        u32 rnIdx = (insn >> 16) & 0xF;
        rn = (rnIdx == 15) ? pc() + 8 : reg(rnIdx);
    }

    // Execute the ALU operation
    u32 rd = 0;
    switch (opcode) {
    // --- Arithmetic ---
    case 0x0: // AND
    case 0x8: // TST (same as AND but result discarded)
        rd = rn & op2;
        if (sFlag) setLogicFlags(rd, sc, true); else pc() += 4;
        break;
    case 0x1: // EOR
    case 0x9: // TEQ
        rd = rn ^ op2;
        if (sFlag) setLogicFlags(rd, sc, true); else pc() += 4;
        break;
    case 0x2: // SUB
    case 0xA: // CMP
        rd = rn - op2;
        if (sFlag) setSubFlags(rd, rn, op2, true); else pc() += 4;
        break;
    case 0x3: // RSB (Reverse Subtract)
        rd = op2 - rn;
        if (sFlag) setSubFlags(rd, op2, rn, true); else pc() += 4;
        break;
    case 0x4: // ADD
    case 0xB: // CMN
        rd = rn + op2;
        if (sFlag) setAddFlags(rd, rn, op2, true); else pc() += 4;
        break;
    case 0x5: // ADC (Add with Carry)
        rd = rn + op2 + ((cpsr() & Flag::C) >> Flag::C_bit);
        if (sFlag) setAddFlags(rd, rn, op2, true); else pc() += 4;
        break;
    case 0x6: // SBC (Subtract with Carry)
        rd = rn - op2 - ((cpsr() & Flag::C) ? 0 : 1);
        if (sFlag) setSubFlags(rd, rn, op2, true); else pc() += 4;
        break;
    case 0x7: // RSC (Reverse Subtract with Carry)
        rd = op2 - rn - ((cpsr() & Flag::C) ? 0 : 1);
        if (sFlag) setSubFlags(rd, op2, rn, true); else pc() += 4;
        break;
    case 0xC: // ORR
        rd = rn | op2;
        if (sFlag) setLogicFlags(rd, sc, true); else pc() += 4;
        break;
    case 0xD: // MOV
        rd = op2;
        if (sFlag) setLogicFlags(rd, sc, true); else pc() += 4;
        break;
    case 0xE: // BIC (Bit Clear = AND NOT)
        rd = rn & ~op2;
        if (sFlag) setLogicFlags(rd, sc, true); else pc() += 4;
        break;
    case 0xF: // MVN (Move NOT)
        rd = ~op2;
        if (sFlag) setLogicFlags(rd, sc, true); else pc() += 4;
        break;
    }

    // Write result unless it's a test-only opcode (TST, TEQ, CMP, CMN)
    u32 rdIdx = (insn >> 12) & 0xF;
    if ((opcode & 0xC) != 0x8) {
        if (rdIdx == 15 && !sFlag) {
            pc() = rd; // Write to PC, no flag update
        } else if (rdIdx == 15 && sFlag) {
            // Rd=R15 with S: restore CPSR from SPSR, then write PC
            cpsr() = spsr();
            switchMode(static_cast<Mode>(cpsr() & 0xF));
            pc() = rd;
        } else {
            setRegBanked(rdIdx, rd);
        }
    } else if (rdIdx == 15 && sFlag) {
        // TST/TEQ/CMP/CMN with Rd=R15 and S: restore CPSR from SPSR (like all S+PC cases)
        cpsr() = spsr();
        switchMode(static_cast<Mode>(cpsr() & 0xF));
    }

    // --- Cycle count ---
    // Normal ALU: 1S. Writes PC: 2S+1N = 3. Register shift: +1I.
    bool writesPC = false;
    if ((opcode & 0xC) != 0x8) {
        writesPC = (rdIdx == 15);
    } else {
        writesPC = (rdIdx == 15 && sFlag);
    }
    m_cycles = writesPC ? 3 : 1;
    if (!(insn & (1u << 25)) && (insn & (1u << 4))) {
        m_cycles += 1; // register-specified shift: +1I
    }
}

// ============================================================================
// ARM Multiply — MUL / MLA
// Encoding: cond 000000AS Rd Rn Rs 1001 Rm
// ============================================================================
void ARM7TDMI::armMul(u32 insn) {
    u32 rm = reg(insn & 0xF);
    u32 rs = reg((insn >> 8) & 0xF);
    u32 result = rm * rs;

    // MLA: accumulate Rn
    bool isAccumulate = insn & (1u << 21);
    if (isAccumulate)
        result += reg((insn >> 12) & 0xF);

    // Write to Rd
    setRegBanked((insn >> 16) & 0xF, result);

    // Update N,Z if S bit set
    if (insn & (1u << 20))
        cpsr() = (cpsr() & ~(Flag::N | Flag::Z)) | aluNZ(result);

    // MUL = 1S + mI, MLA = 1S + (m+1)I
    int m = mulCyclesSigned(rs);
    int extra = isAccumulate ? 1 : 0;
    m_cycles = 1 + m + extra;
}

// ============================================================================
// ARM Long Multiply — UMULL/UMLAL/SMULL/SMLAL
// Encoding: cond 00001UAS RdHi RdLo Rs 1001 Rm
// ============================================================================
void ARM7TDMI::armMulLong(u32 insn, bool isSigned) {
    u32 rdHi = (insn >> 16) & 0xF;
    u32 rdLo = (insn >> 12) & 0xF;
    u32 rsVal = reg((insn >> 8) & 0xF);
    bool isAccumulate = insn & (1u << 21);

    if (isSigned) {
        s64 result = (s64)(s32)reg(insn & 0xF) * (s64)(s32)rsVal;
        // Accumulate if MLA variant
        if (isAccumulate)
            result += ((s64)((u64)reg(rdHi) << 32)) | reg(rdLo);
        setRegBanked(rdHi, (u32)(result >> 32));
        setRegBanked(rdLo, (u32)(result & 0xFFFFFFFF));
        if (insn & (1u << 20))
            cpsr() = (cpsr() & ~(Flag::N | Flag::Z)) | (u32)aluNZ64((u64)result);
    } else {
        u64 result = (u64)reg(insn & 0xF) * (u64)rsVal;
        if (isAccumulate)
            result += ((u64)reg(rdHi) << 32) | reg(rdLo);
        setRegBanked(rdHi, (u32)(result >> 32));
        setRegBanked(rdLo, (u32)(result & 0xFFFFFFFF));
        if (insn & (1u << 20))
            cpsr() = (cpsr() & ~(Flag::N | Flag::Z)) | (u32)aluNZ64(result);
    }

    // xMULL = 1S + (m+1)I, xMLAL = 1S + (m+2)I
    int m = isSigned ? mulCyclesSigned(rsVal) : mulCyclesUnsigned(rsVal);
    int extra = isAccumulate ? 2 : 1;
    m_cycles = 1 + m + extra;
}



// ============================================================================
// ARM Single Data Transfer — LDR / STR (word/byte)
// Encoding: cond 01I PUBWL Rn Rd offset12
// ============================================================================
void ARM7TDMI::armSingleTransfer(u32 insn) {
    bool isBit  = insn & (1u << 25); // I: offset is register (1) or immediate (0)
    bool preIdx = insn & (1u << 24); // P: pre-indexed
    bool upBit  = insn & (1u << 23); // U: add offset (1) or subtract (0)
    bool byteOp = insn & (1u << 22); // B: byte transfer
    bool wBack  = insn & (1u << 21); // W: write-back
    bool isLoad = insn & (1u << 20); // L: load (1) or store (0)

    // Calculate offset
    u32 offset = isBit ? decodeShift(insn, nullptr) : (insn & 0xFFF);

    // Base register
    u32 rnIdx = (insn >> 16) & 0xF;
    u32 rnv;

    if (preIdx) {
        rnv = upBit ? (reg(rnIdx) + offset) : (reg(rnIdx) - offset);
        if (wBack)
            setRegBanked(rnIdx, rnv);
        else if (rnIdx == 15)
            rnv += 8; // PC pipeline compensation
    } else {
        rnv = (rnIdx == 15) ? (pc() + 8) : reg(rnIdx);
    }

    u32 rdIdx = (insn >> 12) & 0xF;

    if (isLoad) {
        if (byteOp) {
            setRegBanked(rdIdx, read8(rnv));
        } else {
            if (rdIdx == 15) {
                pc() = read32(rnv);
                pc() -= 4; // compensate for the +4 at caller
                m_cycles = 5; // LDR PC: 2S+2N+1I
            } else {
                setRegBanked(rdIdx, read32(rnv));
            }
        }
    } else {
        // Store
        if (byteOp) {
            write8(rnv, (u8)(reg(rdIdx) & 0xFF));
        } else {
            u32 val = (rdIdx == 15) ? (pc() + 8 + 4) : reg(rdIdx);
            write32(rnv, val);
        }
        m_cycles = 2; // STR: 2N
    }

    // Post-indexed writeback
    if (!preIdx) {
        u32 newBase = upBit ? (rnv + offset) : (rnv - offset);
        if (rdIdx == rnIdx)
            setRegBanked(rnIdx, reg(rdIdx)); // writeback ignored when Rd==Rn
        else
            setRegBanked(rnIdx, newBase);
    }
}

// ============================================================================
// ARM Halfword/Signed Data Transfer — LDRH/STRH/LDRSB/LDRSH
// Encoding: cond 000P U0WL Rn Rd 0000 1SH1 Rm   (register offset)
//           cond 000P U1WL Rn Rd offH 1SH1 offL  (immediate offset)
// ============================================================================
void ARM7TDMI::armHalfwordTransfer(u32 insn) {
    bool preIdx = insn & (1u << 24);
    bool upBit  = insn & (1u << 23);
    bool immOff = insn & (1u << 22);
    bool wBack  = insn & (1u << 21);
    bool isLoad = insn & (1u << 20);

    u32 offset = immOff
        ? (((insn >> 4) & 0xF0) | (insn & 0xF))
        : reg(insn & 0xF);

    u32 rnIdx = (insn >> 16) & 0xF;
    u32 rnv;

    if (preIdx) {
        rnv = upBit ? (reg(rnIdx) + offset) : (reg(rnIdx) - offset);
        if (wBack) setRegBanked(rnIdx, rnv);
        else if (rnIdx == 15) rnv += 8;
    } else {
        rnv = (rnIdx == 15) ? (pc() + 8) : reg(rnIdx);
    }

    u32 rdIdx = (insn >> 12) & 0xF;
    u32 sh = (insn >> 5) & 3; // S:H bits determine transfer type

    if (isLoad) {
        u32 val = 0;
        switch (sh) {
        case 1: // LDRH — unsigned halfword
            val = read16(rnv);
            break;
        case 2: // LDRSB — signed byte
            val = (u32)(s32)(s8)read8(rnv);
            break;
        case 3: // LDRSH — signed halfword
            val = (u32)(s32)(s16)read16(rnv);
            break;
        }
        if (rdIdx == 15) {
            pc() = val;
            m_cycles = 5; // LDRH/SB/SH PC: 2S+2N+1I
        } else {
            setRegBanked(rdIdx, val);
            pc() += 4;
        }
    } else {
        // STRH
        u32 val = (rdIdx == 15) ? (pc() + 8 + 4) : reg(rdIdx);
        write16(rnv, (u16)val);
        pc() += 4;
        m_cycles = 2; // STRH: 2N
    }

    // Post-indexed writeback
    if (!preIdx) {
        u32 newBase = upBit ? (rnv + offset) : (rnv - offset);
        if (rdIdx == rnIdx)
            setRegBanked(rnIdx, reg(rdIdx));
        else
            setRegBanked(rnIdx, newBase);
    }
}

// ============================================================================
// ARM Swap — SWP / SWPB
// Atomically reads [Rn], writes Rm to [Rn], stores old value in Rd.
// ============================================================================
void ARM7TDMI::armSwap(u32 insn) {
    u32 addr = reg((insn >> 16) & 0xF);
    u32 rmVal = reg(insn & 0xF);
    u32 rdIdx = (insn >> 12) & 0xF;

    if (insn & (1u << 22)) {
        // SWPB — byte
        u32 tmp = read8(addr);
        write8(addr, (u8)rmVal);
        setRegBanked(rdIdx, tmp);
    } else {
        // SWP — word
        u32 tmp = read32(addr);
        write32(addr, rmVal);
        setRegBanked(rdIdx, tmp);
    }
    pc() += 4;
    m_cycles = 4; // SWP: 1S+2N+1I
}

// ============================================================================
// ARM PSR Transfer — MRS / MSR
// MRS: copy CPSR/SPSR to a general register
// MSR: write value to CPSR/SPSR fields
// ============================================================================
void ARM7TDMI::armPSRTransfer(u32 insn) {
    bool useSPSR = insn & (1u << 22);
    int psrReg = useSPSR ? s_regBank[modeIndex()][17] : CPSR;
    u32 oldMode = cpsr() & 0xF;

    if (insn & (1u << 21)) {
        // MSR: write to PSR
        u32 val;
        if (insn & (1u << 25)) {
            // Immediate with rotation
            u32 imm = insn & 0xFF;
            u32 rot = ((insn >> 8) & 0xF) << 1;
            val = rot ? ((imm >> rot) | (imm << (32 - rot))) : imm;
        } else {
            val = reg(insn & 0xF);
        }

        u32 newval = m_regs[psrReg];

        if (psrReg == CPSR) {
            // Control fields only writable from privileged modes
            if (oldMode != static_cast<u32>(Mode::User)) {
                if (insn & (1u << 16)) newval = (newval & 0xFFFFFF00) | (val & 0xFF);
                if (insn & (1u << 17)) newval = (newval & 0xFFFF00FF) | (val & 0xFF00);
                if (insn & (1u << 18)) newval = (newval & 0xFF00FFFF) | (val & 0xFF0000);
            }
            // Status flags always writable
            if (insn & (1u << 19))
                newval = (newval & 0x00FFFFFF) | (val & 0xF8000000);
        } else {
            // SPSR: writable from privileged modes only
            if (((cpsr() & 0x1F) > 0x10) && ((cpsr() & 0x1F) < 0x1F)) {
                if (insn & (1u << 16)) newval = (newval & 0xFFFFFF00) | (val & 0xFF);
                if (insn & (1u << 17)) newval = (newval & 0xFFFF00FF) | (val & 0xFF00);
                if (insn & (1u << 18)) newval = (newval & 0xFF00FFFF) | (val & 0xFF0000);
                if (insn & (1u << 19)) newval = (newval & 0x00FFFFFF) | (val & 0xF8000000);
            }
        }

        newval |= 0x10; // force valid mode (bit 4 always set)
        m_regs[psrReg] = newval;

        // Only switch banks when writing to CPSR
        if (psrReg == CPSR && (newval & 0xF) != oldMode)
            switchMode(static_cast<Mode>(newval & 0xF));
    } else {
        // MRS: read PSR to register
        setRegBanked((insn >> 12) & 0xF, m_regs[psrReg]);
    }
}



// ============================================================================
// Block transfer helpers — used by LDM/STM
// These iterate through register bits, loading/storing from ascending
// or descending addresses.
// ============================================================================

int ARM7TDMI::loadIncrement(u32 pat, u32 base, u32 sFlag) {
    (void)sFlag;
    int count = 0;
    base &= ~3u;
    for (int i = 0; i < 16; i++) {
        if ((pat >> i) & 1) {
            base += 4;
            setRegBanked(i, read32(base));
            count++;
        }
    }
    return count;
}

int ARM7TDMI::loadDecrement(u32 pat, u32 base, u32 sFlag) {
    (void)sFlag;
    int count = 0;
    base &= ~3u;
    for (int i = 15; i >= 0; i--) {
        if ((pat >> i) & 1) {
            base -= 4;
            setRegBanked(i, read32(base));
            count++;
        }
    }
    return count;
}

int ARM7TDMI::storeIncrement(u32 pat, u32 base) {
    int count = 0;
    for (int i = 0; i < 16; i++) {
        if ((pat >> i) & 1) {
            base += 4;
            write32(base, reg(i));
            count++;
        }
    }
    return count;
}

int ARM7TDMI::storeDecrement(u32 pat, u32 base) {
    int count = 0;
    for (int i = 15; i >= 0; i--) {
        if ((pat >> i) & 1) {
            base -= 4;
            write32(base, reg(i));
            count++;
        }
    }
    return count;
}

// ============================================================================
// ARM Block Data Transfer — LDM / STM
// Encoding: cond 100P USWL Rn register_list
// ============================================================================
void ARM7TDMI::armBlockTransfer(u32 insn) {
    u32 rbIdx = (insn >> 16) & 0xF;
    u32 rbv = reg(rbIdx);
    bool isLoad = insn & (1u << 20);
    bool upBit  = insn & (1u << 23);
    bool preIdx = insn & (1u << 24);
    bool sFlag  = insn & (1u << 22);
    bool wBack  = insn & (1u << 21);

    int result;

    if (isLoad) {
        if (upBit) {
            // Incrementing
            u32 base = preIdx ? rbv : (rbv - 4);

            // User bank transfer: S flag set but R15 not in list
            if (sFlag && !(insn & 0x8000)) {
                int savedMode = modeIndex();
                switchMode(Mode::User);
                result = loadIncrement(insn & 0xFFFF, base, sFlag);
                switchMode(static_cast<Mode>(savedMode));
            } else {
                result = loadIncrement(insn & 0xFFFF, base, sFlag);
            }

            if (wBack)
                setRegBanked(rbIdx, reg(rbIdx) + result * 4);

            // LDM: nS + 1N + 1I = n+2; LDM with PC: +2 extra
            m_cycles = result + 2;
            if (insn & 0x8000) {
                pc() -= 4;
                if (sFlag) {
                    cpsr() = spsr();
                    switchMode(static_cast<Mode>(cpsr() & 0xF));
                }
                m_cycles += 2;
            }
        } else {
            // Decrementing
            u32 base = preIdx ? rbv : (rbv + 4);

            if (sFlag && !(insn & 0x8000)) {
                int savedMode = modeIndex();
                switchMode(Mode::User);
                result = loadDecrement(insn & 0xFFFF, base, sFlag);
                switchMode(static_cast<Mode>(savedMode));
            } else {
                result = loadDecrement(insn & 0xFFFF, base, sFlag);
            }

            if (wBack)
                setRegBanked(rbIdx, reg(rbIdx) - result * 4);

            // LDM: nS + 1N + 1I = n+2; LDM with PC: +2 extra
            m_cycles = result + 2;
            if (insn & 0x8000) {
                pc() -= 4;
                if (sFlag) {
                    cpsr() = spsr();
                    switchMode(static_cast<Mode>(cpsr() & 0xF));
                }
                m_cycles += 2;
            }
        }
    } else {
        // Storing
        if (insn & (1u << 15))
            pc() += 12; // R15 stored is PC+12

        if (upBit) {
            u32 base = preIdx ? rbv : (rbv - 4);
            if (sFlag && !(insn & 0x8000)) {
                int savedMode = modeIndex();
                switchMode(Mode::User);
                result = storeIncrement(insn & 0xFFFF, base);
                switchMode(static_cast<Mode>(savedMode));
            } else {
                result = storeIncrement(insn & 0xFFFF, base);
            }
            if (wBack)
                setRegBanked(rbIdx, reg(rbIdx) + result * 4);
        } else {
            u32 base = preIdx ? rbv : (rbv + 4);
            if (sFlag && !(insn & 0x8000)) {
                int savedMode = modeIndex();
                switchMode(Mode::User);
                result = storeDecrement(insn & 0xFFFF, base);
                switchMode(static_cast<Mode>(savedMode));
            } else {
                result = storeDecrement(insn & 0xFFFF, base);
            }
            if (wBack)
                setRegBanked(rbIdx, reg(rbIdx) - result * 4);
        }

        if (insn & (1u << 15))
            pc() -= 12;

        // STM: (n-1)S + 2N = n+1
        m_cycles = result + 1;
    }
}

// ============================================================================
// Coprocessor instructions — delegate to callbacks
// ============================================================================

void ARM7TDMI::armCoprocDataOp(u32 insn) {
    if (m_cop.dataOp)
        m_cop.dataOp(0, insn);
}

void ARM7TDMI::armCoprocRegTransfer(u32 insn) {
    if (insn & (1u << 20)) {
        // MRC: read from coprocessor
        if (m_cop.regRead)
            setRegBanked((insn >> 12) & 0xF, m_cop.regRead(insn));
    }
    // MCR: write to coprocessor (handled by callback if present)
}

void ARM7TDMI::armCoprocDataTransfer(u32 insn) {
    u32 rn = (insn >> 16) & 0xF;
    u32 rnv = reg(rn);
    u32 ornv = rnv;
    u32 offset = (insn & 0xFF) << 2;
    u32* prn = &m_regs[s_regBank[modeIndex()][rn]];

    // Pre-increment
    if ((insn & (1u << 24)) && offset) {
        rnv += (insn & (1u << 23)) ? offset : -offset;
    }

    if (insn & (1u << 20)) {
        if (m_cop.dataTransferRead)
            m_cop.dataTransferRead(insn, prn, m_mem.read32);
    } else {
        if (m_cop.dataTransferWrite)
            m_cop.dataTransferWrite(insn, prn, m_mem.write32);
    }

    // Restore base if no writeback
    if (!(insn & (1u << 21)))
        setRegBanked(rn, ornv);
}



// ============================================================================
// Thumb instruction execution — 16-bit compressed instruction set
// Dispatches by top 4 bits (insn >> 12).
// ============================================================================
void ARM7TDMI::thumbExecute(u32 insn) {
    switch ((insn >> 12) & 0xF) {

    // ---- 0x0: Shift (LSL / LSR) ----
    case 0x0: {
        u32 rs = (insn >> 3) & 7;
        u32 rd = insn & 7;
        u32 rrs = reg(rs);
        u32 offs = (insn >> 6) & 0x1F;

        if (insn & (1u << 11)) {
            // LSR
            if (offs != 0) {
                setRegBanked(rd, rrs >> offs);
                cpsr() = (cpsr() & ~Flag::C) | ((rrs & (1u << (offs - 1))) ? Flag::C : 0);
            } else {
                setRegBanked(rd, 0);
                cpsr() = (cpsr() & ~Flag::C) | ((rrs & 0x80000000) ? Flag::C : 0);
            }
        } else {
            // LSL
            if (offs != 0) {
                setRegBanked(rd, rrs << offs);
                cpsr() = (cpsr() & ~Flag::C) | ((rrs & (1u << (31 - (offs - 1)))) ? Flag::C : 0);
            } else {
                setRegBanked(rd, rrs);
            }
        }
        cpsr() = (cpsr() & ~(Flag::N | Flag::Z)) | aluNZ(reg(rd));
        pc() += 2;
        break;
    }

    // ---- 0x1: Arithmetic (ADD/SUB reg/imm) + ASR ----
    case 0x1: {
        if (insn & (1u << 11)) {
            // ADD/SUB with register or immediate
            u32 subType = (insn >> 9) & 3;
            u32 rdIdx = insn & 7;
            u32 rsVal = reg((insn >> 3) & 7);
            u32 rnImm = (insn >> 6) & 7;
            u32 result;

            switch (subType) {
            case 0: // ADD Rd, Rs, Rn
                result = rsVal + reg(rnImm);
                setRegBanked(rdIdx, result);
                setAddFlags(reg(rdIdx), rsVal, reg(rnImm), false);
                pc() += 2;
                break;
            case 1: // SUB Rd, Rs, Rn
                result = rsVal - reg(rnImm);
                setRegBanked(rdIdx, result);
                setSubFlags(reg(rdIdx), rsVal, reg(rnImm), false);
                pc() += 2;
                break;
            case 2: // ADD Rd, Rs, #imm3
                result = rsVal + rnImm;
                setRegBanked(rdIdx, result);
                setAddFlags(reg(rdIdx), rsVal, rnImm, false);
                pc() += 2;
                break;
            case 3: // SUB Rd, Rs, #imm3
                result = rsVal - rnImm;
                setRegBanked(rdIdx, result);
                setSubFlags(reg(rdIdx), rsVal, rnImm, false);
                pc() += 2;
                break;
            }
        } else {
            // ASR (Arithmetic Shift Right) with immediate
            u32 rs = (insn >> 3) & 7;
            u32 rd = insn & 7;
            u32 rrs = reg(rs);
            u32 offs = (insn >> 6) & 0x1F;
            if (offs == 0) offs = 32;

            if (offs >= 32) {
                cpsr() = (cpsr() & ~Flag::C) | ((rrs >> 31) ? Flag::C : 0);
                setRegBanked(rd, (rrs & 0x80000000) ? 0xFFFFFFFF : 0);
            } else {
                cpsr() = (cpsr() & ~Flag::C) | (((rrs >> (offs - 1)) & 1) ? Flag::C : 0);
                u32 result = (rrs & 0x80000000)
                    ? ((0xFFFFFFFF << (32 - offs)) | (rrs >> offs))
                    : (rrs >> offs);
                setRegBanked(rd, result);
            }
            cpsr() = (cpsr() & ~(Flag::N | Flag::Z)) | aluNZ(reg(rd));
            pc() += 2;
        }
        break;
    }

    // ---- 0x2: CMP / MOV immediate ----
    case 0x2: {
        u32 rdIdx = (insn >> 8) & 7;
        u32 imm = insn & 0xFF;

        if (insn & (1u << 11)) {
            // CMP Rd, #imm8
            u32 rdVal = reg(rdIdx);
            u32 result = rdVal - imm;
            setSubFlags(result, rdVal, imm, false);
            pc() += 2;
        } else {
            // MOV Rd, #imm8
            setRegBanked(rdIdx, imm);
            cpsr() = (cpsr() & ~(Flag::N | Flag::Z)) | aluNZ(reg(rdIdx));
            pc() += 2;
        }
        break;
    }

    // ---- 0x3: ADD/SUB immediate ----
    case 0x3: {
        u32 rdIdx = (insn >> 8) & 7;
        u32 imm = insn & 0xFF;
        u32 rnVal = reg(rdIdx);

        if (insn & (1u << 11)) {
            // SUB Rd, #imm8
            u32 result = rnVal - imm;
            setRegBanked(rdIdx, result);
            setSubFlags(reg(rdIdx), rnVal, imm, false);
            pc() += 2;
        } else {
            // ADD Rd, #imm8
            u32 result = rnVal + imm;
            setRegBanked(rdIdx, result);
            setAddFlags(reg(rdIdx), rnVal, imm, false);
            pc() += 2;
        }
        break;
    }

    // ---- 0x4: ALU operations + Hi-register ops + BX + PC-relative load ----
    case 0x4: {
        u32 group = (insn >> 10) & 3;
        if (group == 0) {
            // Thumb ALU operations (16 opcodes)
            u32 aluOp = (insn >> 6) & 0xF;
            u32 rsIdx = (insn >> 3) & 7;
            u32 rdIdx = insn & 7;
            u32 rsVal = reg(rsIdx);
            u32 rdVal = reg(rdIdx);

            switch (aluOp) {
            case 0x0: // AND
                setRegBanked(rdIdx, rdVal & rsVal);
                cpsr() = (cpsr() & ~(Flag::N | Flag::Z)) | aluNZ(reg(rdIdx));
                pc() += 2; break;
            case 0x1: // EOR
                setRegBanked(rdIdx, rdVal ^ rsVal);
                cpsr() = (cpsr() & ~(Flag::N | Flag::Z)) | aluNZ(reg(rdIdx));
                pc() += 2; break;
            case 0x2: { // LSL
                u32 shift = rsVal & 0xFF;
                if (shift > 0) {
                    if (shift < 32) {
                        cpsr() = (cpsr() & ~Flag::C) | ((rdVal & (1u << (31 - (shift - 1)))) ? Flag::C : 0);
                        setRegBanked(rdIdx, rdVal << shift);
                    } else if (shift == 32) {
                        cpsr() = (cpsr() & ~Flag::C) | ((rdVal & 1) ? Flag::C : 0);
                        setRegBanked(rdIdx, 0);
                    } else {
                        cpsr() &= ~Flag::C;
                        setRegBanked(rdIdx, 0);
                    }
                }
                cpsr() = (cpsr() & ~(Flag::N | Flag::Z)) | aluNZ(reg(rdIdx));
                m_cycles = 2; // 1S+1I (register shift)
                pc() += 2; break;
            }
            case 0x3: { // LSR
                u32 shift = rsVal & 0xFF;
                if (shift > 0) {
                    if (shift < 32) {
                        cpsr() = (cpsr() & ~Flag::C) | ((rdVal & (1u << (shift - 1))) ? Flag::C : 0);
                        setRegBanked(rdIdx, rdVal >> shift);
                    } else if (shift == 32) {
                        cpsr() = (cpsr() & ~Flag::C) | ((rdVal & 0x80000000) ? Flag::C : 0);
                        setRegBanked(rdIdx, 0);
                    } else {
                        cpsr() &= ~Flag::C;
                        setRegBanked(rdIdx, 0);
                    }
                }
                cpsr() = (cpsr() & ~(Flag::N | Flag::Z)) | aluNZ(reg(rdIdx));
                m_cycles = 2; // 1S+1I (register shift)
                pc() += 2; break;
            }
            case 0x4: { // ASR
                u32 shift = rsVal & 0xFF;
                if (shift != 0) {
                    if (shift >= 32) {
                        cpsr() = (cpsr() & ~Flag::C) | ((rdVal >> 31) ? Flag::C : 0);
                        setRegBanked(rdIdx, (rdVal & 0x80000000) ? 0xFFFFFFFF : 0);
                    } else {
                        cpsr() = (cpsr() & ~Flag::C) | (((rdVal >> (shift - 1)) & 1) ? Flag::C : 0);
                        u32 r = (rdVal & 0x80000000)
                            ? ((0xFFFFFFFF << (32 - shift)) | (rdVal >> shift))
                            : (rdVal >> shift);
                        setRegBanked(rdIdx, r);
                    }
                }
                cpsr() = (cpsr() & ~(Flag::N | Flag::Z)) | aluNZ(reg(rdIdx));
                m_cycles = 2; // 1S+1I (register shift)
                pc() += 2; break;
            }
            case 0x5: { // ADC
                u32 c = (cpsr() & Flag::C) ? 1 : 0;
                u32 result = rdVal + rsVal + c;
                setAddFlags(result, rdVal, rsVal, false);
                pc() += 2;
                setRegBanked(rdIdx, result);
                break;
            }
            case 0x6: { // SBC
                u32 c = (cpsr() & Flag::C) ? 0 : 1;
                u32 result = rdVal - rsVal - c;
                setSubFlags(result, rdVal, rsVal, false);
                pc() += 2;
                setRegBanked(rdIdx, result);
                break;
            }
            case 0x7: { // ROR
                // ARM spec: use full bottom byte of Rs, then reduce mod 32
                u32 fullShift = rsVal & 0xFF;
                if (fullShift != 0) {
                    u32 shift = fullShift & 0x1F;
                    if (shift != 0) {
                        cpsr() = (cpsr() & ~Flag::C) | ((rdVal & (1u << (shift - 1))) ? Flag::C : 0);
                        setRegBanked(rdIdx, (rdVal >> shift) | (rdVal << (32 - shift)));
                    } else {
                        // Multiple of 32: value unchanged, carry = bit 31
                        cpsr() = (cpsr() & ~Flag::C) | ((rdVal >> 31) ? Flag::C : 0);
                    }
                }
                cpsr() = (cpsr() & ~(Flag::N | Flag::Z)) | aluNZ(reg(rdIdx));
                m_cycles = 2; // 1S+1I (register shift)
                pc() += 2; break;
            }
            case 0x8: // TST
                cpsr() = (cpsr() & ~(Flag::N | Flag::Z)) | aluNZ(rdVal & rsVal);
                pc() += 2; break;
            case 0x9: { // NEG
                u32 result = 0 - rsVal;
                setRegBanked(rdIdx, result);
                setSubFlags(reg(rdIdx), 0, rsVal, false);
                pc() += 2; break;
            }
            case 0xA: { // CMP
                u32 result = rdVal - rsVal;
                setSubFlags(result, rdVal, rsVal, false);
                pc() += 2; break;
            }
            case 0xB: { // CMN
                u32 result = rdVal + rsVal;
                setAddFlags(result, rdVal, rsVal, false);
                pc() += 2; break;
            }
            case 0xC: // ORR
                setRegBanked(rdIdx, rdVal | rsVal);
                cpsr() = (cpsr() & ~(Flag::N | Flag::Z)) | aluNZ(reg(rdIdx));
                pc() += 2; break;
            case 0xD: { // MUL
                u32 result = rdVal * rsVal;
                setRegBanked(rdIdx, result);
                cpsr() = (cpsr() & ~(Flag::N | Flag::Z)) | aluNZ(result);
                // Thumb MUL: 1S + mI
                m_cycles = 1 + mulCyclesSigned(rdVal);
                pc() += 2; break;
            }
            case 0xE: // BIC
                setRegBanked(rdIdx, rdVal & ~rsVal);
                cpsr() = (cpsr() & ~(Flag::N | Flag::Z)) | aluNZ(reg(rdIdx));
                pc() += 2; break;
            case 0xF: // MVN
                setRegBanked(rdIdx, ~rsVal);
                cpsr() = (cpsr() & ~(Flag::N | Flag::Z)) | aluNZ(reg(rdIdx));
                pc() += 2; break;
            }
        } else if (group == 1) {
            // Hi-register operations + BX
            thumbHiRegBX(insn);
        } else {
            // PC-relative load: LDR Rd, [PC, #imm8*4]
            u32 rdIdx = (insn >> 8) & 7;
            u32 addr = (pc() & ~3u) + 4 + ((insn & 0xFF) << 2);
            setRegBanked(rdIdx, read32(addr));
            m_cycles = 3; // LDR: 1S+1N+1I
            pc() += 2;
        }
        break;
    }

    // ---- 0x5: Register-offset load/store ----
    case 0x5: {
        u32 type = (insn >> 9) & 7;
        u32 rm = reg((insn >> 6) & 7);
        u32 rn = reg((insn >> 3) & 7);
        u32 rd = insn & 7;
        u32 addr = rn + rm;

        switch (type) {
        case 0: write32(addr, reg(rd)); m_cycles = 2; break;             // STR
        case 1: write16(addr, (u16)reg(rd)); m_cycles = 2; break;        // STRH
        case 2: write8(addr, (u8)reg(rd)); m_cycles = 2; break;          // STRB
        case 3: { // LDRSB
            u32 v = read8(addr);
            setRegBanked(rd, (v & 0x80) ? (v | 0xFFFFFF00) : v);
            m_cycles = 3; break;
        }
        case 4: setRegBanked(rd, read32(addr)); m_cycles = 3; break;     // LDR
        case 5: setRegBanked(rd, read16(addr)); m_cycles = 3; break;     // LDRH
        case 6: setRegBanked(rd, read8(addr)); m_cycles = 3; break;      // LDRB
        case 7: { // LDRSH
            u32 v = read16(addr);
            setRegBanked(rd, (v & 0x8000) ? (v | 0xFFFF0000) : v);
            m_cycles = 3; break;
        }
        }
        pc() += 2;
        break;
    }

    default:
        // Handled in part 7
        thumbExecuteHigh(insn);
        break;
    }
}



// ============================================================================
// Thumb Hi-Register Operations + BX
// Format 5: ADD/CMP/MOV with high registers, BX/BLX
// ============================================================================
void ARM7TDMI::thumbHiRegBX(u32 insn) {
    u32 op = (insn >> 8) & 3;
    u32 h = (insn >> 6) & 3;
    u32 rsIdx = (insn >> 3) & 7;
    u32 rdIdx = insn & 7;

    switch (op) {
    case 0: // ADD
        switch (h) {
        case 1: // ADD Rd, HRs
            setRegBanked(rdIdx, reg(rdIdx) + reg(rsIdx + 8));
            if (rsIdx == 7) setRegBanked(rdIdx, reg(rdIdx) + 4);
            break;
        case 2: // ADD HRd, Rs
            setRegBanked(rdIdx + 8, reg(rdIdx + 8) + reg(rsIdx));
            if (rdIdx == 7) { pc() += 2; m_cycles = 3; }
            break;
        case 3: // ADD HRd, HRs
            setRegBanked(rdIdx + 8, reg(rdIdx + 8) + reg(rsIdx + 8));
            if (rsIdx == 7) setRegBanked(rdIdx + 8, reg(rdIdx + 8) + 4);
            if (rdIdx == 7) { pc() += 2; m_cycles = 3; }
            break;
        }
        pc() += 2;
        break;

    case 1: { // CMP
        u32 rsVal, rdVal;
        switch (h) {
        case 0: rsVal = reg(rsIdx);     rdVal = reg(rdIdx);     break;
        case 1: rsVal = reg(rsIdx + 8); rdVal = reg(rdIdx);     break;
        case 2: rsVal = reg(rsIdx);     rdVal = reg(rdIdx + 8); break;
        case 3: rsVal = reg(rsIdx + 8); rdVal = reg(rdIdx + 8); break;
        default: rsVal = rdVal = 0; break;
        }
        u32 result = rdVal - rsVal;
        setSubFlags(result, rdVal, rsVal, false);
        pc() += 2;
        break;
    }

    case 2: // MOV
        switch (h) {
        case 1: // MOV Rd, HRs
            if (rsIdx == 7) setRegBanked(rdIdx, reg(rsIdx + 8) + 4);
            else setRegBanked(rdIdx, reg(rsIdx + 8));
            pc() += 2;
            break;
        case 2: // MOV HRd, Rs
            setRegBanked(rdIdx + 8, reg(rsIdx));
            if (rdIdx != 7) pc() += 2;
            else { pc() &= ~1u; m_cycles = 3; }
            break;
        case 3: // MOV HRd, HRs
            if (rsIdx == 7) setRegBanked(rdIdx + 8, reg(rsIdx + 8) + 4);
            else setRegBanked(rdIdx + 8, reg(rsIdx + 8));
            if (rdIdx != 7) pc() += 2;
            else { pc() &= ~1u; m_cycles = 3; }
            break;
        default:
            pc() += 2;
            break;
        }
        break;

    case 3: { // BX / BLX
        u32 addr;
        if (h & 1) {
            // BX HRs
            addr = reg(rsIdx + 8);
            if ((rsIdx + 8) == 15) addr += 2;
        } else {
            // BX Rs
            addr = reg(rsIdx);
        }
        if (addr & 1) {
            addr &= ~1u; // Stay in Thumb
        } else {
            cpsr() &= ~Flag::T; // Switch to ARM
            if (addr & 2) addr += 2;
        }
        pc() = addr;
        m_cycles = 3; // BX: 2S+1N
        break;
    }
    }
}

// ============================================================================
// Thumb opcodes 0x6–0xF (called from thumbExecute default case)
// ============================================================================
void ARM7TDMI::thumbExecuteHigh(u32 insn) {
    switch ((insn >> 12) & 0xF) {

    // ---- 0x6: Word load/store with immediate offset ----
    case 0x6: {
        u32 rn = (insn >> 3) & 7;
        u32 rd = insn & 7;
        u32 offs = ((insn >> 6) & 0x1F) << 2;
        if (insn & (1u << 11)) {
            setRegBanked(rd, read32(reg(rn) + offs));
            m_cycles = 3; // LDR: 1S+1N+1I
        } else {
            write32(reg(rn) + offs, reg(rd));
            m_cycles = 2; // STR: 2N
        }
        pc() += 2;
        break;
    }

    // ---- 0x7: Byte load/store with immediate offset ----
    case 0x7: {
        u32 rn = (insn >> 3) & 7;
        u32 rd = insn & 7;
        u32 offs = (insn >> 6) & 0x1F;
        if (insn & (1u << 11)) {
            setRegBanked(rd, read8(reg(rn) + offs));
            m_cycles = 3; // LDRB: 1S+1N+1I
        } else {
            write8(reg(rn) + offs, (u8)reg(rd));
            m_cycles = 2; // STRB: 2N
        }
        pc() += 2;
        break;
    }

    // ---- 0x8: Halfword load/store with immediate offset ----
    case 0x8: {
        u32 rs = (insn >> 3) & 7;
        u32 rd = insn & 7;
        u32 offs = ((insn >> 6) & 0x1F) << 1;
        if (insn & (1u << 11)) {
            setRegBanked(rd, read16(reg(rs) + offs));
            m_cycles = 3; // LDRH: 1S+1N+1I
        } else {
            write16(reg(rs) + offs, (u16)reg(rd));
            m_cycles = 2; // STRH: 2N
        }
        pc() += 2;
        break;
    }

    // ---- 0x9: SP-relative load/store ----
    case 0x9: {
        u32 rd = (insn >> 8) & 7;
        u32 offs = (u8)(insn & 0xFF);
        u32 addr = reg(13) + ((u32)offs << 2);
        if (insn & (1u << 11)) {
            setRegBanked(rd, read32(addr));
            m_cycles = 3; // LDR SP: 1S+1N+1I
        } else {
            write32(addr, reg(rd));
            m_cycles = 2; // STR SP: 2N
        }
        pc() += 2;
        break;
    }

    // ---- 0xA: Get relative address (ADD Rd, PC/SP, #nn) ----
    case 0xA: {
        u32 rd = (insn >> 8) & 7;
        u32 offs = (u8)(insn & 0xFF) << 2;
        if (insn & (1u << 11))
            setRegBanked(rd, reg(13) + offs);       // ADD Rd, SP, #imm
        else
            setRegBanked(rd, ((pc() + 4) & ~3u) + offs); // ADD Rd, PC, #imm
        pc() += 2;
        break;
    }

    // ---- 0xB: Stack operations (ADD SP / PUSH / POP) ----
    case 0xB: {
        u32 subOp = (insn >> 8) & 0xF;
        switch (subOp) {
        case 0x0: { // ADD SP, #imm / ADD SP, -#imm
            u32 imm = (insn & 0x7F) << 2;
            if (insn & 0x80)
                setRegBanked(13, reg(13) - imm);
            else
                setRegBanked(13, reg(13) + imm);
            pc() += 2;
            break;
        }
        case 0x4: { // PUSH {Rlist}
            int nregs = 0;
            for (int i = 7; i >= 0; i--) {
                if (insn & (1 << i)) {
                    setRegBanked(13, reg(13) - 4);
                    write32(reg(13), reg(i));
                    nregs++;
                }
            }
            // PUSH (STM): (n-1)S + 2N = n+1
            m_cycles = nregs + 1;
            pc() += 2;
            break;
        }
        case 0x5: { // PUSH {Rlist, LR}
            int nregs = 1; // LR
            setRegBanked(13, reg(13) - 4);
            write32(reg(13), reg(14));
            for (int i = 7; i >= 0; i--) {
                if (insn & (1 << i)) {
                    setRegBanked(13, reg(13) - 4);
                    write32(reg(13), reg(i));
                    nregs++;
                }
            }
            // PUSH (STM): (n-1)S + 2N = n+1
            m_cycles = nregs + 1;
            pc() += 2;
            break;
        }
        case 0xC: { // POP {Rlist}
            int nregs = 0;
            for (int i = 0; i < 8; i++) {
                if (insn & (1 << i)) {
                    setRegBanked(i, read32(reg(13)));
                    setRegBanked(13, reg(13) + 4);
                    nregs++;
                }
            }
            // POP (LDM): nS + 1N + 1I = n+2
            m_cycles = nregs + 2;
            pc() += 2;
            break;
        }
        case 0xD: { // POP {Rlist, PC}
            int nregs = 1; // PC
            for (int i = 0; i < 8; i++) {
                if (insn & (1 << i)) {
                    setRegBanked(i, read32(reg(13)));
                    setRegBanked(13, reg(13) + 4);
                    nregs++;
                }
            }
            pc() = read32(reg(13)) & ~1u;
            setRegBanked(13, reg(13) + 4);
            // POP PC (LDM+PC): (n+1)S + 2N + 1I = n+4
            m_cycles = nregs + 4;
            break;
        }
        default:
            pc() += 2;
            break;
        }
        break;
    }

    default:
        // Opcodes 0xC–0xF handled in part 8
        thumbExecuteHighest(insn);
        break;
    }
}



// ============================================================================
// Thumb opcodes 0xC–0xF
// ============================================================================
void ARM7TDMI::thumbExecuteHighest(u32 insn) {
    switch ((insn >> 12) & 0xF) {

    // ---- 0xC: Multiple load/store (LDMIA / STMIA) ----
    case 0xC: {
        u32 rbIdx = (insn >> 8) & 7;
        u32 addr = reg(rbIdx) & 0xFFFFFFFC;
        bool isLoad = insn & (1u << 11);
        int nregs = 0;

        if (isLoad) {
            bool rbInList = insn & (1u << rbIdx);
            for (int i = 0; i < 8; i++) {
                if (insn & (1 << i)) {
                    setRegBanked(i, read32(addr));
                    addr += 4;
                    nregs++;
                }
            }
            if (!rbInList) setRegBanked(rbIdx, addr);
            // LDMIA: nS + 1N + 1I = n+2
            m_cycles = nregs + 2;
        } else {
            for (int i = 0; i < 8; i++) {
                if (insn & (1 << i)) {
                    write32(addr, reg(i));
                    addr += 4;
                    nregs++;
                }
            }
            setRegBanked(rbIdx, addr);
            // STMIA: (n-1)S + 2N = n+1
            m_cycles = nregs + 1;
        }
        pc() += 2;
        break;
    }

    // ---- 0xD: Conditional branch ----
    case 0xD: {
        s32 offs = (s8)(insn & 0xFF);
        u32 cond = (insn >> 8) & 0xF;

        if (cond == 0xF) {
            // SWI (encoded as condition 0xF in Thumb)
            m_pendingSwi = 1;
            m_cycles = 3; // SWI: 2S+1N
            checkIRQState();
            break;
        }

        if (cond == 0xE) {
            // Reserved (ARM9 undefined)
            pc() += 2;
            break;
        }

        if (evalCondition(cond)) {
            pc() += 4 + (offs << 1);
            m_cycles = 3; // Taken: 2S+1N
        } else {
            pc() += 2;
        }
        break;
    }

    // ---- 0xE: Unconditional branch / BLX(1) upper ----
    case 0xE: {
        if (insn & (1u << 11)) {
            // BLX(1) — second half: branch to LR + offset, switch to ARM
            u32 addr = reg(14) + ((insn & 0x7FF) << 1);
            addr &= 0xFFFFFFFC;
            setRegBanked(14, (pc() + 4) | 1);
            pc() = addr;
            m_cycles = 3; // BLX suffix: 2S+1N
        } else {
            // B — unconditional branch with 11-bit offset
            s32 offs = (insn & 0x7FF) << 1;
            if (offs & 0x800) offs |= 0xFFFFF800; // sign extend
            pc() += 4 + offs;
            m_cycles = 3; // B: 2S+1N
        }
        break;
    }

    // ---- 0xF: BL (Branch with Link, two-instruction sequence) ----
    case 0xF: {
        if (insn & (1u << 11)) {
            // Second instruction: add low offset to LR, jump
            u32 addr = reg(14) + ((insn & 0x7FF) << 1);
            setRegBanked(14, (pc() + 2) | 1);
            pc() = addr;
            m_cycles = 3; // BL suffix: 2S+1N
        } else {
            // First instruction: set LR = PC + 4 + (offset << 12)
            u32 offs = (insn & 0x7FF) << 12;
            if (offs & (1u << 22)) offs |= 0xFF800000; // sign extend
            setRegBanked(14, pc() + 4 + offs);
            pc() += 2;
        }
        break;
    }

    default:
        pc() += 2;
        break;
    }
}

// ============================================================================
// Main execution loop — runs for 'cycles' CPU cycles
//
// Each instruction handler sets m_cycles to its actual cycle cost.
// Default is 3 (fetch/decode/execute pipeline).
// Returns the actual number of cycles consumed.
// ============================================================================
int ARM7TDMI::run(int cycles) {
    int remaining = cycles;

    do {
        m_cycles = 3; // default: 1N + 1S + 1I (pipeline refill)

        if (flagT()) {
            // ---- Thumb mode: 16-bit instructions ----
            u32 insn = fetchThumb(pc() & ~1u);
            m_cycles = 1; // most Thumb instructions: 1S
            thumbExecute(insn);
        } else {
            // ---- ARM mode: 32-bit instructions ----
            u32 insn = fetchARM(pc());
            u32 cond = insn >> 28;

            // Check condition code
            if (!evalCondition(cond)) {
                // Condition failed: skip instruction (1 cycle)
                pc() += 4;
                m_cycles = 1;
            } else {
                // Decode and execute based on bits [27:24]
                switch ((insn >> 24) & 0xF) {
                case 0x0: case 0x1: case 0x2: case 0x3:
                    // BX / Multiply / HalfWord DT / Data Processing / PSR
                    if ((insn & 0x0FFFFFF0) == 0x012FFF10) {
                        // BX Rn — Branch and Exchange
                        u32 addr = reg(insn & 0xF);
                        if (addr & 1) { cpsr() |= Flag::T; addr--; }
                        pc() = addr;
                    } else if ((insn & 0x0E000000) == 0 && (insn & 0x80) && (insn & 0x10)) {
                        if (insn & 0x60) {
                            armHalfwordTransfer(insn);
                        } else if (insn & 0x01000000) {
                            armSwap(insn);
                        } else {
                            if (insn & 0x800000) {
                                armMulLong(insn, !!(insn & 0x00400000));
                            } else {
                                armMul(insn);
                            }
                            pc() += 4;
                        }
                    } else if ((insn & 0x0C000000) == 0) {
                        if (!(insn & 0x00100000) && ((insn & 0x01800000) == 0x01000000)) {
                            armPSRTransfer(insn);
                            m_cycles = 1; // PSR: 1 S-cycle
                            pc() += 4;
                        } else {
                            armALU(insn);
                        }
                    }
                    break;

                case 0x4: case 0x5: case 0x6: case 0x7:
                    armSingleTransfer(insn);
                    pc() += 4;
                    break;

                case 0x8: case 0x9:
                    armBlockTransfer(insn);
                    pc() += 4;
                    break;

                case 0xA: case 0xB:
                    armBranch(insn);
                    break;

                case 0xC: case 0xD:
                    armCoprocDataTransfer(insn);
                    pc() += 4;
                    break;

                case 0xE:
                    if (insn & 0x10) armCoprocRegTransfer(insn);
                    else armCoprocDataOp(insn);
                    pc() += 4;
                    break;

                case 0xF:
                    // SWI
                    m_pendingSwi = 1;
                    checkIRQState();
                    break;
                }
            }
        }

        checkIRQState();
        remaining -= m_cycles;
        m_totalCycles += m_cycles;

        // Fold in memory wait-state cycles accumulated during this instruction
        if (m_mem.consumeWaitCycles) {
            int wait = m_mem.consumeWaitCycles();
            remaining -= wait;
            m_totalCycles += wait;
        }

    } while (remaining > 0);

    return cycles - remaining;
}

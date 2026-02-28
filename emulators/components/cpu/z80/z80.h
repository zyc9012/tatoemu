#pragma once

#include <cstdint>
#include <cstdlib>
#include <cstring>

using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using s8  = int8_t;
using s32 = int32_t;

// ---------------------------------------------------------------------------
// Zilog Z80 CPU emulator
//
// The Z80 is an 8-bit CPU with a 16-bit address bus (64 KB address space) and
// a separate I/O address space.  It extends the Intel 8080 instruction set with
// index registers (IX/IY), block operations, bit manipulation, and a two-level
// interrupt system (maskable IRQ with three modes, plus edge-triggered NMI).
// ---------------------------------------------------------------------------
class Z80 {
public:
    // Interrupt line constants
    static constexpr int InputLineNmi = 1;
    static constexpr int ClearLine = 0;
    static constexpr int AssertLine = 1;

    // F register flag bits.  The Z80 flag byte layout is: S Z Y H X P/V N C
    //   Bit 7 (SF) — Sign: set when result is negative (bit 7 of result)
    //   Bit 6 (ZF) — Zero: set when result is zero
    //   Bit 5 (YF) — undocumented, copies bit 5 of result
    //   Bit 4 (HF) — Half-carry: carry from bit 3→4 (used by DAA)
    //   Bit 3 (XF) — undocumented, copies bit 3 of result
    //   Bit 2 (PF/VF) — Parity or oVerflow, context-dependent
    //   Bit 1 (NF) — subtract: set after SUB/SBC/DEC/NEG/CP
    //   Bit 0 (CF) — Carry
    static constexpr u8 CF = 0x01;
    static constexpr u8 NF = 0x02;
    static constexpr u8 PF = 0x04;
    static constexpr u8 VF = 0x04;
    static constexpr u8 XF = 0x08;
    static constexpr u8 HF = 0x10;
    static constexpr u8 YF = 0x20;
    static constexpr u8 ZF = 0x40;
    static constexpr u8 SF = 0x80;

    // Endian-aware register pair
    union Pair {
        u32 d = 0;
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        struct { u8 h3, h2, h, l; } b;
        struct { u16 h, l; } w;
#else
        struct { u8 l, h, h2, h3; } b;
        struct { u16 l, h; } w;
#endif
    };

    // Full CPU state (saveable)
    struct State {
        Pair prvpc, pc, sp, af, bc, de, hl, ix, iy, wz;
        Pair af2, bc2, de2, hl2;       // Shadow register bank (swapped by EX/EXX)
        u8 r = 0, r2 = 0;              // R: refresh counter (low 7 bits), R2: saved bit 7
        u8 iff1 = 0, iff2 = 0;         // Interrupt flip-flops (IFF1 gates IRQs, IFF2 saved by NMI)
        u8 halt = 0, im = 0, i = 0;    // HALT state, interrupt mode (0/1/2), I register
        u8 nmiState = 0, irqState = 0, nmiPending = 0;
        u8 afterEi = 0;                // Delay IRQ check one instruction after EI
        u8 afterRetn = 0;              // Restore IFF1 from IFF2 after RETN
        s32 cyclesRemaining = 0;       // Counts down as instructions execute
        s32 cyclesBudget = 0;          // Initial cycle count for this execute() call
        int endRun = 0;                // Set to 1 to break out of execute loop early
        int holdIrq = 0;               // Hold IRQ line for one-shot interrupt
        int vector = 0xff;             // IRQ vector byte(s) placed on data bus
    };

    using ReadHandler = u8 (*)(u32);
    using WriteHandler = void (*)(u32, u8);

    Z80();
    ~Z80();

    void init();
    void reset();
    int execute(int cycles);

    void setIrqLine(int irqLine, int state);  // Assert/clear IRQ or NMI line
    void setIrqHold() { m_s.holdIrq = 1; }
    void setVector(int v) { m_s.vector = v; }

    void setIoReadHandler(ReadHandler h) { m_ioRead = h; }
    void setIoWriteHandler(WriteHandler h) { m_ioWrite = h; }
    void setProgramReadHandler(ReadHandler h) { m_progRead = h; }
    void setProgramWriteHandler(WriteHandler h) { m_progWrite = h; }
    void setOpReadHandler(ReadHandler h) { m_opRead = h; }
    void setOpArgReadHandler(ReadHandler h) { m_opArgRead = h; }

    // Save/restore entire CPU state for save-states
    void getContext(void* dst) const { if (dst) std::memcpy(dst, &m_s, sizeof(m_s)); }
    void setContext(const void* src) { if (src) { std::memcpy(&m_s, src, sizeof(m_s)); } }
    static constexpr size_t contextSize() { return sizeof(State); }

    s32 totalCycles() const { return m_s.cyclesBudget - m_s.cyclesRemaining; }
    int getPC() const { return m_s.pc.w.l; }
    void setPC(int v) { m_s.pc.w.l = static_cast<u16>(v); }
    int getAF() const { return m_s.af.w.l; }
    void setAF(int v) { m_s.af.w.l = static_cast<u16>(v); }
    int getBC() const { return m_s.bc.w.l; }
    int getDE() const { return m_s.de.w.l; }
    int getHL() const { return m_s.hl.w.l; }
    int getSP() const { return m_s.sp.w.l; }
    int getI() const { return m_s.i; }
    void setCarry(int c) { if (c) m_s.af.b.l |= CF; else m_s.af.b.l &= ~CF; }
    int getCarry() const { return m_s.af.b.l & CF; }
    void exAf();
    int getPop();

private:
    // Register byte accessors — return references into the Pair unions so
    // that reads and writes go directly to the register storage.
    u8& A() { return m_s.af.b.h; }
    u8& F() { return m_s.af.b.l; }
    u8& B() { return m_s.bc.b.h; }
    u8& C() { return m_s.bc.b.l; }
    u8& D() { return m_s.de.b.h; }
    u8& E() { return m_s.de.b.l; }
    u8& H() { return m_s.hl.b.h; }
    u8& L() { return m_s.hl.b.l; }

    // Register word (16-bit) accessors
    u16& AF() { return m_s.af.w.l; }
    u16& BC() { return m_s.bc.w.l; }
    u16& DE() { return m_s.de.w.l; }
    u16& HL() { return m_s.hl.w.l; }
    u16& SP() { return m_s.sp.w.l; }
    u16& PC() { return m_s.pc.w.l; }
    u16& WZ() { return m_s.wz.w.l; }
    u8& WZ_H() { return m_s.wz.b.h; }
    u8& WZ_L() { return m_s.wz.b.l; }

    // Memory / I/O access helpers — thin wrappers around the bus callbacks
    u8 rm(u16 addr) { return m_progRead(addr); }          // Read byte from memory
    void wm(u16 addr, u8 val) { m_progWrite(addr, val); } // Write byte to memory
    u8 rop();          // Fetch opcode byte at PC (advances PC)
    u8 arg();          // Fetch operand byte at PC (advances PC)
    u16 arg16();       // Fetch 16-bit operand (little-endian, advances PC by 2)
    u8 in(u16 port) { return m_ioRead(port); }           // Read from I/O port
    void out(u16 port, u8 val) { m_ioWrite(port, val); } // Write to I/O port
    void rm16(u16 addr, Pair* p);  // Read 16-bit value from memory into Pair
    void wm16(u16 addr, Pair* p);  // Write 16-bit value from Pair to memory
    void push(Pair& p);            // Push 16-bit register onto stack
    void pop(Pair& p);             // Pop 16-bit register from stack

    // Cycle accounting — subtract cycles from the remaining budget
    void eatCycles(int table, u8 op) { m_s.cyclesRemaining -= m_cc[table][op]; }
    void eatCyclesN(int n) { m_s.cyclesRemaining -= n; }

    // 8-bit register access by index, matching the Z80's r field encoding:
    //   0=B, 1=C, 2=D, 3=E, 4=H, 5=L, 6=(HL) [memory], 7=A
    u8 getReg8(int idx);
    void setReg8(int idx, u8 val);

    // 8-bit ALU — these operate on the accumulator (A) and update flags in F
    void aluAdd(u8 val);                 // A += val
    void aluAdc(u8 val);                 // A += val + carry
    void aluSub(u8 val);                 // A -= val
    void aluSbc(u8 val);                 // A -= val + carry
    void aluAnd(u8 val);                 // A &= val
    void aluXor(u8 val);                 // A ^= val
    void aluOr(u8 val);                  // A |= val
    void aluCp(u8 val);                  // Compare A - val (flags only, A unchanged)
    u8 aluInc(u8 val);                   // val + 1  (preserves carry flag)
    u8 aluDec(u8 val);                   // val - 1  (preserves carry flag)
    void aluDaa();                       // Decimal Adjust Accumulator (BCD correction)
    void aluNeg();                       // A = 0 - A (two's complement negate)
    void aluRlca();                      // Rotate A left, old bit 7 to carry and bit 0
    void aluRrca();                      // Rotate A right, old bit 0 to carry and bit 7
    void aluRla();                       // Rotate A left through carry
    void aluRra();                       // Rotate A right through carry
    void aluRrd();                       // Rotate right digit: A(low) <-> (HL)(low), (HL)(high) -> (HL)(low)
    void aluRld();                       // Rotate left digit: reverse of RRD
    void aluAdd16(Pair& dst, Pair& src); // dst += src (16-bit, only H/C/Y/X flags)
    void aluAdc16(Pair& src);            // HL += src + carry (full 16-bit flags)
    void aluSbc16(Pair& src);            // HL -= src - carry (full 16-bit flags)

    // CB-prefix shift/rotate/bit operations — return the result (caller stores it)
    u8 opRlc(u8 val);                    // Rotate left circular
    u8 opRrc(u8 val);                    // Rotate right circular
    u8 opRl(u8 val);                     // Rotate left through carry
    u8 opRr(u8 val);                     // Rotate right through carry
    u8 opSla(u8 val);                    // Shift left arithmetic (bit 0 = 0)
    u8 opSra(u8 val);                    // Shift right arithmetic (bit 7 preserved)
    u8 opSll(u8 val);                    // Shift left logical (undocumented: bit 0 = 1)
    u8 opSrl(u8 val);                    // Shift right logical (bit 7 = 0)
    void opBit(int bit, u8 val);         // Test bit — flags only, no result
    void opBitHl(int bit, u8 val);       // BIT on (HL) — undoc flags differ from register form
    void opBitXy(int bit, u8 val);       // BIT on (IX/IY+d) — undoc flags use WZ high byte

    // Block operations — single-step and auto-repeating variants
    // LDI/LDD: block copy  CPI/CPD: block search  INI/IND: block input  OUTI/OUTD: block output
    // R-suffixed versions (LDIR, etc.) repeat until BC==0 (or match found for CPIR/CPDR)
    void opLdi(); void opCpi(); void opIni(); void opOuti();
    void opLdd(); void opCpd(); void opInd(); void opOutd();
    void opLdir(); void opCpir(); void opInir(); void opOtir();
    void opLddr(); void opCpdr(); void opIndr(); void opOtdr();

    void opExsp(Pair& reg);   // EX (SP),rr — swap register with top-of-stack

    void takeInterrupt();     // Service a maskable interrupt (IM 0/1/2)

    // Opcode dispatch — one function per prefix group.  The main loop calls
    // execOp() for unprefixed opcodes; prefix bytes (CB/DD/ED/FD) dispatch
    // to their respective handlers.  DD and FD share execIndexed() with the
    // index register passed by reference, eliminating code duplication.
    void execOp(u8 opcode);                     // Unprefixed opcodes (256 cases)
    void execCb(u8 opcode);                     // CB prefix — decoded algorithmically
    void execEd(u8 opcode);                     // ED prefix — sparse switch
    void execIndexed(Pair& xy, u8 opcode);      // DD/FD — parameterized on IX or IY
    void execXyCb(u16 ea, u8 opcode);           // DDCB/FDCB — indexed bit ops

    // Cycle table indices — each prefix group has its own timing table.
    // TableEx holds extra cycles charged for taken branches and interrupt latency.
    enum CycleTable { TableOp = 0, TableCb, TableEd, TableXy, TableXyCb, TableEx };

    // Instance data
    State m_s{};
    ReadHandler m_ioRead = nullptr;
    WriteHandler m_ioWrite = nullptr;
    ReadHandler m_progRead = nullptr;
    WriteHandler m_progWrite = nullptr;
    ReadHandler m_opRead = nullptr;
    ReadHandler m_opArgRead = nullptr;
    const u8* m_cc[6]{};

    // Static cycle count tables (T-states per opcode, indexed by opcode byte)
    static const u8 s_ccOp[256];    // Unprefixed opcodes
    static const u8 s_ccCb[256];    // CB-prefixed opcodes
    static const u8 s_ccEd[256];    // ED-prefixed opcodes
    static const u8 s_ccXy[256];    // DD/FD-prefixed opcodes
    static const u8 s_ccXyCb[256];  // DDCB/FDCB-prefixed opcodes
    static const u8 s_ccEx[256];    // Extra cycles for taken branches / interrupt latency

    // Precomputed flag lookup tables (shared across all instances).  These
    // trade ~130 KB of memory for O(1) flag computation in every ALU operation.
    //   s_sz[r]       = SF|ZF|YF|XF flags for result r
    //   s_szBit[r]    = same but for BIT instruction (ZF|PF set together when zero)
    //   s_szp[r]      = s_sz[r] + parity flag
    //   s_szhvInc[r]  = flags for INC result r  (VF when 0x80, HF when low nibble wraps)
    //   s_szhvDec[r]  = flags for DEC result r  (VF when 0x7F, HF when low nibble wraps)
    //   s_szhvcAdd[i] = full flags for 8-bit add, indexed by (carry<<16 | oldA<<8 | result)
    //   s_szhvcSub[i] = full flags for 8-bit sub/cp, same indexing
    static u8 s_sz[256];
    static u8 s_szBit[256];
    static u8 s_szp[256];
    static u8 s_szhvInc[256];
    static u8 s_szhvDec[256];
    static u8* s_szhvcAdd;      // 2×256×256 = 128 KB (add without carry + adc with carry)
    static u8* s_szhvcSub;      // 2×256×256 = 128 KB (sub without carry + sbc with carry)
    static bool s_tablesInit;   // Guard: tables built on first instance construction
    static int s_instanceCount; // Reference count to free tables when last instance dies
};

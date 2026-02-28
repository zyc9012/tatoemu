#pragma once

#include <cstdint>
#include <cstdlib>
#include <cstring>

using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using s8  = int8_t;
using s16 = int16_t;
using s32 = int32_t;

// ---------------------------------------------------------------------------
// Sharp SM83 CPU emulator
//
// The SM83 (Sharp LR35902) is the 8-bit CPU used in the Game Boy and Game Boy
// Color.  It is loosely based on the Intel 8080 / Zilog Z80, but with a
// reduced instruction set: no IX/IY index registers, no I/O port space, no
// block or ED-prefix instructions.  Unique features include the SWAP nibble
// instruction and a simplified interrupt system with 5 fixed vectors governed
// by memory-mapped IF (0xFF0F) and IE (0xFFFF) registers.
// ---------------------------------------------------------------------------
class SM83 {
public:
    // F register flag bits:  Z N H C  (bits 7-4; bits 3-0 are always zero)
    //   Bit 7 (ZF) - Zero: set when result is zero
    //   Bit 6 (NF) - Subtract: set after subtraction operations
    //   Bit 5 (HF) - Half-carry: carry from bit 3->4 (addition) or borrow
    //   Bit 4 (CF) - Carry: carry out of bit 7
    static constexpr u8 ZF = 0x80;
    static constexpr u8 NF = 0x40;
    static constexpr u8 HF = 0x20;
    static constexpr u8 CF = 0x10;

    // Endian-aware register pair
    union Pair {
        u16 w = 0;
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        struct { u8 h, l; } b;
#else
        struct { u8 l, h; } b;
#endif
    };

    // CPU register state (saveable)
    struct Regs {
        Pair af, bc, de, hl;
        u16 sp = 0;
        u16 pc = 0;
        u8 ime = 0;            // Interrupt Master Enable
        u8 imeDelay = 0;       // EI delays IME by one instruction
        u8 halt = 0;           // HALT state
        u8 haltBug = 0;        // HALT bug: next fetch doesn't increment PC
        s32 cyclesRemaining = 0;
        s32 cyclesBudget = 0;
    };

    using ReadHandler  = u8  (*)(u16);
    using WriteHandler = void (*)(u16, u8);
    using StopHandler  = void (*)();

    SM83();
    ~SM83();

    void reset();
    int execute(int cycles);

    void setReadHandler(ReadHandler h)   { m_read = h; }
    void setWriteHandler(WriteHandler h) { m_write = h; }
    void setStopHandler(StopHandler h)   { m_stop = h; }

    // Save/restore entire CPU state
    void getContext(void* dst) const { if (dst) std::memcpy(dst, &m_r, sizeof(m_r)); }
    void setContext(const void* src)  { if (src) std::memcpy(&m_r, src, sizeof(m_r)); }
    static constexpr size_t contextSize() { return sizeof(Regs); }

    // Cycle accounting
    s32 totalCycles() const { return m_r.cyclesBudget - m_r.cyclesRemaining; }

    // Register accessors
    int getPC() const { return m_r.pc; }
    void setPC(int v) { m_r.pc = static_cast<u16>(v); }
    int getSP() const { return m_r.sp; }
    void setSP(int v) { m_r.sp = static_cast<u16>(v); }
    int getAF() const { return m_r.af.w; }
    void setAF(int v) { m_r.af.w = static_cast<u16>(v) & 0xFFF0; }
    int getBC() const { return m_r.bc.w; }
    void setBC(int v) { m_r.bc.w = static_cast<u16>(v); }
    int getDE() const { return m_r.de.w; }
    void setDE(int v) { m_r.de.w = static_cast<u16>(v); }
    int getHL() const { return m_r.hl.w; }
    void setHL(int v) { m_r.hl.w = static_cast<u16>(v); }

    bool isHalted() const { return m_r.halt != 0; }
    void setIME(bool v) { m_r.ime = v ? 1 : 0; }
    bool getIME() const { return m_r.ime != 0; }

private:
    // Register byte accessors
    u8& A() { return m_r.af.b.h; }
    u8& F() { return m_r.af.b.l; }
    u8& B() { return m_r.bc.b.h; }
    u8& C() { return m_r.bc.b.l; }
    u8& D() { return m_r.de.b.h; }
    u8& E() { return m_r.de.b.l; }
    u8& H() { return m_r.hl.b.h; }
    u8& L() { return m_r.hl.b.l; }

    // Register word accessors
    u16& AF() { return m_r.af.w; }
    u16& BC() { return m_r.bc.w; }
    u16& DE() { return m_r.de.w; }
    u16& HL() { return m_r.hl.w; }
    u16& SP() { return m_r.sp; }
    u16& PC() { return m_r.pc; }

    // Memory access - thin wrappers around bus callbacks
    u8  rd(u16 addr) { return m_read(addr); }
    void wr(u16 addr, u8 val) { m_write(addr, val); }

    u8  fetch8();       // Fetch byte at PC (advances PC, respects HALT bug)
    u16 fetch16();      // Fetch 16-bit little-endian at PC (advances PC by 2)
    void push(u16 val); // Push 16-bit value onto stack
    u16 pop();          // Pop 16-bit value from stack

    // Flag helpers
    void setFlag(u8 flag, bool val);
    bool getFlag(u8 flag) const;

    // Interrupt handling - reads IF/IE from memory via callbacks
    int handleInterrupts();

    // Opcode dispatch
    int execOp(u8 opcode);   // Unprefixed opcodes (256 entries)
    int execCb(u8 opcode);   // CB-prefixed opcodes (256 entries)

    // Instance data
    Regs m_r{};
    ReadHandler  m_read  = nullptr;
    WriteHandler m_write = nullptr;
    StopHandler  m_stop  = nullptr;
};


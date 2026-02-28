#include "z80.h"

// ---------------------------------------------------------------------------
// Static member definitions
// ---------------------------------------------------------------------------

u8 Z80::s_sz[256];
u8 Z80::s_szBit[256];
u8 Z80::s_szp[256];
u8 Z80::s_szhvInc[256];
u8 Z80::s_szhvDec[256];
u8* Z80::s_szhvcAdd = nullptr;
u8* Z80::s_szhvcSub = nullptr;
bool Z80::s_tablesInit = false;
int Z80::s_instanceCount = 0;

const u8 Z80::s_ccOp[256] = {
  4,10, 7, 6, 4, 4, 7, 4, 4,11, 7, 6, 4, 4, 7, 4,
  8,10, 7, 6, 4, 4, 7, 4,12,11, 7, 6, 4, 4, 7, 4,
  7,10,16, 6, 4, 4, 7, 4, 7,11,16, 6, 4, 4, 7, 4,
  7,10,13, 6,11,11,10, 4, 7,11,13, 6, 4, 4, 7, 4,
  4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4,
  4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4,
  4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4,
  7, 7, 7, 7, 7, 7, 4, 7, 4, 4, 4, 4, 4, 4, 7, 4,
  4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4,
  4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4,
  4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4,
  4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4,
  5,10,10,10,10,11, 7,11, 5,10,10, 0,10,17, 7,11,
  5,10,10,11,10,11, 7,11, 5, 4,10,11,10, 0, 7,11,
  5,10,10,19,10,11, 7,11, 5, 4,10, 4,10, 0, 7,11,
  5,10,10, 4,10,11, 7,11, 5, 6,10, 4,10, 0, 7,11
};

const u8 Z80::s_ccCb[256] = {
  8, 8, 8, 8, 8, 8,15, 8, 8, 8, 8, 8, 8, 8,15, 8,
  8, 8, 8, 8, 8, 8,15, 8, 8, 8, 8, 8, 8, 8,15, 8,
  8, 8, 8, 8, 8, 8,15, 8, 8, 8, 8, 8, 8, 8,15, 8,
  8, 8, 8, 8, 8, 8,15, 8, 8, 8, 8, 8, 8, 8,15, 8,
  8, 8, 8, 8, 8, 8,12, 8, 8, 8, 8, 8, 8, 8,12, 8,
  8, 8, 8, 8, 8, 8,12, 8, 8, 8, 8, 8, 8, 8,12, 8,
  8, 8, 8, 8, 8, 8,12, 8, 8, 8, 8, 8, 8, 8,12, 8,
  8, 8, 8, 8, 8, 8,12, 8, 8, 8, 8, 8, 8, 8,12, 8,
  8, 8, 8, 8, 8, 8,15, 8, 8, 8, 8, 8, 8, 8,15, 8,
  8, 8, 8, 8, 8, 8,15, 8, 8, 8, 8, 8, 8, 8,15, 8,
  8, 8, 8, 8, 8, 8,15, 8, 8, 8, 8, 8, 8, 8,15, 8,
  8, 8, 8, 8, 8, 8,15, 8, 8, 8, 8, 8, 8, 8,15, 8,
  8, 8, 8, 8, 8, 8,15, 8, 8, 8, 8, 8, 8, 8,15, 8,
  8, 8, 8, 8, 8, 8,15, 8, 8, 8, 8, 8, 8, 8,15, 8,
  8, 8, 8, 8, 8, 8,15, 8, 8, 8, 8, 8, 8, 8,15, 8,
  8, 8, 8, 8, 8, 8,15, 8, 8, 8, 8, 8, 8, 8,15, 8
};

const u8 Z80::s_ccEd[256] = {
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
 12,12,15,20, 8,14, 8, 9,12,12,15,20, 8,14, 8, 9,
 12,12,15,20, 8,14, 8, 9,12,12,15,20, 8,14, 8, 9,
 12,12,15,20, 8,14, 8,18,12,12,15,20, 8,14, 8,18,
 12,12,15,20, 8,14, 8, 8,12,12,15,20, 8,14, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
 16,16,16,16, 8, 8, 8, 8,16,16,16,16, 8, 8, 8, 8,
 16,16,16,16, 8, 8, 8, 8,16,16,16,16, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8
};

const u8 Z80::s_ccXy[256] = {
  8,14,11,10, 8, 8,11, 8, 8,15,11,10, 8, 8,11, 8,
 12,14,11,10, 8, 8,11, 8,16,15,11,10, 8, 8,11, 8,
 11,14,20,10, 8, 8,11, 8,11,15,20,10, 8, 8,11, 8,
 11,14,17,10,23,23,19, 8,11,15,17,10, 8, 8,11, 8,
  8, 8, 8, 8, 8, 8,19, 8, 8, 8, 8, 8, 8, 8,19, 8,
  8, 8, 8, 8, 8, 8,19, 8, 8, 8, 8, 8, 8, 8,19, 8,
  8, 8, 8, 8, 8, 8,19, 8, 8, 8, 8, 8, 8, 8,19, 8,
 19,19,19,19,19,19, 8,19, 8, 8, 8, 8, 8, 8,19, 8,
  8, 8, 8, 8, 8, 8,19, 8, 8, 8, 8, 8, 8, 8,19, 8,
  8, 8, 8, 8, 8, 8,19, 8, 8, 8, 8, 8, 8, 8,19, 8,
  8, 8, 8, 8, 8, 8,19, 8, 8, 8, 8, 8, 8, 8,19, 8,
  8, 8, 8, 8, 8, 8,19, 8, 8, 8, 8, 8, 8, 8,19, 8,
  9,14,14,14,14,15,11,15, 9,14,14, 0,14,21,11,15,
  9,14,14,15,14,15,11,15, 9, 8,14,15,14, 4,11,15,
  9,14,14,23,14,15,11,15, 9, 8,14, 8,14, 4,11,15,
  9,14,14, 8,14,15,11,15, 9,10,14, 8,14, 4,11,15
};

const u8 Z80::s_ccXyCb[256] = {
 23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,
 23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,
 23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,
 23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,
 20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,
 20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,
 20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,
 20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,
 23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,
 23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,
 23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,
 23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,
 23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,
 23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,
 23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,
 23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23
};

// Extra cycles if jr/jp/call taken and interrupt latency on rst 0-7
const u8 Z80::s_ccEx[256] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  5, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0,
  5, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  5, 5, 5, 5, 0, 0, 0, 0, 5, 5, 5, 5, 0, 0, 0, 0,
  6, 0, 0, 0, 7, 0, 0, 2, 6, 0, 0, 0, 7, 0, 0, 2,
  6, 0, 0, 0, 7, 0, 0, 2, 6, 0, 0, 0, 7, 0, 0, 2,
  6, 0, 0, 0, 7, 0, 0, 2, 6, 0, 0, 0, 7, 0, 0, 2,
  6, 0, 0, 0, 7, 0, 0, 2, 6, 0, 0, 0, 7, 0, 0, 2
};

// ---------------------------------------------------------------------------
// Construction, destruction, and flag table generation
// ---------------------------------------------------------------------------

Z80::Z80() {
    s_instanceCount++;
    init();
}

Z80::~Z80() {
    s_instanceCount--;
    if (s_instanceCount <= 0) {
        if (s_szhvcAdd) { std::free(s_szhvcAdd); s_szhvcAdd = nullptr; }
        if (s_szhvcSub) { std::free(s_szhvcSub); s_szhvcSub = nullptr; }
        s_tablesInit = false;
        s_instanceCount = 0;
    }
}

void Z80::init() {
    // Wire up the per-instance cycle table pointers to the static arrays
    m_cc[TableOp] = s_ccOp;
    m_cc[TableCb] = s_ccCb;
    m_cc[TableEd] = s_ccEd;
    m_cc[TableXy] = s_ccXy;
    m_cc[TableXyCb] = s_ccXyCb;
    m_cc[TableEx] = s_ccEx;

    if (!s_tablesInit) {
        // Allocate big flag arrays for ADD/ADC/SUB/SBC
        if (!s_szhvcAdd) s_szhvcAdd = static_cast<u8*>(std::malloc(2 * 256 * 256));
        if (!s_szhvcSub) s_szhvcSub = static_cast<u8*>(std::malloc(2 * 256 * 256));

        u8* padd = &s_szhvcAdd[0];
        u8* padc = &s_szhvcAdd[256 * 256];
        u8* psub = &s_szhvcSub[0];
        u8* psbc = &s_szhvcSub[256 * 256];

        for (int oldval = 0; oldval < 256; oldval++) {
            for (int newval = 0; newval < 256; newval++) {
                // add or adc w/o carry set
                int val = newval - oldval;
                *padd = (newval) ? ((newval & 0x80) ? SF : 0) : ZF;
                *padd |= (newval & (YF | XF));
                if ((newval & 0x0f) < (oldval & 0x0f)) *padd |= HF;
                if (newval < oldval) *padd |= CF;
                if ((val ^ oldval ^ 0x80) & (val ^ newval) & 0x80) *padd |= VF;
                padd++;

                // adc with carry set
                val = newval - oldval - 1;
                *padc = (newval) ? ((newval & 0x80) ? SF : 0) : ZF;
                *padc |= (newval & (YF | XF));
                if ((newval & 0x0f) <= (oldval & 0x0f)) *padc |= HF;
                if (newval <= oldval) *padc |= CF;
                if ((val ^ oldval ^ 0x80) & (val ^ newval) & 0x80) *padc |= VF;
                padc++;

                // cp, sub or sbc w/o carry set
                val = oldval - newval;
                *psub = NF | ((newval) ? ((newval & 0x80) ? SF : 0) : ZF);
                *psub |= (newval & (YF | XF));
                if ((newval & 0x0f) > (oldval & 0x0f)) *psub |= HF;
                if (newval > oldval) *psub |= CF;
                if ((val ^ oldval) & (oldval ^ newval) & 0x80) *psub |= VF;
                psub++;

                // sbc with carry set
                val = oldval - newval - 1;
                *psbc = NF | ((newval) ? ((newval & 0x80) ? SF : 0) : ZF);
                *psbc |= (newval & (YF | XF));
                if ((newval & 0x0f) >= (oldval & 0x0f)) *psbc |= HF;
                if (newval >= oldval) *psbc |= CF;
                if ((val ^ oldval) & (oldval ^ newval) & 0x80) *psbc |= VF;
                psbc++;
            }
        }

        // Build SZ, SZ_BIT, SZP, SZHV_inc, SZHV_dec tables
        for (int i = 0; i < 256; i++) {
            int p = 0;
            if (i & 0x01) ++p; if (i & 0x02) ++p; if (i & 0x04) ++p; if (i & 0x08) ++p;
            if (i & 0x10) ++p; if (i & 0x20) ++p; if (i & 0x40) ++p; if (i & 0x80) ++p;

            s_sz[i] = i ? (i & SF) : ZF;
            s_sz[i] |= (i & (YF | XF));
            s_szBit[i] = i ? (i & SF) : (ZF | PF);
            s_szBit[i] |= (i & (YF | XF));
            s_szp[i] = s_sz[i] | ((p & 1) ? 0 : PF);

            s_szhvInc[i] = s_sz[i];
            if (i == 0x80) s_szhvInc[i] |= VF;
            if ((i & 0x0f) == 0x00) s_szhvInc[i] |= HF;

            s_szhvDec[i] = s_sz[i] | NF;
            if (i == 0x7f) s_szhvDec[i] |= VF;
            if ((i & 0x0f) == 0x0f) s_szhvDec[i] |= HF;
        }

        s_tablesInit = true;
    }

    // Reset registers
    std::memset(&m_s, 0, sizeof(m_s));
    m_s.ix.w.l = 0xffff;
    m_s.iy.w.l = 0xffff;
    m_s.af.b.l = ZF;
    m_s.vector = 0xff;
}

void Z80::reset() {
    // Preserve handler pointers - only reset CPU state
    State clean{};
    clean.nmiState = ClearLine;
    clean.irqState = ClearLine;
    clean.ix.w.l = 0xffff;
    clean.iy.w.l = 0xffff;
    clean.vector = 0xff;
    m_s = clean;
}

// ---------------------------------------------------------------------------
// Memory access, stack operations, and register helpers
// ---------------------------------------------------------------------------

u8 Z80::rop() {
    u16 pc = PC();
    PC()++;
    return m_mem.opRead(pc, m_mem.userData);
}

u8 Z80::arg() {
    u16 pc = PC();
    PC()++;
    return m_mem.opArgRead(pc, m_mem.userData);
}

u16 Z80::arg16() {
    u16 pc = PC();
    PC() += 2;
    u8 lo = m_mem.opArgRead(pc, m_mem.userData);
    u8 hi = m_mem.opArgRead((pc + 1) & 0xffff, m_mem.userData);
    return lo | (hi << 8);
}

void Z80::rm16(u16 addr, Pair* p) {
    p->b.l = rm(addr);
    p->b.h = rm((addr + 1) & 0xffff);
}

void Z80::wm16(u16 addr, Pair* p) {
    wm(addr, p->b.l);
    wm((addr + 1) & 0xffff, p->b.h);
}

void Z80::push(Pair& p) {
    SP()--;
    wm(SP(), p.b.h);
    SP()--;
    wm(SP(), p.b.l);
}

void Z80::pop(Pair& p) {
    rm16(SP(), &p);
    SP() += 2;
}

u8 Z80::getReg8(int idx) {
    switch (idx) {
        case 0: return B(); case 1: return C();
        case 2: return D(); case 3: return E();
        case 4: return H(); case 5: return L();
        case 6: return rm(HL());
        case 7: return A();
        default: return 0;
    }
}

void Z80::setReg8(int idx, u8 val) {
    switch (idx) {
        case 0: B() = val; break; case 1: C() = val; break;
        case 2: D() = val; break; case 3: E() = val; break;
        case 4: H() = val; break; case 5: L() = val; break;
        case 6: wm(HL(), val); break; case 7: A() = val; break;
    }
}

void Z80::opExsp(Pair& reg) {
    Pair tmp{};
    rm16(SP(), &tmp);
    wm16(SP(), &reg);
    reg = tmp;
    WZ() = reg.w.l;
}

// ---------------------------------------------------------------------------
// 8-bit ALU operations
// ---------------------------------------------------------------------------

void Z80::aluAdd(u8 val) {
    u32 ah = m_s.af.d & 0xff00;                 // old A in bits 15..8
    u32 res = static_cast<u8>((ah >> 8) + val); // 8-bit result
    F() = s_szhvcAdd[ah | res];                 // all flags from lookup
    A() = static_cast<u8>(res);
}

void Z80::aluAdc(u8 val) {
    u32 ah = m_s.af.d & 0xff00;
    u32 c = m_s.af.d & 1;
    u32 res = static_cast<u8>((ah >> 8) + val + c);
    F() = s_szhvcAdd[(c << 16) | ah | res];
    A() = static_cast<u8>(res);
}

void Z80::aluSub(u8 val) {
    u32 ah = m_s.af.d & 0xff00;
    u32 res = static_cast<u8>((ah >> 8) - val);
    F() = s_szhvcSub[ah | res];
    A() = static_cast<u8>(res);
}

void Z80::aluSbc(u8 val) {
    u32 ah = m_s.af.d & 0xff00;
    u32 c = m_s.af.d & 1;
    u32 res = static_cast<u8>((ah >> 8) - val - c);
    F() = s_szhvcSub[(c << 16) | ah | res];
    A() = static_cast<u8>(res);
}

void Z80::aluAnd(u8 val) {
    A() &= val;
    F() = s_szp[A()] | HF;
}

void Z80::aluXor(u8 val) {
    A() ^= val;
    F() = s_szp[A()];
}

void Z80::aluOr(u8 val) {
    A() |= val;
    F() = s_szp[A()];
}

void Z80::aluCp(u8 val) {
    u32 ah = m_s.af.d & 0xff00;
    u32 res = static_cast<u8>((ah >> 8) - val);
    F() = (s_szhvcSub[ah | res] & ~(YF | XF)) | (val & (YF | XF));
}

u8 Z80::aluInc(u8 val) {
    u8 res = val + 1;
    F() = (F() & CF) | s_szhvInc[res];
    return res;
}

u8 Z80::aluDec(u8 val) {
    u8 res = val - 1;
    F() = (F() & CF) | s_szhvDec[res];
    return res;
}

void Z80::aluDaa() {
    u8 a = A();
    if (F() & NF) {
        if ((F() & HF) | ((A() & 0xf) > 9)) a -= 6;
        if ((F() & CF) | (A() > 0x99)) a -= 0x60;
    } else {
        if ((F() & HF) | ((A() & 0xf) > 9)) a += 6;
        if ((F() & CF) | (A() > 0x99)) a += 0x60;
    }
    F() = (F() & (CF | NF)) | (A() > 0x99) | ((A() ^ a) & HF) | s_szp[a];
    A() = a;
}

void Z80::aluNeg() {
    u8 val = A();
    A() = 0;
    aluSub(val);
}

// --- Rotate/shift on accumulator, RRD/RLD, and 16-bit ALU ---

void Z80::aluRlca() {
    A() = (A() << 1) | (A() >> 7);
    F() = (F() & (SF | ZF | PF)) | (A() & (YF | XF | CF));
}

void Z80::aluRrca() {
    F() = (F() & (SF | ZF | PF)) | (A() & CF);
    A() = (A() >> 1) | (A() << 7);
    F() |= (A() & (YF | XF));
}

void Z80::aluRla() {
    u8 res = (A() << 1) | (F() & CF);
    u8 c = (A() & 0x80) ? CF : 0;
    F() = (F() & (SF | ZF | PF)) | c | (res & (YF | XF));
    A() = res;
}

void Z80::aluRra() {
    u8 res = (A() >> 1) | (F() << 7);
    u8 c = (A() & 0x01) ? CF : 0;
    F() = (F() & (SF | ZF | PF)) | c | (res & (YF | XF));
    A() = res;
}

void Z80::aluRrd() {
    u8 n = rm(HL());
    WZ() = HL() + 1;
    wm(HL(), (n >> 4) | (A() << 4));
    A() = (A() & 0xf0) | (n & 0x0f);
    F() = (F() & CF) | s_szp[A()];
}

void Z80::aluRld() {
    u8 n = rm(HL());
    WZ() = HL() + 1;
    wm(HL(), (n << 4) | (A() & 0x0f));
    A() = (A() & 0xf0) | (n >> 4);
    F() = (F() & CF) | s_szp[A()];
}

void Z80::aluAdd16(Pair& dst, Pair& src) {
    u32 res = dst.d + src.d;
    WZ() = dst.w.l + 1;
    F() = (F() & (SF | ZF | VF)) |
           (((dst.d ^ res ^ src.d) >> 8) & HF) |
           ((res >> 16) & CF) | ((res >> 8) & (YF | XF));
    dst.w.l = static_cast<u16>(res);
}

void Z80::aluAdc16(Pair& src) {
    u32 res = m_s.hl.d + src.d + (F() & CF);
    WZ() = HL() + 1;
    F() = (((m_s.hl.d ^ res ^ src.d) >> 8) & HF) |
           ((res >> 16) & CF) |
           ((res >> 8) & (SF | YF | XF)) |
           ((res & 0xffff) ? 0 : ZF) |
           (((src.d ^ m_s.hl.d ^ 0x8000) & (src.d ^ res) & 0x8000) >> 13);
    HL() = static_cast<u16>(res);
}

void Z80::aluSbc16(Pair& src) {
    u32 res = m_s.hl.d - src.d - (F() & CF);
    WZ() = HL() + 1;
    F() = (((m_s.hl.d ^ res ^ src.d) >> 8) & HF) | NF |
           ((res >> 16) & CF) |
           ((res >> 8) & (SF | YF | XF)) |
           ((res & 0xffff) ? 0 : ZF) |
           (((src.d ^ m_s.hl.d) & (m_s.hl.d ^ res) & 0x8000) >> 13);
    HL() = static_cast<u16>(res);
}

// --- CB-prefix shift/rotate ops ---

u8 Z80::opRlc(u8 val) {
    u8 c = (val & 0x80) ? CF : 0;
    u8 res = ((val << 1) | (val >> 7)) & 0xff;
    F() = s_szp[res] | c;
    return res;
}

u8 Z80::opRrc(u8 val) {
    u8 c = (val & 0x01) ? CF : 0;
    u8 res = ((val >> 1) | (val << 7)) & 0xff;
    F() = s_szp[res] | c;
    return res;
}

u8 Z80::opRl(u8 val) {
    u8 c = (val & 0x80) ? CF : 0;
    u8 res = ((val << 1) | (F() & CF)) & 0xff;
    F() = s_szp[res] | c;
    return res;
}

u8 Z80::opRr(u8 val) {
    u8 c = (val & 0x01) ? CF : 0;
    u8 res = ((val >> 1) | (F() << 7)) & 0xff;
    F() = s_szp[res] | c;
    return res;
}

u8 Z80::opSla(u8 val) {
    u8 c = (val & 0x80) ? CF : 0;
    u8 res = (val << 1) & 0xff;
    F() = s_szp[res] | c;
    return res;
}

u8 Z80::opSra(u8 val) {
    u8 c = (val & 0x01) ? CF : 0;
    u8 res = ((val >> 1) | (val & 0x80)) & 0xff;
    F() = s_szp[res] | c;
    return res;
}

u8 Z80::opSll(u8 val) {
    u8 c = (val & 0x80) ? CF : 0;
    u8 res = ((val << 1) | 0x01) & 0xff;
    F() = s_szp[res] | c;
    return res;
}

u8 Z80::opSrl(u8 val) {
    u8 c = (val & 0x01) ? CF : 0;
    u8 res = (val >> 1) & 0xff;
    F() = s_szp[res] | c;
    return res;
}

void Z80::opBit(int bit, u8 val) {
    F() = (F() & CF) | HF | (s_szBit[val & (1 << bit)] & ~(YF | XF)) | (val & (YF | XF));
}

void Z80::opBitHl(int bit, u8 val) {
    F() = (F() & CF) | HF | (s_szBit[val & (1 << bit)] & ~(YF | XF)) | (WZ_H() & (YF | XF));
}

void Z80::opBitXy(int bit, u8 val) {
    F() = (F() & CF) | HF | (s_szBit[val & (1 << bit)] & ~(YF | XF)) | (WZ_H() & (YF | XF));
}

// --- Block operations ---

void Z80::opLdi() {
    u8 io = rm(HL());
    wm(DE(), io);
    F() &= SF | ZF | CF;
    if ((A() + io) & 0x02) F() |= YF;
    if ((A() + io) & 0x08) F() |= XF;
    HL()++; DE()++; BC()--;
    if (BC()) F() |= VF;
}

void Z80::opCpi() {
    u8 val = rm(HL());
    u8 res = A() - val;
    WZ()++;
    HL()++; BC()--;
    F() = (F() & CF) | (s_sz[res] & ~(YF | XF)) | ((A() ^ val ^ res) & HF) | NF;
    if (F() & HF) res -= 1;
    if (res & 0x02) F() |= YF;
    if (res & 0x08) F() |= XF;
    if (BC()) F() |= VF;
}

void Z80::opIni() {
    u8 io = in(BC());
    WZ() = BC() + 1;
    B()--;
    wm(HL(), io);
    HL()++;
    F() = s_sz[B()];
    unsigned t = (static_cast<unsigned>(C() + 1) & 0xff) + static_cast<unsigned>(io);
    if (io & SF) F() |= NF;
    if (t & 0x100) F() |= HF | CF;
    F() |= s_szp[static_cast<u8>(t & 0x07) ^ B()] & PF;
}

void Z80::opOuti() {
    u8 io = rm(HL());
    B()--;
    WZ() = BC() + 1;
    out(BC(), io);
    HL()++;
    F() = s_sz[B()];
    unsigned t = static_cast<unsigned>(L()) + static_cast<unsigned>(io);
    if (io & SF) F() |= NF;
    if (t & 0x100) F() |= HF | CF;
    F() |= s_szp[static_cast<u8>(t & 0x07) ^ B()] & PF;
}

void Z80::opLdd() {
    u8 io = rm(HL());
    wm(DE(), io);
    F() &= SF | ZF | CF;
    if ((A() + io) & 0x02) F() |= YF;
    if ((A() + io) & 0x08) F() |= XF;
    HL()--; DE()--; BC()--;
    if (BC()) F() |= VF;
}

void Z80::opCpd() {
    u8 val = rm(HL());
    u8 res = A() - val;
    WZ()--;
    HL()--; BC()--;
    F() = (F() & CF) | (s_sz[res] & ~(YF | XF)) | ((A() ^ val ^ res) & HF) | NF;
    if (F() & HF) res -= 1;
    if (res & 0x02) F() |= YF;
    if (res & 0x08) F() |= XF;
    if (BC()) F() |= VF;
}

void Z80::opInd() {
    u8 io = in(BC());
    WZ() = BC() - 1;
    B()--;
    wm(HL(), io);
    HL()--;
    F() = s_sz[B()];
    unsigned t = (static_cast<unsigned>(C() - 1) & 0xff) + static_cast<unsigned>(io);
    if (io & SF) F() |= NF;
    if (t & 0x100) F() |= HF | CF;
    F() |= s_szp[static_cast<u8>(t & 0x07) ^ B()] & PF;
}

void Z80::opOutd() {
    u8 io = rm(HL());
    B()--;
    WZ() = BC() - 1;
    out(BC(), io);
    HL()--;
    F() = s_sz[B()];
    unsigned t = static_cast<unsigned>(L()) + static_cast<unsigned>(io);
    if (io & SF) F() |= NF;
    if (t & 0x100) F() |= HF | CF;
    F() |= s_szp[static_cast<u8>(t & 0x07) ^ B()] & PF;
}

void Z80::opLdir() {
    opLdi();
    if (BC()) { eatCycles(TableEx, 0xb0); PC() -= 2; WZ() = PC() + 1; }
}

void Z80::opCpir() {
    opCpi();
    if (BC() && !(F() & ZF)) { eatCycles(TableEx, 0xb1); PC() -= 2; WZ() = PC() + 1; }
}

void Z80::opInir() {
    opIni();
    if (B()) { eatCycles(TableEx, 0xb2); PC() -= 2; }
}

void Z80::opOtir() {
    opOuti();
    if (B()) { eatCycles(TableEx, 0xb3); PC() -= 2; }
}

void Z80::opLddr() {
    opLdd();
    if (BC()) { eatCycles(TableEx, 0xb8); PC() -= 2; WZ() = PC() + 1; }
}

void Z80::opCpdr() {
    opCpd();
    if (BC() && !(F() & ZF)) { eatCycles(TableEx, 0xb9); PC() -= 2; WZ() = PC() + 1; }
}

void Z80::opIndr() {
    opInd();
    if (B()) { eatCycles(TableEx, 0xba); PC() -= 2; }
}

void Z80::opOtdr() {
    opOutd();
    if (B()) { eatCycles(TableEx, 0xbb); PC() -= 2; }
}

// --- CB prefix opcodes (compact algorithmic dispatch) ---

void Z80::execCb(u8 opcode) {
    u8 group = opcode >> 6;    // 0=shift, 1=BIT, 2=RES, 3=SET
    u8 bitN = (opcode >> 3) & 7;
    u8 reg = opcode & 7;

    u8 val = getReg8(reg);

    switch (group) {
        case 0: { // Shift/rotate
            u8 result;
            switch (bitN) {
                case 0: result = opRlc(val); break;
                case 1: result = opRrc(val); break;
                case 2: result = opRl(val); break;
                case 3: result = opRr(val); break;
                case 4: result = opSla(val); break;
                case 5: result = opSra(val); break;
                case 6: result = opSll(val); break;
                default: result = opSrl(val); break;
            }
            setReg8(reg, result);
            break;
        }
        case 1: // BIT
            if (reg == 6) opBitHl(bitN, val);
            else opBit(bitN, val);
            break;
        case 2: // RES
            setReg8(reg, val & ~(1 << bitN));
            break;
        case 3: // SET
            setReg8(reg, val | (1 << bitN));
            break;
    }
}

// --- XYCB prefix opcodes (IX/IY + displacement + CB) ---

void Z80::execXyCb(u16 ea, u8 opcode) {
    u8 group = opcode >> 6;
    u8 bitN = (opcode >> 3) & 7;
    u8 reg = opcode & 7;
    u8 val = rm(ea);

    switch (group) {
        case 0: { // Shift/rotate
            u8 result;
            switch (bitN) {
                case 0: result = opRlc(val); break;
                case 1: result = opRrc(val); break;
                case 2: result = opRl(val); break;
                case 3: result = opRr(val); break;
                case 4: result = opSla(val); break;
                case 5: result = opSra(val); break;
                case 6: result = opSll(val); break;
                default: result = opSrl(val); break;
            }
            wm(ea, result);
            if (reg != 6) setReg8(reg, result);
            break;
        }
        case 1: // BIT
            opBitXy(bitN, val);
            return; // no write-back for BIT
        case 2: { // RES
            u8 result = val & ~(1 << bitN);
            wm(ea, result);
            if (reg != 6) setReg8(reg, result);
            break;
        }
        case 3: { // SET
            u8 result = val | (1 << bitN);
            wm(ea, result);
            if (reg != 6) setReg8(reg, result);
            break;
        }
    }
}

// --- DD/FD prefix opcodes (combined IX/IY dispatch) ---

void Z80::execIndexed(Pair& xy, u8 opcode) {
    switch (opcode) {
        case 0x09: aluAdd16(xy, m_s.bc); break;
        case 0x19: aluAdd16(xy, m_s.de); break;
        case 0x21: xy.w.l = arg16(); break;
        case 0x22: { u16 ea = arg16(); wm16(ea, &xy); WZ() = ea + 1; break; }
        case 0x23: xy.w.l++; break;
        case 0x24: xy.b.h = aluInc(xy.b.h); break;
        case 0x25: xy.b.h = aluDec(xy.b.h); break;
        case 0x26: xy.b.h = arg(); break;
        case 0x29: aluAdd16(xy, xy); break;
        case 0x2a: { u16 ea = arg16(); rm16(ea, &xy); WZ() = ea + 1; break; }
        case 0x2b: xy.w.l--; break;
        case 0x2c: xy.b.l = aluInc(xy.b.l); break;
        case 0x2d: xy.b.l = aluDec(xy.b.l); break;
        case 0x2e: xy.b.l = arg(); break;
        case 0x34: { u16 ea = static_cast<u16>(xy.w.l + static_cast<s8>(arg())); WZ() = ea; wm(ea, aluInc(rm(ea))); break; }
        case 0x35: { u16 ea = static_cast<u16>(xy.w.l + static_cast<s8>(arg())); WZ() = ea; wm(ea, aluDec(rm(ea))); break; }
        case 0x36: { u16 ea = static_cast<u16>(xy.w.l + static_cast<s8>(arg())); WZ() = ea; wm(ea, arg()); break; }
        case 0x39: aluAdd16(xy, m_s.sp); break;
        case 0x44: B() = xy.b.h; break;
        case 0x45: B() = xy.b.l; break;
        case 0x46: { u16 ea = static_cast<u16>(xy.w.l + static_cast<s8>(arg())); WZ() = ea; B() = rm(ea); break; }
        case 0x4c: C() = xy.b.h; break;
        case 0x4d: C() = xy.b.l; break;
        case 0x4e: { u16 ea = static_cast<u16>(xy.w.l + static_cast<s8>(arg())); WZ() = ea; C() = rm(ea); break; }
        case 0x54: D() = xy.b.h; break;
        case 0x55: D() = xy.b.l; break;
        case 0x56: { u16 ea = static_cast<u16>(xy.w.l + static_cast<s8>(arg())); WZ() = ea; D() = rm(ea); break; }
        case 0x5c: E() = xy.b.h; break;
        case 0x5d: E() = xy.b.l; break;
        case 0x5e: { u16 ea = static_cast<u16>(xy.w.l + static_cast<s8>(arg())); WZ() = ea; E() = rm(ea); break; }
        case 0x60: xy.b.h = B(); break;
        case 0x61: xy.b.h = C(); break;
        case 0x62: xy.b.h = D(); break;
        case 0x63: xy.b.h = E(); break;
        case 0x64: break; // LD IXH,IXH (nop)
        case 0x65: xy.b.h = xy.b.l; break;
        case 0x66: { u16 ea = static_cast<u16>(xy.w.l + static_cast<s8>(arg())); WZ() = ea; H() = rm(ea); break; }
        case 0x67: xy.b.h = A(); break;
        case 0x68: xy.b.l = B(); break;
        case 0x69: xy.b.l = C(); break;
        case 0x6a: xy.b.l = D(); break;
        case 0x6b: xy.b.l = E(); break;
        case 0x6c: xy.b.l = xy.b.h; break;
        case 0x6d: break; // LD IXL,IXL (nop)
        case 0x6e: { u16 ea = static_cast<u16>(xy.w.l + static_cast<s8>(arg())); WZ() = ea; L() = rm(ea); break; }
        case 0x6f: xy.b.l = A(); break;
        case 0x70: { u16 ea = static_cast<u16>(xy.w.l + static_cast<s8>(arg())); WZ() = ea; wm(ea, B()); break; }
        case 0x71: { u16 ea = static_cast<u16>(xy.w.l + static_cast<s8>(arg())); WZ() = ea; wm(ea, C()); break; }
        case 0x72: { u16 ea = static_cast<u16>(xy.w.l + static_cast<s8>(arg())); WZ() = ea; wm(ea, D()); break; }
        case 0x73: { u16 ea = static_cast<u16>(xy.w.l + static_cast<s8>(arg())); WZ() = ea; wm(ea, E()); break; }
        case 0x74: { u16 ea = static_cast<u16>(xy.w.l + static_cast<s8>(arg())); WZ() = ea; wm(ea, H()); break; }
        case 0x75: { u16 ea = static_cast<u16>(xy.w.l + static_cast<s8>(arg())); WZ() = ea; wm(ea, L()); break; }
        case 0x77: { u16 ea = static_cast<u16>(xy.w.l + static_cast<s8>(arg())); WZ() = ea; wm(ea, A()); break; }
        case 0x7c: A() = xy.b.h; break;
        case 0x7d: A() = xy.b.l; break;
        case 0x7e: { u16 ea = static_cast<u16>(xy.w.l + static_cast<s8>(arg())); WZ() = ea; A() = rm(ea); break; }
        case 0x84: aluAdd(xy.b.h); break;
        case 0x85: aluAdd(xy.b.l); break;
        case 0x86: { u16 ea = static_cast<u16>(xy.w.l + static_cast<s8>(arg())); WZ() = ea; aluAdd(rm(ea)); break; }
        case 0x8c: aluAdc(xy.b.h); break;
        case 0x8d: aluAdc(xy.b.l); break;
        case 0x8e: { u16 ea = static_cast<u16>(xy.w.l + static_cast<s8>(arg())); WZ() = ea; aluAdc(rm(ea)); break; }
        case 0x94: aluSub(xy.b.h); break;
        case 0x95: aluSub(xy.b.l); break;
        case 0x96: { u16 ea = static_cast<u16>(xy.w.l + static_cast<s8>(arg())); WZ() = ea; aluSub(rm(ea)); break; }
        case 0x9c: aluSbc(xy.b.h); break;
        case 0x9d: aluSbc(xy.b.l); break;
        case 0x9e: { u16 ea = static_cast<u16>(xy.w.l + static_cast<s8>(arg())); WZ() = ea; aluSbc(rm(ea)); break; }
        case 0xa4: aluAnd(xy.b.h); break;
        case 0xa5: aluAnd(xy.b.l); break;
        case 0xa6: { u16 ea = static_cast<u16>(xy.w.l + static_cast<s8>(arg())); WZ() = ea; aluAnd(rm(ea)); break; }
        case 0xac: aluXor(xy.b.h); break;
        case 0xad: aluXor(xy.b.l); break;
        case 0xae: { u16 ea = static_cast<u16>(xy.w.l + static_cast<s8>(arg())); WZ() = ea; aluXor(rm(ea)); break; }
        case 0xb4: aluOr(xy.b.h); break;
        case 0xb5: aluOr(xy.b.l); break;
        case 0xb6: { u16 ea = static_cast<u16>(xy.w.l + static_cast<s8>(arg())); WZ() = ea; aluOr(rm(ea)); break; }
        case 0xbc: aluCp(xy.b.h); break;
        case 0xbd: aluCp(xy.b.l); break;
        case 0xbe: { u16 ea = static_cast<u16>(xy.w.l + static_cast<s8>(arg())); WZ() = ea; aluCp(rm(ea)); break; }
        case 0xcb: {
            u16 ea = static_cast<u16>(xy.w.l + static_cast<s8>(arg()));
            WZ() = ea;
            u8 op2 = arg();
            eatCycles(TableXyCb, op2);
            execXyCb(ea, op2);
            break;
        }
        case 0xdd: // Illegal: chain to DD (IX) dispatch
            m_s.r++;
            { u8 op2 = rop(); eatCycles(TableXy, op2); execIndexed(m_s.ix, op2); }
            break;
        case 0xe1: pop(xy); break;
        case 0xe3: opExsp(xy); break;
        case 0xe5: push(xy); break;
        case 0xe9: PC() = xy.w.l; break;
        case 0xf9: SP() = xy.w.l; break;
        case 0xfd: // Illegal: chain to FD (IY) dispatch
            m_s.r++;
            { u8 op2 = rop(); eatCycles(TableXy, op2); execIndexed(m_s.iy, op2); }
            break;
        default:
            // All other opcodes behave as if the DD/FD prefix wasn't there
            execOp(opcode);
            break;
    }
}

// --- ED prefix opcodes ---

void Z80::execEd(u8 opcode) {
    switch (opcode) {
        // IN r,(C) - 0x40-0x78 (even steps of 8)
        case 0x40: B() = in(BC()); F() = (F() & CF) | s_szp[B()]; WZ() = BC() + 1; break;
        case 0x48: C() = in(BC()); F() = (F() & CF) | s_szp[C()]; WZ() = BC() + 1; break;
        case 0x50: D() = in(BC()); F() = (F() & CF) | s_szp[D()]; WZ() = BC() + 1; break;
        case 0x58: E() = in(BC()); F() = (F() & CF) | s_szp[E()]; WZ() = BC() + 1; break;
        case 0x60: H() = in(BC()); F() = (F() & CF) | s_szp[H()]; WZ() = BC() + 1; break;
        case 0x68: L() = in(BC()); F() = (F() & CF) | s_szp[L()]; WZ() = BC() + 1; break;
        case 0x70: { u8 tmp = in(BC()); F() = (F() & CF) | s_szp[tmp]; WZ() = BC() + 1; break; } // IN F,(C)
        case 0x78: A() = in(BC()); F() = (F() & CF) | s_szp[A()]; WZ() = BC() + 1; break;

        // OUT (C),r - 0x41-0x79 (even steps of 8)
        case 0x41: out(BC(), B()); WZ() = BC() + 1; break;
        case 0x49: out(BC(), C()); WZ() = BC() + 1; break;
        case 0x51: out(BC(), D()); WZ() = BC() + 1; break;
        case 0x59: out(BC(), E()); WZ() = BC() + 1; break;
        case 0x61: out(BC(), H()); WZ() = BC() + 1; break;
        case 0x69: out(BC(), L()); WZ() = BC() + 1; break;
        case 0x71: out(BC(), 0); WZ() = BC() + 1; break; // OUT (C),0
        case 0x79: out(BC(), A()); WZ() = BC() + 1; break;

        // SBC HL,rr
        case 0x42: aluSbc16(m_s.bc); break;
        case 0x52: aluSbc16(m_s.de); break;
        case 0x62: aluSbc16(m_s.hl); break;
        case 0x72: aluSbc16(m_s.sp); break;

        // ADC HL,rr
        case 0x4a: aluAdc16(m_s.bc); break;
        case 0x5a: aluAdc16(m_s.de); break;
        case 0x6a: aluAdc16(m_s.hl); break;
        case 0x7a: aluAdc16(m_s.sp); break;

        // LD (nn),rr
        case 0x43: { u16 ea = arg16(); wm16(ea, &m_s.bc); WZ() = ea + 1; break; }
        case 0x53: { u16 ea = arg16(); wm16(ea, &m_s.de); WZ() = ea + 1; break; }
        case 0x63: { u16 ea = arg16(); wm16(ea, &m_s.hl); WZ() = ea + 1; break; }
        case 0x73: { u16 ea = arg16(); wm16(ea, &m_s.sp); WZ() = ea + 1; break; }

        // LD rr,(nn)
        case 0x4b: { u16 ea = arg16(); rm16(ea, &m_s.bc); WZ() = ea + 1; break; }
        case 0x5b: { u16 ea = arg16(); rm16(ea, &m_s.de); WZ() = ea + 1; break; }
        case 0x6b: { u16 ea = arg16(); rm16(ea, &m_s.hl); WZ() = ea + 1; break; }
        case 0x7b: { u16 ea = arg16(); rm16(ea, &m_s.sp); WZ() = ea + 1; break; }

        // NEG (0x44 + mirrors)
        case 0x44: case 0x4c: case 0x54: case 0x5c:
        case 0x64: case 0x6c: case 0x74: case 0x7c:
            aluNeg(); break;

        // RETN (0x45 + mirrors)
        case 0x45: case 0x55: case 0x65: case 0x75:
            pop(m_s.pc); WZ() = PC(); m_s.afterRetn = 1; break;

        // RETI
        case 0x4d: case 0x5d: case 0x6d: case 0x7d:
            pop(m_s.pc); WZ() = PC(); m_s.iff1 = m_s.iff2; break;

        // IM 0/1/2
        case 0x46: case 0x4e: case 0x66: case 0x6e: m_s.im = 0; break;
        case 0x56: case 0x76: m_s.im = 1; break;
        case 0x5e: case 0x7e: m_s.im = 2; break;

        // LD I,A / LD R,A / LD A,I / LD A,R
        case 0x47: m_s.i = A(); break;
        case 0x4f: m_s.r = A(); m_s.r2 = A() & 0x80; break;
        case 0x57: // LD A,I
            A() = m_s.i;
            F() = (F() & CF) | s_sz[A()] | (m_s.iff2 << 2);
            break;
        case 0x5f: // LD A,R
            A() = (m_s.r & 0x7f) | m_s.r2;
            F() = (F() & CF) | s_sz[A()] | (m_s.iff2 << 2);
            break;

        // RRD / RLD
        case 0x67: aluRrd(); break;
        case 0x6f: aluRld(); break;

        // Block operations
        case 0xa0: opLdi(); break;
        case 0xa1: opCpi(); break;
        case 0xa2: opIni(); break;
        case 0xa3: opOuti(); break;
        case 0xa8: opLdd(); break;
        case 0xa9: opCpd(); break;
        case 0xaa: opInd(); break;
        case 0xab: opOutd(); break;
        case 0xb0: opLdir(); break;
        case 0xb1: opCpir(); break;
        case 0xb2: opInir(); break;
        case 0xb3: opOtir(); break;
        case 0xb8: opLddr(); break;
        case 0xb9: opCpdr(); break;
        case 0xba: opIndr(); break;
        case 0xbb: opOtdr(); break;

        default: break; // All undefined ED opcodes are NOPs (cycles already eaten)
    }
}

// --- Main opcode dispatch (unprefixed opcodes 00-FF) ---

void Z80::execOp(u8 opcode) {
    switch (opcode) {
        case 0x00: break; // NOP
        case 0x01: BC() = arg16(); break; // LD BC,nn
        case 0x02: wm(BC(), A()); WZ_L() = (BC() + 1) & 0xff; WZ_H() = A(); break;
        case 0x03: BC()++; break;
        case 0x04: B() = aluInc(B()); break;
        case 0x05: B() = aluDec(B()); break;
        case 0x06: B() = arg(); break;
        case 0x07: aluRlca(); break;
        case 0x08: { // EX AF,AF'
            Pair tmp = m_s.af; m_s.af = m_s.af2; m_s.af2 = tmp; break;
        }
        case 0x09: aluAdd16(m_s.hl, m_s.bc); break;
        case 0x0a: A() = rm(BC()); WZ() = BC() + 1; break;
        case 0x0b: BC()--; break;
        case 0x0c: C() = aluInc(C()); break;
        case 0x0d: C() = aluDec(C()); break;
        case 0x0e: C() = arg(); break;
        case 0x0f: aluRrca(); break;
        case 0x10: { // DJNZ
            B()--;
            if (B()) { eatCycles(TableEx, 0x10); s8 d = static_cast<s8>(arg()); PC() += d; WZ() = PC(); }
            else { arg(); } // consume displacement byte
            break;
        }
        case 0x11: DE() = arg16(); break;
        case 0x12: wm(DE(), A()); WZ_L() = (DE() + 1) & 0xff; WZ_H() = A(); break;
        case 0x13: DE()++; break;
        case 0x14: D() = aluInc(D()); break;
        case 0x15: D() = aluDec(D()); break;
        case 0x16: D() = arg(); break;
        case 0x17: aluRla(); break;
        case 0x18: { // JR
            s8 d = static_cast<s8>(arg()); PC() += d; WZ() = PC(); break;
        }
        case 0x19: aluAdd16(m_s.hl, m_s.de); break;
        case 0x1a: A() = rm(DE()); WZ() = DE() + 1; break;
        case 0x1b: DE()--; break;
        case 0x1c: E() = aluInc(E()); break;
        case 0x1d: E() = aluDec(E()); break;
        case 0x1e: E() = arg(); break;
        case 0x1f: aluRra(); break;
        case 0x20: // JR NZ
            if (!(F() & ZF)) { eatCycles(TableEx, 0x20); s8 d = static_cast<s8>(arg()); PC() += d; WZ() = PC(); }
            else { arg(); }
            break;
        case 0x21: HL() = arg16(); break;
        case 0x22: { u16 ea = arg16(); wm16(ea, &m_s.hl); WZ() = ea + 1; break; }
        case 0x23: HL()++; break;
        case 0x24: H() = aluInc(H()); break;
        case 0x25: H() = aluDec(H()); break;
        case 0x26: H() = arg(); break;
        case 0x27: aluDaa(); break;
        case 0x28: // JR Z
            if (F() & ZF) { eatCycles(TableEx, 0x28); s8 d = static_cast<s8>(arg()); PC() += d; WZ() = PC(); }
            else { arg(); }
            break;
        case 0x29: aluAdd16(m_s.hl, m_s.hl); break;
        case 0x2a: { u16 ea = arg16(); rm16(ea, &m_s.hl); WZ() = ea + 1; break; }
        case 0x2b: HL()--; break;
        case 0x2c: L() = aluInc(L()); break;
        case 0x2d: L() = aluDec(L()); break;
        case 0x2e: L() = arg(); break;
        case 0x2f: // CPL
            A() ^= 0xff; F() = (F() & (SF | ZF | PF | CF)) | HF | NF | (A() & (YF | XF));
            break;
        case 0x30: // JR NC
            if (!(F() & CF)) { eatCycles(TableEx, 0x30); s8 d = static_cast<s8>(arg()); PC() += d; WZ() = PC(); }
            else { arg(); }
            break;
        case 0x31: SP() = arg16(); break;
        case 0x32: { u16 ea = arg16(); wm(ea, A()); WZ_L() = (ea + 1) & 0xff; WZ_H() = A(); break; }
        case 0x33: SP()++; break;
        case 0x34: wm(HL(), aluInc(rm(HL()))); break;
        case 0x35: wm(HL(), aluDec(rm(HL()))); break;
        case 0x36: wm(HL(), arg()); break;
        case 0x37: // SCF
            F() = (F() & (SF | ZF | PF)) | CF | (A() & (YF | XF));
            break;
        case 0x38: // JR C
            if (F() & CF) { eatCycles(TableEx, 0x38); s8 d = static_cast<s8>(arg()); PC() += d; WZ() = PC(); }
            else { arg(); }
            break;
        case 0x39: aluAdd16(m_s.hl, m_s.sp); break;
        case 0x3a: { u16 ea = arg16(); A() = rm(ea); WZ() = ea + 1; break; }
        case 0x3b: SP()--; break;
        case 0x3c: A() = aluInc(A()); break;
        case 0x3d: A() = aluDec(A()); break;
        case 0x3e: A() = arg(); break;
        case 0x3f: // CCF
            F() = ((F() & (SF | ZF | PF | CF)) | ((F() & CF) << 4) | (A() & (YF | XF))) ^ CF;
            break;
        // LD r,r' (0x40-0x7f)
        case 0x40: break; // LD B,B
        case 0x41: B() = C(); break;
        case 0x42: B() = D(); break;
        case 0x43: B() = E(); break;
        case 0x44: B() = H(); break;
        case 0x45: B() = L(); break;
        case 0x46: B() = rm(HL()); break;
        case 0x47: B() = A(); break;
        case 0x48: C() = B(); break;
        case 0x49: break; // LD C,C
        case 0x4a: C() = D(); break;
        case 0x4b: C() = E(); break;
        case 0x4c: C() = H(); break;
        case 0x4d: C() = L(); break;
        case 0x4e: C() = rm(HL()); break;
        case 0x4f: C() = A(); break;
        case 0x50: D() = B(); break;
        case 0x51: D() = C(); break;
        case 0x52: break; // LD D,D
        case 0x53: D() = E(); break;
        case 0x54: D() = H(); break;
        case 0x55: D() = L(); break;
        case 0x56: D() = rm(HL()); break;
        case 0x57: D() = A(); break;
        case 0x58: E() = B(); break;
        case 0x59: E() = C(); break;
        case 0x5a: E() = D(); break;
        case 0x5b: break; // LD E,E
        case 0x5c: E() = H(); break;
        case 0x5d: E() = L(); break;
        case 0x5e: E() = rm(HL()); break;
        case 0x5f: E() = A(); break;
        case 0x60: H() = B(); break;
        case 0x61: H() = C(); break;
        case 0x62: H() = D(); break;
        case 0x63: H() = E(); break;
        case 0x64: break; // LD H,H
        case 0x65: H() = L(); break;
        case 0x66: H() = rm(HL()); break;
        case 0x67: H() = A(); break;
        case 0x68: L() = B(); break;
        case 0x69: L() = C(); break;
        case 0x6a: L() = D(); break;
        case 0x6b: L() = E(); break;
        case 0x6c: L() = H(); break;
        case 0x6d: break; // LD L,L
        case 0x6e: L() = rm(HL()); break;
        case 0x6f: L() = A(); break;
        case 0x70: wm(HL(), B()); break;
        case 0x71: wm(HL(), C()); break;
        case 0x72: wm(HL(), D()); break;
        case 0x73: wm(HL(), E()); break;
        case 0x74: wm(HL(), H()); break;
        case 0x75: wm(HL(), L()); break;
        case 0x76: // HALT
            PC()--; m_s.halt = 1; break;
        case 0x77: wm(HL(), A()); break;
        case 0x78: A() = B(); break;
        case 0x79: A() = C(); break;
        case 0x7a: A() = D(); break;
        case 0x7b: A() = E(); break;
        case 0x7c: A() = H(); break;
        case 0x7d: A() = L(); break;
        case 0x7e: A() = rm(HL()); break;
        case 0x7f: break; // LD A,A
        // ALU A,r (0x80-0xbf)
        case 0x80: aluAdd(B()); break;
        case 0x81: aluAdd(C()); break;
        case 0x82: aluAdd(D()); break;
        case 0x83: aluAdd(E()); break;
        case 0x84: aluAdd(H()); break;
        case 0x85: aluAdd(L()); break;
        case 0x86: aluAdd(rm(HL())); break;
        case 0x87: aluAdd(A()); break;
        case 0x88: aluAdc(B()); break;
        case 0x89: aluAdc(C()); break;
        case 0x8a: aluAdc(D()); break;
        case 0x8b: aluAdc(E()); break;
        case 0x8c: aluAdc(H()); break;
        case 0x8d: aluAdc(L()); break;
        case 0x8e: aluAdc(rm(HL())); break;
        case 0x8f: aluAdc(A()); break;
        case 0x90: aluSub(B()); break;
        case 0x91: aluSub(C()); break;
        case 0x92: aluSub(D()); break;
        case 0x93: aluSub(E()); break;
        case 0x94: aluSub(H()); break;
        case 0x95: aluSub(L()); break;
        case 0x96: aluSub(rm(HL())); break;
        case 0x97: aluSub(A()); break;
        case 0x98: aluSbc(B()); break;
        case 0x99: aluSbc(C()); break;
        case 0x9a: aluSbc(D()); break;
        case 0x9b: aluSbc(E()); break;
        case 0x9c: aluSbc(H()); break;
        case 0x9d: aluSbc(L()); break;
        case 0x9e: aluSbc(rm(HL())); break;
        case 0x9f: aluSbc(A()); break;
        case 0xa0: aluAnd(B()); break;
        case 0xa1: aluAnd(C()); break;
        case 0xa2: aluAnd(D()); break;
        case 0xa3: aluAnd(E()); break;
        case 0xa4: aluAnd(H()); break;
        case 0xa5: aluAnd(L()); break;
        case 0xa6: aluAnd(rm(HL())); break;
        case 0xa7: aluAnd(A()); break;
        case 0xa8: aluXor(B()); break;
        case 0xa9: aluXor(C()); break;
        case 0xaa: aluXor(D()); break;
        case 0xab: aluXor(E()); break;
        case 0xac: aluXor(H()); break;
        case 0xad: aluXor(L()); break;
        case 0xae: aluXor(rm(HL())); break;
        case 0xaf: aluXor(A()); break;
        case 0xb0: aluOr(B()); break;
        case 0xb1: aluOr(C()); break;
        case 0xb2: aluOr(D()); break;
        case 0xb3: aluOr(E()); break;
        case 0xb4: aluOr(H()); break;
        case 0xb5: aluOr(L()); break;
        case 0xb6: aluOr(rm(HL())); break;
        case 0xb7: aluOr(A()); break;
        case 0xb8: aluCp(B()); break;
        case 0xb9: aluCp(C()); break;
        case 0xba: aluCp(D()); break;
        case 0xbb: aluCp(E()); break;
        case 0xbc: aluCp(H()); break;
        case 0xbd: aluCp(L()); break;
        case 0xbe: aluCp(rm(HL())); break;
        case 0xbf: aluCp(A()); break;

        // Control flow and prefix opcodes (0xc0-0xff)
        case 0xc0: if (!(F() & ZF)) { eatCycles(TableEx, 0xc0); pop(m_s.pc); WZ() = PC(); } break;
        case 0xc1: pop(m_s.bc); break;
        case 0xc2: if (!(F() & ZF)) { PC() = arg16(); WZ() = PC(); } else { WZ() = arg16(); } break;
        case 0xc3: PC() = arg16(); WZ() = PC(); break;
        case 0xc4: if (!(F() & ZF)) { eatCycles(TableEx, 0xc4); u16 ea = arg16(); WZ() = ea; push(m_s.pc); PC() = ea; } else { WZ() = arg16(); } break;
        case 0xc5: push(m_s.bc); break;
        case 0xc6: aluAdd(arg()); break;
        case 0xc7: push(m_s.pc); PC() = 0x00; WZ() = PC(); break; // RST 00
        case 0xc8: if (F() & ZF) { eatCycles(TableEx, 0xc8); pop(m_s.pc); WZ() = PC(); } break;
        case 0xc9: pop(m_s.pc); WZ() = PC(); break; // RET
        case 0xca: if (F() & ZF) { PC() = arg16(); WZ() = PC(); } else { WZ() = arg16(); } break;
        // Prefix CB: fetch next byte; it encodes a shift/rotate/bit instruction.
        case 0xcb: m_s.r++; { u8 op2 = rop(); eatCycles(TableCb, op2); execCb(op2); } break;
        case 0xcc: if (F() & ZF) { eatCycles(TableEx, 0xcc); u16 ea = arg16(); WZ() = ea; push(m_s.pc); PC() = ea; } else { WZ() = arg16(); } break;
        case 0xcd: { u16 ea = arg16(); WZ() = ea; push(m_s.pc); PC() = ea; break; } // CALL
        case 0xce: aluAdc(arg()); break;
        case 0xcf: push(m_s.pc); PC() = 0x08; WZ() = PC(); break; // RST 08
        case 0xd0: if (!(F() & CF)) { eatCycles(TableEx, 0xd0); pop(m_s.pc); WZ() = PC(); } break;
        case 0xd1: pop(m_s.de); break;
        case 0xd2: if (!(F() & CF)) { PC() = arg16(); WZ() = PC(); } else { WZ() = arg16(); } break;
        case 0xd3: { u8 n = arg(); u16 port = n | (A() << 8); out(port, A()); WZ_L() = (n + 1) & 0xff; WZ_H() = A(); break; }
        case 0xd4: if (!(F() & CF)) { eatCycles(TableEx, 0xd4); u16 ea = arg16(); WZ() = ea; push(m_s.pc); PC() = ea; } else { WZ() = arg16(); } break;
        case 0xd5: push(m_s.de); break;
        case 0xd6: aluSub(arg()); break;
        case 0xd7: push(m_s.pc); PC() = 0x10; WZ() = PC(); break; // RST 10
        case 0xd8: if (F() & CF) { eatCycles(TableEx, 0xd8); pop(m_s.pc); WZ() = PC(); } break;
        case 0xd9: { // EXX
            Pair tmp;
            tmp = m_s.bc; m_s.bc = m_s.bc2; m_s.bc2 = tmp;
            tmp = m_s.de; m_s.de = m_s.de2; m_s.de2 = tmp;
            tmp = m_s.hl; m_s.hl = m_s.hl2; m_s.hl2 = tmp;
            break;
        }
        case 0xda: if (F() & CF) { PC() = arg16(); WZ() = PC(); } else { WZ() = arg16(); } break;
        case 0xdb: { u8 n = arg(); u16 port = n | (A() << 8); A() = in(port); WZ() = port + 1; break; }
        case 0xdc: if (F() & CF) { eatCycles(TableEx, 0xdc); u16 ea = arg16(); WZ() = ea; push(m_s.pc); PC() = ea; } else { WZ() = arg16(); } break;
        // Prefix DD: all following operations use IX instead of HL.
        case 0xdd: m_s.r++; { u8 op2 = rop(); eatCycles(TableXy, op2); execIndexed(m_s.ix, op2); } break;
        case 0xde: aluSbc(arg()); break;
        case 0xdf: push(m_s.pc); PC() = 0x18; WZ() = PC(); break; // RST 18
        case 0xe0: if (!(F() & PF)) { eatCycles(TableEx, 0xe0); pop(m_s.pc); WZ() = PC(); } break;
        case 0xe1: pop(m_s.hl); break;
        case 0xe2: if (!(F() & PF)) { PC() = arg16(); WZ() = PC(); } else { WZ() = arg16(); } break;
        case 0xe3: opExsp(m_s.hl); break;
        case 0xe4: if (!(F() & PF)) { eatCycles(TableEx, 0xe4); u16 ea = arg16(); WZ() = ea; push(m_s.pc); PC() = ea; } else { WZ() = arg16(); } break;
        case 0xe5: push(m_s.hl); break;
        case 0xe6: aluAnd(arg()); break;
        case 0xe7: push(m_s.pc); PC() = 0x20; WZ() = PC(); break; // RST 20
        case 0xe8: if (F() & PF) { eatCycles(TableEx, 0xe8); pop(m_s.pc); WZ() = PC(); } break;
        case 0xe9: PC() = HL(); break; // JP (HL)
        case 0xea: if (F() & PF) { PC() = arg16(); WZ() = PC(); } else { WZ() = arg16(); } break;
        case 0xeb: { Pair tmp = m_s.de; m_s.de = m_s.hl; m_s.hl = tmp; break; } // EX DE,HL
        case 0xec: if (F() & PF) { eatCycles(TableEx, 0xec); u16 ea = arg16(); WZ() = ea; push(m_s.pc); PC() = ea; } else { WZ() = arg16(); } break;
        // Prefix ED: extended instructions (block ops, 16-bit ALU, I/O, etc.)
        case 0xed: m_s.r++; { u8 op2 = rop(); eatCycles(TableEd, op2); execEd(op2); } break;
        case 0xee: aluXor(arg()); break;
        case 0xef: push(m_s.pc); PC() = 0x28; WZ() = PC(); break; // RST 28
        case 0xf0: if (!(F() & SF)) { eatCycles(TableEx, 0xf0); pop(m_s.pc); WZ() = PC(); } break;
        case 0xf1: pop(m_s.af); break;
        case 0xf2: if (!(F() & SF)) { PC() = arg16(); WZ() = PC(); } else { WZ() = arg16(); } break;
        case 0xf3: m_s.iff1 = m_s.iff2 = 0; break; // DI
        case 0xf4: if (!(F() & SF)) { eatCycles(TableEx, 0xf4); u16 ea = arg16(); WZ() = ea; push(m_s.pc); PC() = ea; } else { WZ() = arg16(); } break;
        case 0xf5: push(m_s.af); break;
        case 0xf6: aluOr(arg()); break;
        case 0xf7: push(m_s.pc); PC() = 0x30; WZ() = PC(); break; // RST 30
        case 0xf8: if (F() & SF) { eatCycles(TableEx, 0xf8); pop(m_s.pc); WZ() = PC(); } break;
        case 0xf9: SP() = HL(); break;
        case 0xfa: if (F() & SF) { PC() = arg16(); WZ() = PC(); } else { WZ() = arg16(); } break;
        case 0xfb: m_s.iff1 = m_s.iff2 = 1; m_s.afterEi = 1; break; // EI
        case 0xfc: if (F() & SF) { eatCycles(TableEx, 0xfc); u16 ea = arg16(); WZ() = ea; push(m_s.pc); PC() = ea; } else { WZ() = arg16(); } break;
        // Prefix FD: all following operations use IY instead of HL.
        case 0xfd: m_s.r++; { u8 op2 = rop(); eatCycles(TableXy, op2); execIndexed(m_s.iy, op2); } break;
        case 0xfe: aluCp(arg()); break;
        case 0xff: push(m_s.pc); PC() = 0x38; WZ() = PC(); break; // RST 38
    }
}

// --- Interrupt handling ---
//
// The Z80 has three interrupt modes:
//   IM 0: Device places an instruction on the data bus (typically RST xx)
//   IM 1: Always calls 0x0038 (like RST 38h) — simplest, most common in games
//   IM 2: Vectored — reads a 16-bit address from table at [I*256 + vector]
//
// All modes: leave HALT, clear IFF1/IFF2, push PC, and jump to handler.

void Z80::takeInterrupt() {
    int irqVector = m_s.vector;

    m_s.prvpc.d = static_cast<u32>(-1);

    // Leave HALT state
    if (m_s.halt) { m_s.halt = 0; PC()++; }

    // Clear both interrupt flip flops
    m_s.iff1 = m_s.iff2 = 0;

    if (m_s.holdIrq) {
        m_s.holdIrq = 0;
        m_s.irqState = 0;
    }

    m_s.r++;

    if (m_s.im == 2) {
        // Interrupt mode 2: Call [I:vector]
        irqVector = (irqVector & 0xff) | (m_s.i << 8);
        push(m_s.pc);
        rm16(irqVector, &m_s.pc);
        eatCyclesN(m_cc[TableOp][0xcd] + m_cc[TableEx][0xff]);
    } else if (m_s.im == 1) {
        // Interrupt mode 1: RST 38h
        push(m_s.pc);
        PC() = 0x0038;
        eatCyclesN(m_cc[TableOp][0xff] + m_cc[TableEx][0xff]);
    } else {
        // Interrupt mode 0: decode vector
        switch (irqVector & 0xff0000) {
            case 0xcd0000: // CALL
                push(m_s.pc);
                PC() = irqVector & 0xffff;
                eatCyclesN(m_cc[TableOp][0xcd]);
                break;
            case 0xc30000: // JP
                PC() = irqVector & 0xffff;
                eatCyclesN(m_cc[TableOp][0xc3]);
                break;
            default: // RST
                push(m_s.pc);
                PC() = irqVector & 0x0038;
                eatCyclesN(m_cc[TableOp][0xff]);
                break;
        }
        eatCyclesN(m_cc[TableEx][0xff]);
    }
    WZ() = PC();
}

// --- Main execute loop ---

int Z80::execute(int cycles) {
    m_s.cyclesRemaining = cycles;
    m_s.cyclesBudget = cycles;
    m_s.endRun = 0;

    // Check for NMIs on the way in
    if (m_s.nmiPending) {
        m_s.prvpc.d = static_cast<u32>(-1);
        if (m_s.halt) { m_s.halt = 0; PC()++; }
        m_s.iff1 = 0;
        push(m_s.pc);
        PC() = 0x0066;
        WZ() = PC();
        eatCyclesN(11);
        m_s.nmiPending = 0;
    }

    do {
        // Check for IRQs before each instruction
        if (m_s.irqState != ClearLine && m_s.iff1 && !m_s.afterEi)
            takeInterrupt();
        m_s.afterEi = 0;

        if (m_s.afterRetn) {
            m_s.iff1 = m_s.iff2;
            m_s.afterRetn = 0;
        }

        m_s.prvpc.d = m_s.pc.d;
        m_s.r++;
        u8 opcode = rop();
        eatCycles(TableOp, opcode);
        execOp(opcode);
    } while (m_s.cyclesRemaining > 0 && !m_s.endRun);

    int executed = cycles - m_s.cyclesRemaining;
    m_s.cyclesBudget = m_s.cyclesRemaining = 0;
    return executed;
}

// --- IRQ line handling ---

void Z80::setIrqLine(int irqLine, int state) {
    if (irqLine == InputLineNmi) {
        if (m_s.nmiState == ClearLine && state != ClearLine)
            m_s.nmiPending = 1;
        m_s.nmiState = state;
    } else {
        m_s.irqState = state;
    }
}

// --- Public accessors ---

// EX AF,AF' — exposed for external save-state or debugger use.
void Z80::exAf() {
    Pair tmp = m_s.af;
    m_s.af = m_s.af2;
    m_s.af2 = tmp;
}

// Read and pop a 16-bit value from the stack (used by debugger/save-state).
int Z80::getPop() {
    Pair addr{};
    rm16(SP(), &addr);
    SP() += 2;
    return addr.w.l;
}

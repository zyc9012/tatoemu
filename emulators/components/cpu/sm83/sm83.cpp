#include "sm83.h"

SM83::SM83() { reset(); }
SM83::~SM83() {}

void SM83::reset() {
    std::memset(&m_r, 0, sizeof(m_r));
}

// ---------------------------------------------------------------------------
// execute() — run the CPU for the requested number of T-cycles.
// Returns the actual number of cycles consumed (may slightly overshoot).
// Modelled after the Z80 execute() loop: budget is stored in Regs so that
// the entire CPU state is self-contained and saveable.
// ---------------------------------------------------------------------------
int SM83::execute(int cycles) {
    m_r.cyclesRemaining = cycles;
    m_r.cyclesBudget = cycles;

    do {
        // Handle HALT state
        if (m_r.halt) {
            u8 ie = rd(0xFFFF);
            u8 ifReg = rd(0xFF0F);
            if (ie & ifReg & 0x1F) {
                m_r.halt = 0;
            } else {
                m_r.cyclesRemaining -= 4;
                continue;
            }
        }

        // Service interrupts
        int irqCycles = handleInterrupts();
        if (irqCycles > 0) {
            m_r.cyclesRemaining -= irqCycles;
            continue;
        }

        // Delayed IME enable (EI takes effect after the next instruction)
        if (m_r.imeDelay) {
            m_r.ime = 1;
            m_r.imeDelay = 0;
        }

        u8 opcode = fetch8();
        int opCycles = execOp(opcode);
        m_r.cyclesRemaining -= opCycles;
    } while (m_r.cyclesRemaining > 0);

    int executed = cycles - m_r.cyclesRemaining;
    m_r.cyclesBudget = m_r.cyclesRemaining = 0;
    return executed;
}

// ---------------------------------------------------------------------------
// Interrupt handling — returns cycles consumed (20 if serviced, 0 otherwise)
// ---------------------------------------------------------------------------
int SM83::handleInterrupts() {
    if (!m_r.ime) return 0;

    u8 ie = rd(0xFFFF);
    u8 ifReg = rd(0xFF0F);
    u8 triggered = ie & ifReg & 0x1F;
    if (!triggered) return 0;

    m_r.ime = 0;
    m_r.halt = 0;

    static const u16 vectors[5] = {0x40, 0x48, 0x50, 0x58, 0x60};
    for (int i = 0; i < 5; i++) {
        if (triggered & (1 << i)) {
            wr(0xFF0F, ifReg & ~(1 << i));
            push(PC());
            PC() = vectors[i];
            return 20;
        }
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Memory helpers
// ---------------------------------------------------------------------------
u8 SM83::fetch8() {
    u8 val = rd(m_r.pc);
    if (!m_r.haltBug) m_r.pc++;
    m_r.haltBug = 0;
    return val;
}

u16 SM83::fetch16() {
    u8 lo = rd(m_r.pc++);
    u8 hi = rd(m_r.pc++);
    return (u16(hi) << 8) | lo;
}

void SM83::push(u16 val) {
    m_r.sp -= 2;
    wr(m_r.sp, val & 0xFF);
    wr(m_r.sp + 1, val >> 8);
}

u16 SM83::pop() {
    u8 lo = rd(m_r.sp);
    u8 hi = rd(m_r.sp + 1);
    m_r.sp += 2;
    return (u16(hi) << 8) | lo;
}

void SM83::setFlag(u8 flag, bool val) {
    if (val) F() |= flag; else F() &= ~flag;
}

bool SM83::getFlag(u8 flag) const {
    return (m_r.af.b.l & flag) != 0;
}

// ---------------------------------------------------------------------------
// execOp — unprefixed opcodes.  Returns T-cycle cost.
// ---------------------------------------------------------------------------
int SM83::execOp(u8 op) {
    switch (op) {
    // NOP
    case 0x00: return 4;

    // LD rr, nn
    case 0x01: BC() = fetch16(); return 12;
    case 0x11: DE() = fetch16(); return 12;
    case 0x21: HL() = fetch16(); return 12;
    case 0x31: SP() = fetch16(); return 12;

    // LD (rr), A / LD (nn), SP
    case 0x02: wr(BC(), A()); return 8;
    case 0x12: wr(DE(), A()); return 8;
    case 0x08: { u16 a = fetch16(); wr(a, SP() & 0xFF); wr(a + 1, SP() >> 8); return 20; }

    // LD A, (rr)
    case 0x0A: A() = rd(BC()); return 8;
    case 0x1A: A() = rd(DE()); return 8;

    // INC rr (16-bit)
    case 0x03: BC()++; return 8;
    case 0x13: DE()++; return 8;
    case 0x23: HL()++; return 8;
    case 0x33: SP()++; return 8;

    // DEC rr (16-bit)
    case 0x0B: BC()--; return 8;
    case 0x1B: DE()--; return 8;
    case 0x2B: HL()--; return 8;
    case 0x3B: SP()--; return 8;

    // INC r (8-bit)
    case 0x04: B()++; setFlag(ZF, B()==0); setFlag(NF,false); setFlag(HF,(B()&0x0F)==0); return 4;
    case 0x0C: C()++; setFlag(ZF, C()==0); setFlag(NF,false); setFlag(HF,(C()&0x0F)==0); return 4;
    case 0x14: D()++; setFlag(ZF, D()==0); setFlag(NF,false); setFlag(HF,(D()&0x0F)==0); return 4;
    case 0x1C: E()++; setFlag(ZF, E()==0); setFlag(NF,false); setFlag(HF,(E()&0x0F)==0); return 4;
    case 0x24: H()++; setFlag(ZF, H()==0); setFlag(NF,false); setFlag(HF,(H()&0x0F)==0); return 4;
    case 0x2C: L()++; setFlag(ZF, L()==0); setFlag(NF,false); setFlag(HF,(L()&0x0F)==0); return 4;
    case 0x34: { u8 v=rd(HL()); v++; wr(HL(),v); setFlag(ZF,v==0); setFlag(NF,false); setFlag(HF,(v&0x0F)==0); return 12; }
    case 0x3C: A()++; setFlag(ZF, A()==0); setFlag(NF,false); setFlag(HF,(A()&0x0F)==0); return 4;

    // DEC r (8-bit)
    case 0x05: setFlag(HF,(B()&0x0F)==0); B()--; setFlag(ZF,B()==0); setFlag(NF,true); return 4;
    case 0x0D: setFlag(HF,(C()&0x0F)==0); C()--; setFlag(ZF,C()==0); setFlag(NF,true); return 4;
    case 0x15: setFlag(HF,(D()&0x0F)==0); D()--; setFlag(ZF,D()==0); setFlag(NF,true); return 4;
    case 0x1D: setFlag(HF,(E()&0x0F)==0); E()--; setFlag(ZF,E()==0); setFlag(NF,true); return 4;
    case 0x25: setFlag(HF,(H()&0x0F)==0); H()--; setFlag(ZF,H()==0); setFlag(NF,true); return 4;
    case 0x2D: setFlag(HF,(L()&0x0F)==0); L()--; setFlag(ZF,L()==0); setFlag(NF,true); return 4;
    case 0x35: { u8 v=rd(HL()); setFlag(HF,(v&0x0F)==0); v--; wr(HL(),v); setFlag(ZF,v==0); setFlag(NF,true); return 12; }
    case 0x3D: setFlag(HF,(A()&0x0F)==0); A()--; setFlag(ZF,A()==0); setFlag(NF,true); return 4;

    // LD r, n (immediate)
    case 0x06: B() = fetch8(); return 8;
    case 0x0E: C() = fetch8(); return 8;
    case 0x16: D() = fetch8(); return 8;
    case 0x1E: E() = fetch8(); return 8;
    case 0x26: H() = fetch8(); return 8;
    case 0x2E: L() = fetch8(); return 8;
    case 0x36: wr(HL(), fetch8()); return 12;
    case 0x3E: A() = fetch8(); return 8;

    // RLCA / RRCA / RLA / RRA
    case 0x07: { bool c=(A()&0x80)!=0; A()=(A()<<1)|(c?1:0); F()=c?CF:0; return 4; }
    case 0x0F: { bool c=(A()&0x01)!=0; A()=(A()>>1)|(c?0x80:0); F()=c?CF:0; return 4; }
    case 0x17: { bool oc=getFlag(CF); bool nc=(A()&0x80)!=0; A()=(A()<<1)|(oc?1:0); F()=nc?CF:0; return 4; }
    case 0x1F: { bool oc=getFlag(CF); bool nc=(A()&0x01)!=0; A()=(A()>>1)|(oc?0x80:0); F()=nc?CF:0; return 4; }

    // STOP
    case 0x10: fetch8(); return 4;

    // JR / JR cc
    case 0x18: { s8 off=static_cast<s8>(fetch8()); PC()+=off; return 12; }
    case 0x20: { s8 off=static_cast<s8>(fetch8()); if(!getFlag(ZF)){PC()+=off;return 12;} return 8; }
    case 0x28: { s8 off=static_cast<s8>(fetch8()); if( getFlag(ZF)){PC()+=off;return 12;} return 8; }
    case 0x30: { s8 off=static_cast<s8>(fetch8()); if(!getFlag(CF)){PC()+=off;return 12;} return 8; }
    case 0x38: { s8 off=static_cast<s8>(fetch8()); if( getFlag(CF)){PC()+=off;return 12;} return 8; }
    // DAA
    case 0x27: {
        u16 a = A();
        if (!getFlag(NF)) {
            if (getFlag(CF) || a > 0x99) { a += 0x60; setFlag(CF, true); }
            if (getFlag(HF) || (a & 0x0F) > 0x09) { a += 0x06; }
        } else {
            if (getFlag(CF)) a -= 0x60;
            if (getFlag(HF)) a -= 0x06;
        }
        A() = static_cast<u8>(a);
        setFlag(ZF, A() == 0);
        setFlag(HF, false);
        return 4;
    }

    // CPL
    case 0x2F: A() = ~A(); setFlag(NF, true); setFlag(HF, true); return 4;
    // SCF
    case 0x37: setFlag(NF, false); setFlag(HF, false); setFlag(CF, true); return 4;
    // CCF
    case 0x3F: setFlag(NF, false); setFlag(HF, false); setFlag(CF, !getFlag(CF)); return 4;

    // LD (HL+), A / LD (HL-), A
    case 0x22: wr(HL(), A()); HL()++; return 8;
    case 0x32: wr(HL(), A()); HL()--; return 8;
    // LD A, (HL+) / LD A, (HL-)
    case 0x2A: A() = rd(HL()); HL()++; return 8;
    case 0x3A: A() = rd(HL()); HL()--; return 8;

    // ADD HL, rr
    case 0x09: { u32 r=HL()+BC(); setFlag(NF,false); setFlag(HF,((HL()&0xFFF)+(BC()&0xFFF))>0xFFF); setFlag(CF,r>0xFFFF); HL()=r&0xFFFF; return 8; }
    case 0x19: { u32 r=HL()+DE(); setFlag(NF,false); setFlag(HF,((HL()&0xFFF)+(DE()&0xFFF))>0xFFF); setFlag(CF,r>0xFFFF); HL()=r&0xFFFF; return 8; }
    case 0x29: { u32 r=HL()+HL(); setFlag(NF,false); setFlag(HF,((HL()&0xFFF)+(HL()&0xFFF))>0xFFF); setFlag(CF,r>0xFFFF); HL()=r&0xFFFF; return 8; }
    case 0x39: { u32 r=HL()+SP(); setFlag(NF,false); setFlag(HF,((HL()&0xFFF)+(SP()&0xFFF))>0xFFF); setFlag(CF,r>0xFFFF); HL()=r&0xFFFF; return 8; }

    // LD r, r' (8-bit register-to-register loads)
    case 0x40: return 4; // LD B,B
    case 0x41: B()=C(); return 4;
    case 0x42: B()=D(); return 4;
    case 0x43: B()=E(); return 4;
    case 0x44: B()=H(); return 4;
    case 0x45: B()=L(); return 4;
    case 0x46: B()=rd(HL()); return 8;
    case 0x47: B()=A(); return 4;

    case 0x48: C()=B(); return 4;
    case 0x49: return 4; // LD C,C
    case 0x4A: C()=D(); return 4;
    case 0x4B: C()=E(); return 4;
    case 0x4C: C()=H(); return 4;
    case 0x4D: C()=L(); return 4;
    case 0x4E: C()=rd(HL()); return 8;
    case 0x4F: C()=A(); return 4;

    case 0x50: D()=B(); return 4;
    case 0x51: D()=C(); return 4;
    case 0x52: return 4; // LD D,D
    case 0x53: D()=E(); return 4;
    case 0x54: D()=H(); return 4;
    case 0x55: D()=L(); return 4;
    case 0x56: D()=rd(HL()); return 8;
    case 0x57: D()=A(); return 4;

    case 0x58: E()=B(); return 4;
    case 0x59: E()=C(); return 4;
    case 0x5A: E()=D(); return 4;
    case 0x5B: return 4; // LD E,E
    case 0x5C: E()=H(); return 4;
    case 0x5D: E()=L(); return 4;
    case 0x5E: E()=rd(HL()); return 8;
    case 0x5F: E()=A(); return 4;

    case 0x60: H()=B(); return 4;
    case 0x61: H()=C(); return 4;
    case 0x62: H()=D(); return 4;
    case 0x63: H()=E(); return 4;
    case 0x64: return 4; // LD H,H
    case 0x65: H()=L(); return 4;
    case 0x66: H()=rd(HL()); return 8;
    case 0x67: H()=A(); return 4;

    case 0x68: L()=B(); return 4;
    case 0x69: L()=C(); return 4;
    case 0x6A: L()=D(); return 4;
    case 0x6B: L()=E(); return 4;
    case 0x6C: L()=H(); return 4;
    case 0x6D: return 4; // LD L,L
    case 0x6E: L()=rd(HL()); return 8;
    case 0x6F: L()=A(); return 4;

    case 0x70: wr(HL(),B()); return 8;
    case 0x71: wr(HL(),C()); return 8;
    case 0x72: wr(HL(),D()); return 8;
    case 0x73: wr(HL(),E()); return 8;
    case 0x74: wr(HL(),H()); return 8;
    case 0x75: wr(HL(),L()); return 8;
    case 0x77: wr(HL(),A()); return 8;

    case 0x78: A()=B(); return 4;
    case 0x79: A()=C(); return 4;
    case 0x7A: A()=D(); return 4;
    case 0x7B: A()=E(); return 4;
    case 0x7C: A()=H(); return 4;
    case 0x7D: A()=L(); return 4;
    case 0x7E: A()=rd(HL()); return 8;
    case 0x7F: return 4; // LD A,A

    // HALT
    case 0x76:
        if (!m_r.ime) {
            u8 ie = rd(0xFFFF);
            u8 ifReg = rd(0xFF0F);
            if (ifReg & ie & 0x1F) { m_r.haltBug = 1; return 4; }
        }
        m_r.halt = 1;
        return 4;

    // --- ALU A, r ---
    // ADD A, r
    case 0x80: case 0x81: case 0x82: case 0x83:
    case 0x84: case 0x85: case 0x86: case 0x87: {
        u8 v; bool hl=false;
        switch(op&7){case 0:v=B();break;case 1:v=C();break;case 2:v=D();break;case 3:v=E();break;
                     case 4:v=H();break;case 5:v=L();break;case 6:v=rd(HL());hl=true;break;default:v=A();break;}
        u16 r=A()+v;
        setFlag(HF,((A()&0x0F)+(v&0x0F))>0x0F);
        setFlag(CF,r>0xFF);
        A()=r&0xFF;
        setFlag(ZF,A()==0); setFlag(NF,false);
        return hl?8:4;
    }
    // ADC A, r
    case 0x88: case 0x89: case 0x8A: case 0x8B:
    case 0x8C: case 0x8D: case 0x8E: case 0x8F: {
        u8 v; bool hl=false;
        switch(op&7){case 0:v=B();break;case 1:v=C();break;case 2:v=D();break;case 3:v=E();break;
                     case 4:v=H();break;case 5:v=L();break;case 6:v=rd(HL());hl=true;break;default:v=A();break;}
        u8 cy=getFlag(CF)?1:0;
        u16 r=A()+v+cy;
        setFlag(HF,((A()&0x0F)+(v&0x0F)+cy)>0x0F);
        setFlag(CF,r>0xFF);
        A()=r&0xFF;
        setFlag(ZF,A()==0); setFlag(NF,false);
        return hl?8:4;
    }
    // SUB r
    case 0x90: case 0x91: case 0x92: case 0x93:
    case 0x94: case 0x95: case 0x96: case 0x97: {
        u8 v; bool hl=false;
        switch(op&7){case 0:v=B();break;case 1:v=C();break;case 2:v=D();break;case 3:v=E();break;
                     case 4:v=H();break;case 5:v=L();break;case 6:v=rd(HL());hl=true;break;default:v=A();break;}
        setFlag(HF,(A()&0x0F)<(v&0x0F));
        setFlag(CF,A()<v);
        A()-=v;
        setFlag(ZF,A()==0); setFlag(NF,true);
        return hl?8:4;
    }
    // SBC A, r
    case 0x98: case 0x99: case 0x9A: case 0x9B:
    case 0x9C: case 0x9D: case 0x9E: case 0x9F: {
        u8 v; bool hl=false;
        switch(op&7){case 0:v=B();break;case 1:v=C();break;case 2:v=D();break;case 3:v=E();break;
                     case 4:v=H();break;case 5:v=L();break;case 6:v=rd(HL());hl=true;break;default:v=A();break;}
        u8 cy=getFlag(CF)?1:0;
        s16 r=A()-v-cy;
        setFlag(HF,((A()&0x0F)-(v&0x0F)-cy)<0);
        setFlag(CF,r<0);
        A()=r&0xFF;
        setFlag(ZF,A()==0); setFlag(NF,true);
        return hl?8:4;
    }
    // AND r
    case 0xA0: case 0xA1: case 0xA2: case 0xA3:
    case 0xA4: case 0xA5: case 0xA6: case 0xA7: {
        u8 v; bool hl=false;
        switch(op&7){case 0:v=B();break;case 1:v=C();break;case 2:v=D();break;case 3:v=E();break;
                     case 4:v=H();break;case 5:v=L();break;case 6:v=rd(HL());hl=true;break;default:v=A();break;}
        A()&=v;
        setFlag(ZF,A()==0); setFlag(NF,false); setFlag(HF,true); setFlag(CF,false);
        return hl?8:4;
    }
    // XOR r
    case 0xA8: case 0xA9: case 0xAA: case 0xAB:
    case 0xAC: case 0xAD: case 0xAE: case 0xAF: {
        u8 v; bool hl=false;
        switch(op&7){case 0:v=B();break;case 1:v=C();break;case 2:v=D();break;case 3:v=E();break;
                     case 4:v=H();break;case 5:v=L();break;case 6:v=rd(HL());hl=true;break;default:v=A();break;}
        A()^=v;
        setFlag(ZF,A()==0); setFlag(NF,false); setFlag(HF,false); setFlag(CF,false);
        return hl?8:4;
    }
    // OR r
    case 0xB0: case 0xB1: case 0xB2: case 0xB3:
    case 0xB4: case 0xB5: case 0xB6: case 0xB7: {
        u8 v; bool hl=false;
        switch(op&7){case 0:v=B();break;case 1:v=C();break;case 2:v=D();break;case 3:v=E();break;
                     case 4:v=H();break;case 5:v=L();break;case 6:v=rd(HL());hl=true;break;default:v=A();break;}
        A()|=v;
        setFlag(ZF,A()==0); setFlag(NF,false); setFlag(HF,false); setFlag(CF,false);
        return hl?8:4;
    }
    // CP r
    case 0xB8: case 0xB9: case 0xBA: case 0xBB:
    case 0xBC: case 0xBD: case 0xBE: case 0xBF: {
        u8 v; bool hl=false;
        switch(op&7){case 0:v=B();break;case 1:v=C();break;case 2:v=D();break;case 3:v=E();break;
                     case 4:v=H();break;case 5:v=L();break;case 6:v=rd(HL());hl=true;break;default:v=A();break;}
        setFlag(ZF,A()==v); setFlag(NF,true);
        setFlag(HF,(A()&0x0F)<(v&0x0F));
        setFlag(CF,A()<v);
        return hl?8:4;
    }
    // --- ALU A, n (immediate) ---
    case 0xC6: { u8 v=fetch8(); u16 r=A()+v; setFlag(HF,((A()&0x0F)+(v&0x0F))>0x0F); setFlag(CF,r>0xFF); A()=r&0xFF; setFlag(ZF,A()==0); setFlag(NF,false); return 8; }
    case 0xD6: { u8 v=fetch8(); setFlag(HF,(A()&0x0F)<(v&0x0F)); setFlag(CF,A()<v); A()-=v; setFlag(ZF,A()==0); setFlag(NF,true); return 8; }
    case 0xE6: { A()&=fetch8(); setFlag(ZF,A()==0); setFlag(NF,false); setFlag(HF,true); setFlag(CF,false); return 8; }
    case 0xF6: { A()|=fetch8(); setFlag(ZF,A()==0); setFlag(NF,false); setFlag(HF,false); setFlag(CF,false); return 8; }
    case 0xCE: { u8 v=fetch8(); u8 cy=getFlag(CF)?1:0; u16 r=A()+v+cy; setFlag(HF,((A()&0x0F)+(v&0x0F)+cy)>0x0F); setFlag(CF,r>0xFF); A()=r&0xFF; setFlag(ZF,A()==0); setFlag(NF,false); return 8; }
    case 0xDE: { u8 v=fetch8(); u8 cy=getFlag(CF)?1:0; s16 r=A()-v-cy; setFlag(HF,((A()&0x0F)-(v&0x0F)-cy)<0); setFlag(CF,r<0); A()=r&0xFF; setFlag(ZF,A()==0); setFlag(NF,true); return 8; }
    case 0xEE: { A()^=fetch8(); setFlag(ZF,A()==0); setFlag(NF,false); setFlag(HF,false); setFlag(CF,false); return 8; }
    case 0xFE: { u8 v=fetch8(); setFlag(ZF,A()==v); setFlag(NF,true); setFlag(HF,(A()&0x0F)<(v&0x0F)); setFlag(CF,A()<v); return 8; }

    // RET cc
    case 0xC0: if(!getFlag(ZF)){PC()=pop();return 20;} return 8;
    case 0xC8: if( getFlag(ZF)){PC()=pop();return 20;} return 8;
    case 0xD0: if(!getFlag(CF)){PC()=pop();return 20;} return 8;
    case 0xD8: if( getFlag(CF)){PC()=pop();return 20;} return 8;

    // POP
    case 0xC1: BC()=pop(); return 12;
    case 0xD1: DE()=pop(); return 12;
    case 0xE1: HL()=pop(); return 12;
    case 0xF1: AF()=pop()&0xFFF0; return 12;

    // JP cc, nn
    case 0xC2: { u16 a=fetch16(); if(!getFlag(ZF)){PC()=a;return 16;} return 12; }
    case 0xCA: { u16 a=fetch16(); if( getFlag(ZF)){PC()=a;return 16;} return 12; }
    case 0xD2: { u16 a=fetch16(); if(!getFlag(CF)){PC()=a;return 16;} return 12; }
    case 0xDA: { u16 a=fetch16(); if( getFlag(CF)){PC()=a;return 16;} return 12; }

    // JP nn
    case 0xC3: PC()=fetch16(); return 16;

    // CALL cc, nn
    case 0xC4: { u16 a=fetch16(); if(!getFlag(ZF)){push(PC());PC()=a;return 24;} return 12; }
    case 0xCC: { u16 a=fetch16(); if( getFlag(ZF)){push(PC());PC()=a;return 24;} return 12; }
    case 0xD4: { u16 a=fetch16(); if(!getFlag(CF)){push(PC());PC()=a;return 24;} return 12; }
    case 0xDC: { u16 a=fetch16(); if( getFlag(CF)){push(PC());PC()=a;return 24;} return 12; }

    // PUSH
    case 0xC5: push(BC()); return 16;
    case 0xD5: push(DE()); return 16;
    case 0xE5: push(HL()); return 16;
    case 0xF5: push(AF()); return 16;

    // RST
    case 0xC7: push(PC()); PC()=0x00; return 16;
    case 0xCF: push(PC()); PC()=0x08; return 16;
    case 0xD7: push(PC()); PC()=0x10; return 16;
    case 0xDF: push(PC()); PC()=0x18; return 16;
    case 0xE7: push(PC()); PC()=0x20; return 16;
    case 0xEF: push(PC()); PC()=0x28; return 16;
    case 0xF7: push(PC()); PC()=0x30; return 16;
    case 0xFF: push(PC()); PC()=0x38; return 16;

    // RET / RETI
    case 0xC9: PC()=pop(); return 16;
    case 0xD9: PC()=pop(); m_r.ime=1; return 16;

    // JP (HL)
    case 0xE9: PC()=HL(); return 4;

    // LD SP, HL
    case 0xF9: SP()=HL(); return 8;

    // CALL nn
    case 0xCD: { u16 a=fetch16(); push(PC()); PC()=a; return 24; }

    // CB prefix
    case 0xCB: return execCb(fetch8()) + 4; // +4 for the CB fetch itself

    // LDH (n), A / LD (C), A / LDH A, (n) / LD A, (C)
    case 0xE0: wr(0xFF00+fetch8(), A()); return 12;
    case 0xE2: wr(0xFF00+C(), A()); return 8;
    case 0xF0: A()=rd(0xFF00+fetch8()); return 12;
    case 0xF2: A()=rd(0xFF00+C()); return 8;

    // LD (nn), A / LD A, (nn)
    case 0xEA: wr(fetch16(), A()); return 16;
    case 0xFA: A()=rd(fetch16()); return 16;

    // DI / EI
    case 0xF3: m_r.ime=0; return 4;
    case 0xFB: m_r.imeDelay=1; return 4;

    // ADD SP, n
    case 0xE8: {
        s8 v=static_cast<s8>(fetch8());
        u32 r=SP()+v;
        setFlag(ZF,false); setFlag(NF,false);
        setFlag(HF,((SP()&0x0F)+(v&0x0F))>0x0F);
        setFlag(CF,((SP()&0xFF)+(v&0xFF))>0xFF);
        SP()=r&0xFFFF;
        return 16;
    }

    // LD HL, SP+n
    case 0xF8: {
        s8 v=static_cast<s8>(fetch8());
        u32 r=SP()+v;
        setFlag(ZF,false); setFlag(NF,false);
        setFlag(HF,((SP()&0x0F)+(v&0x0F))>0x0F);
        setFlag(CF,((SP()&0xFF)+(v&0xFF))>0xFF);
        HL()=r&0xFFFF;
        return 12;
    }

    default: return 4; // Undefined opcodes act as NOP
    }
}

// ---------------------------------------------------------------------------
// execCb — CB-prefixed opcodes.  Returns T-cycle cost (excluding the CB
// prefix fetch which is accounted for in execOp).
// ---------------------------------------------------------------------------
int SM83::execCb(u8 op) {
    // Decode register operand
    u8* reg = nullptr;
    u8 val = 0;
    bool useMem = false;

    switch (op & 0x07) {
    case 0: reg = &B(); break;
    case 1: reg = &C(); break;
    case 2: reg = &D(); break;
    case 3: reg = &E(); break;
    case 4: reg = &H(); break;
    case 5: reg = &L(); break;
    case 6: val = rd(HL()); useMem = true; break;
    case 7: reg = &A(); break;
    }

    if (!useMem && reg) val = *reg;

    u8 bit = (op >> 3) & 0x07;

    if (op < 0x40) {
        // Rotate / shift / swap
        switch (op >> 3) {
        case 0: { // RLC
            bool c = (val & 0x80) != 0;
            val = (val << 1) | (c ? 1 : 0);
            setFlag(ZF, val == 0); setFlag(NF, false); setFlag(HF, false); setFlag(CF, c);
            break;
        }
        case 1: { // RRC
            bool c = (val & 0x01) != 0;
            val = (val >> 1) | (c ? 0x80 : 0);
            setFlag(ZF, val == 0); setFlag(NF, false); setFlag(HF, false); setFlag(CF, c);
            break;
        }
        case 2: { // RL
            bool oc = getFlag(CF); bool nc = (val & 0x80) != 0;
            val = (val << 1) | (oc ? 1 : 0);
            setFlag(ZF, val == 0); setFlag(NF, false); setFlag(HF, false); setFlag(CF, nc);
            break;
        }
        case 3: { // RR
            bool oc = getFlag(CF); bool nc = (val & 0x01) != 0;
            val = (val >> 1) | (oc ? 0x80 : 0);
            setFlag(ZF, val == 0); setFlag(NF, false); setFlag(HF, false); setFlag(CF, nc);
            break;
        }
        case 4: { // SLA
            bool c = (val & 0x80) != 0;
            val <<= 1;
            setFlag(ZF, val == 0); setFlag(NF, false); setFlag(HF, false); setFlag(CF, c);
            break;
        }
        case 5: { // SRA
            bool c = (val & 0x01) != 0;
            val = (val >> 1) | (val & 0x80);
            setFlag(ZF, val == 0); setFlag(NF, false); setFlag(HF, false); setFlag(CF, c);
            break;
        }
        case 6: { // SWAP
            val = ((val & 0x0F) << 4) | ((val & 0xF0) >> 4);
            setFlag(ZF, val == 0); setFlag(NF, false); setFlag(HF, false); setFlag(CF, false);
            break;
        }
        case 7: { // SRL
            bool c = (val & 0x01) != 0;
            val >>= 1;
            setFlag(ZF, val == 0); setFlag(NF, false); setFlag(HF, false); setFlag(CF, c);
            break;
        }
        }
    } else if (op < 0x80) {
        // BIT — test bit, no writeback
        setFlag(ZF, (val & (1 << bit)) == 0);
        setFlag(NF, false); setFlag(HF, true);
        return useMem ? 8 : 4;
    } else if (op < 0xC0) {
        // RES
        val &= ~(1 << bit);
    } else {
        // SET
        val |= (1 << bit);
    }

    // Writeback (BIT returned above)
    if (useMem) wr(HL(), val);
    else if (reg) *reg = val;

    return useMem ? 12 : 4;
}

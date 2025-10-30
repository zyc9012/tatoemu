#include "cpu.h"
#include "mmu.h"
#include <iostream>

CPU::CPU()
    : m_mmu(nullptr)
    , m_halted(false)
    , m_haltBug(false)
    , m_ime(false)
    , m_enableIMENextInstruction(false)
    , m_if(0)
    , m_gbcMode(false) {
    reset();
}

CPU::~CPU() {
}

void CPU::setMMU(MMU* mmu) {
    m_mmu = mmu;
}

void CPU::saveState(std::ofstream& file) const {
    file.write(reinterpret_cast<const char*>(&m_regs), sizeof(m_regs));
    file.write(reinterpret_cast<const char*>(&m_halted), sizeof(m_halted));
    file.write(reinterpret_cast<const char*>(&m_haltBug), sizeof(m_haltBug));
    file.write(reinterpret_cast<const char*>(&m_ime), sizeof(m_ime));
    file.write(reinterpret_cast<const char*>(&m_enableIMENextInstruction), sizeof(m_enableIMENextInstruction));
    file.write(reinterpret_cast<const char*>(&m_if), sizeof(m_if));
    file.write(reinterpret_cast<const char*>(&m_gbcMode), sizeof(m_gbcMode));
}

void CPU::loadState(std::ifstream& file) {
    file.read(reinterpret_cast<char*>(&m_regs), sizeof(m_regs));
    file.read(reinterpret_cast<char*>(&m_halted), sizeof(m_halted));
    file.read(reinterpret_cast<char*>(&m_haltBug), sizeof(m_haltBug));
    file.read(reinterpret_cast<char*>(&m_ime), sizeof(m_ime));
    file.read(reinterpret_cast<char*>(&m_enableIMENextInstruction), sizeof(m_enableIMENextInstruction));
    file.read(reinterpret_cast<char*>(&m_if), sizeof(m_if));
    file.read(reinterpret_cast<char*>(&m_gbcMode), sizeof(m_gbcMode));
}

void CPU::reset(bool useBootrom) {
    if (useBootrom) {
        // Start from the beginning for bootrom execution
        m_regs.af = 0x0000;
        m_regs.bc = 0x0000;
        m_regs.de = 0x0000;
        m_regs.hl = 0x0000;
        m_regs.sp = 0x0000;
        m_regs.pc = 0x0000;  // Start at bootrom
    } else {
        // Initialize registers to post-boot values (skip bootrom)
        if (m_gbcMode) {
            m_regs.af = 0x1180;
            m_regs.bc = 0x0000;
            m_regs.de = 0xFF56;
            m_regs.hl = 0x000D;
            m_regs.sp = 0xFFFE;
            m_regs.pc = 0x0100;  // Start at ROM entry point
        } else {
            m_regs.af = 0x01B0;
            m_regs.bc = 0x0013;
            m_regs.de = 0x00D8;
            m_regs.hl = 0x014D;
            m_regs.sp = 0xFFFE;
            m_regs.pc = 0x0100;  // Start at ROM entry point
        }
    }
    
    m_halted = false;
    m_haltBug = false;
    m_ime = false;
    m_enableIMENextInstruction = false;
    m_if = 0;
}

u32 CPU::step() {
    if (m_halted) {
        // Check if any interrupts are pending
        u8 ie = m_mmu ? m_mmu->read(0xFFFF) : 0;
        if (m_if & ie & 0x1F) {
            m_halted = false;
        } else {
            return 4; // NOP cycles while halted
        }
    }

    u32 cycles = handleInterrupts();
    if (cycles > 0) {
        // Interrupt was handled
        return cycles;
    }

    if (m_enableIMENextInstruction) {
        m_ime = true;
        m_enableIMENextInstruction = false;
    }

    u8 opcode = fetch8();
    cycles = executeInstruction(opcode);
    
    return cycles;
}

u32 CPU::handleInterrupts() {
    if (!m_ime) {
        return 0;
    }

    u8 ie = m_mmu ? m_mmu->read(0xFFFF) : 0;
    u8 triggered = m_if & ie & 0x1F;

    if (!triggered) {
        return 0;
    }

    m_ime = false;
    m_halted = false;

    // Handle interrupts in priority order
    for (int i = 0; i < 5; i++) {
        if (triggered & (1 << i)) {
            m_if &= ~(1 << i);
            
            // Push PC to stack
            push(m_regs.pc);
            
            // Jump to interrupt handler
            static const u16 handlers[5] = {0x40, 0x48, 0x50, 0x58, 0x60};
            m_regs.pc = handlers[i];
            
            // Interrupt servicing takes 20 cycles (5 M-cycles)
            return 20;
        }
    }
    
    return 0;
}

void CPU::requestInterrupt(u8 interrupt) {
    m_if |= interrupt;
}

// Helper functions
u8 CPU::read8(u16 address) const {
    if (address == 0xFF0F) {
        return m_if;
    }
    return m_mmu ? m_mmu->read(address) : 0xFF;
}

void CPU::write8(u16 address, u8 value) {
    if (address == 0xFF0F) {
        m_if = value;
        return;
    }
    if (m_mmu) {
        m_mmu->write(address, value);
    }
}

u16 CPU::read16(u16 address) const {
    u8 lo = read8(address);
    u8 hi = read8(address + 1);
    return (hi << 8) | lo;
}

void CPU::write16(u16 address, u16 value) {
    write8(address, value & 0xFF);
    write8(address + 1, value >> 8);
}

u8 CPU::fetch8() {
    u8 value = read8(m_regs.pc);
    if (!m_haltBug) {
        m_regs.pc++;
    }
    m_haltBug = false;  // Clear after one instruction
    return value;
}

u16 CPU::fetch16() {
    u16 value = read16(m_regs.pc);
    m_regs.pc += 2;
    return value;
}

void CPU::push(u16 value) {
    m_regs.sp -= 2;
    write16(m_regs.sp, value);
}

u16 CPU::pop() {
    u16 value = read16(m_regs.sp);
    m_regs.sp += 2;
    return value;
}

void CPU::setFlag(u8 flag, bool value) {
    if (value) {
        m_regs.f |= flag;
    } else {
        m_regs.f &= ~flag;
    }
}

bool CPU::getFlag(u8 flag) const {
    return (m_regs.f & flag) != 0;
}

u32 CPU::executeInstruction(u8 opcode) {
    switch (opcode) {
        // NOP
        case 0x00: return 4;
        
        // LD BC, nn
        case 0x01: m_regs.bc = fetch16(); return 12;
        case 0x11: m_regs.de = fetch16(); return 12;
        case 0x21: m_regs.hl = fetch16(); return 12;
        case 0x31: m_regs.sp = fetch16(); return 12;
        
        // LD (BC), A
        case 0x02: write8(m_regs.bc, m_regs.a); return 8;
        case 0x12: write8(m_regs.de, m_regs.a); return 8;
        
        // LD (nn), SP
        case 0x08: write16(fetch16(), m_regs.sp); return 20;
        
        // LD A, (BC)
        case 0x0A: m_regs.a = read8(m_regs.bc); return 8;
        case 0x1A: m_regs.a = read8(m_regs.de); return 8;
        
        // INC BC (16-bit INC takes 8 cycles)
        case 0x03: m_regs.bc++; return 8;
        case 0x13: m_regs.de++; return 8;
        case 0x23: m_regs.hl++; return 8;
        case 0x33: m_regs.sp++; return 8;
        
        // DEC BC (16-bit DEC takes 8 cycles)
        case 0x0B: m_regs.bc--; return 8;
        case 0x1B: m_regs.de--; return 8;
        case 0x2B: m_regs.hl--; return 8;
        case 0x3B: m_regs.sp--; return 8;
        
        // INC B (8-bit INC takes 4 cycles, except (HL) = 12)
        case 0x04: {
            m_regs.b++;
            setFlag(FLAG_Z, m_regs.b == 0);
            setFlag(FLAG_N, false);
            setFlag(FLAG_H, (m_regs.b & 0x0F) == 0);
            return 4;
        }
        case 0x0C: {
            m_regs.c++;
            setFlag(FLAG_Z, m_regs.c == 0);
            setFlag(FLAG_N, false);
            setFlag(FLAG_H, (m_regs.c & 0x0F) == 0);
            return 4;
        }
        case 0x14: {
            m_regs.d++;
            setFlag(FLAG_Z, m_regs.d == 0);
            setFlag(FLAG_N, false);
            setFlag(FLAG_H, (m_regs.d & 0x0F) == 0);
            return 4;
        }
        case 0x1C: {
            m_regs.e++;
            setFlag(FLAG_Z, m_regs.e == 0);
            setFlag(FLAG_N, false);
            setFlag(FLAG_H, (m_regs.e & 0x0F) == 0);
            return 4;
        }
        case 0x24: {
            m_regs.h++;
            setFlag(FLAG_Z, m_regs.h == 0);
            setFlag(FLAG_N, false);
            setFlag(FLAG_H, (m_regs.h & 0x0F) == 0);
            return 4;
        }
        case 0x2C: {
            m_regs.l++;
            setFlag(FLAG_Z, m_regs.l == 0);
            setFlag(FLAG_N, false);
            setFlag(FLAG_H, (m_regs.l & 0x0F) == 0);
            return 4;
        }
        case 0x34: {
            u8 value = read8(m_regs.hl);
            value++;
            write8(m_regs.hl, value);
            setFlag(FLAG_Z, value == 0);
            setFlag(FLAG_N, false);
            setFlag(FLAG_H, (value & 0x0F) == 0);
            return 12;
        }
        case 0x3C: {
            m_regs.a++;
            setFlag(FLAG_Z, m_regs.a == 0);
            setFlag(FLAG_N, false);
            setFlag(FLAG_H, (m_regs.a & 0x0F) == 0);
            return 4;
        }
        
        // DEC B (8-bit DEC takes 4 cycles, except (HL) = 12)
        case 0x05: {
            setFlag(FLAG_H, (m_regs.b & 0x0F) == 0);
            m_regs.b--;
            setFlag(FLAG_Z, m_regs.b == 0);
            setFlag(FLAG_N, true);
            return 4;
        }
        case 0x0D: {
            setFlag(FLAG_H, (m_regs.c & 0x0F) == 0);
            m_regs.c--;
            setFlag(FLAG_Z, m_regs.c == 0);
            setFlag(FLAG_N, true);
            return 4;
        }
        case 0x15: {
            setFlag(FLAG_H, (m_regs.d & 0x0F) == 0);
            m_regs.d--;
            setFlag(FLAG_Z, m_regs.d == 0);
            setFlag(FLAG_N, true);
            return 4;
        }
        case 0x1D: {
            setFlag(FLAG_H, (m_regs.e & 0x0F) == 0);
            m_regs.e--;
            setFlag(FLAG_Z, m_regs.e == 0);
            setFlag(FLAG_N, true);
            return 4;
        }
        case 0x25: {
            setFlag(FLAG_H, (m_regs.h & 0x0F) == 0);
            m_regs.h--;
            setFlag(FLAG_Z, m_regs.h == 0);
            setFlag(FLAG_N, true);
            return 4;
        }
        case 0x2D: {
            setFlag(FLAG_H, (m_regs.l & 0x0F) == 0);
            m_regs.l--;
            setFlag(FLAG_Z, m_regs.l == 0);
            setFlag(FLAG_N, true);
            return 4;
        }
        case 0x35: {
            u8 value = read8(m_regs.hl);
            setFlag(FLAG_H, (value & 0x0F) == 0);
            value--;
            write8(m_regs.hl, value);
            setFlag(FLAG_Z, value == 0);
            setFlag(FLAG_N, true);
            return 12;
        }
        case 0x3D: {
            setFlag(FLAG_H, (m_regs.a & 0x0F) == 0);
            m_regs.a--;
            setFlag(FLAG_Z, m_regs.a == 0);
            setFlag(FLAG_N, true);
            return 4;
        }
        
        // LD B, n (8 cycles)
        case 0x06: m_regs.b = fetch8(); return 8;
        case 0x0E: m_regs.c = fetch8(); return 8;
        case 0x16: m_regs.d = fetch8(); return 8;
        case 0x1E: m_regs.e = fetch8(); return 8;
        case 0x26: m_regs.h = fetch8(); return 8;
        case 0x2E: m_regs.l = fetch8(); return 8;
        case 0x36: write8(m_regs.hl, fetch8()); return 12;
        case 0x3E: m_regs.a = fetch8(); return 8;
        
        // RLCA (4 cycles)
        case 0x07: {
            bool carry = (m_regs.a & 0x80) != 0;
            m_regs.a = (m_regs.a << 1) | (carry ? 1 : 0);
            setFlag(FLAG_Z, false);
            setFlag(FLAG_N, false);
            setFlag(FLAG_H, false);
            setFlag(FLAG_C, carry);
            return 4;
        }
        
        // RRCA (4 cycles)
        case 0x0F: {
            bool carry = (m_regs.a & 0x01) != 0;
            m_regs.a = (m_regs.a >> 1) | (carry ? 0x80 : 0);
            setFlag(FLAG_Z, false);
            setFlag(FLAG_N, false);
            setFlag(FLAG_H, false);
            setFlag(FLAG_C, carry);
            return 4;
        }
        
        // RLA (4 cycles)
        case 0x17: {
            bool oldCarry = getFlag(FLAG_C);
            bool newCarry = (m_regs.a & 0x80) != 0;
            m_regs.a = (m_regs.a << 1) | (oldCarry ? 1 : 0);
            setFlag(FLAG_Z, false);
            setFlag(FLAG_N, false);
            setFlag(FLAG_H, false);
            setFlag(FLAG_C, newCarry);
            return 4;
        }
        
        // RRA (4 cycles)
        case 0x1F: {
            bool oldCarry = getFlag(FLAG_C);
            bool newCarry = (m_regs.a & 0x01) != 0;
            m_regs.a = (m_regs.a >> 1) | (oldCarry ? 0x80 : 0);
            setFlag(FLAG_Z, false);
            setFlag(FLAG_N, false);
            setFlag(FLAG_H, false);
            setFlag(FLAG_C, newCarry);
            return 4;
        }
        
        // STOP (4 cycles)
        case 0x10:
            fetch8(); // STOP takes a 0x00 byte after it
            if (m_gbcMode && m_mmu) {
                // In GBC mode, STOP is used for speed switching
                m_mmu->performSpeedSwitch();
            }
            // In DMG mode or if not switching speed, STOP halts until button press
            // For simplicity, we just continue execution
            return 4;
        
        // JR n (12 cycles)
        case 0x18: {
            s8 offset = static_cast<s8>(fetch8());
            m_regs.pc += offset;
            return 12;
        }
        
        // JR NZ, n (12 if taken, 8 if not)
        case 0x20: {
            s8 offset = static_cast<s8>(fetch8());
            if (!getFlag(FLAG_Z)) {
                m_regs.pc += offset;
                return 12;
            }
            return 8;
        }
        
        // JR Z, n (12 if taken, 8 if not)
        case 0x28: {
            s8 offset = static_cast<s8>(fetch8());
            if (getFlag(FLAG_Z)) {
                m_regs.pc += offset;
                return 12;
            }
            return 8;
        }
        
        // JR NC, n (12 if taken, 8 if not)
        case 0x30: {
            s8 offset = static_cast<s8>(fetch8());
            if (!getFlag(FLAG_C)) {
                m_regs.pc += offset;
                return 12;
            }
            return 8;
        }
        
        // JR C, n (12 if taken, 8 if not)
        case 0x38: {
            s8 offset = static_cast<s8>(fetch8());
            if (getFlag(FLAG_C)) {
                m_regs.pc += offset;
                return 12;
            }
            return 8;
        }
        
        // DAA (4 cycles) - FIXED: now clears carry when no adjustment
        case 0x27: {
            u8 a = m_regs.a;
            if (!getFlag(FLAG_N)) {
                if (getFlag(FLAG_H) || (a & 0x0F) > 0x09) {
                    a += 0x06;
                }
                if (getFlag(FLAG_C) || a > 0x9F) {
                    a += 0x60;
                    setFlag(FLAG_C, true);
                } else {
                    setFlag(FLAG_C, false);  // FIX: Clear C when no adjustment
                }
            } else {
                if (getFlag(FLAG_H)) {
                    a = (a - 0x06) & 0xFF;
                }
                if (getFlag(FLAG_C)) {
                    a -= 0x60;
                }
            }
            m_regs.a = a;
            setFlag(FLAG_Z, a == 0);
            setFlag(FLAG_H, false);
            return 4;
        }
        
        // CPL (4 cycles)
        case 0x2F:
            m_regs.a = ~m_regs.a;
            setFlag(FLAG_N, true);
            setFlag(FLAG_H, true);
            return 4;
        
        // SCF (4 cycles)
        case 0x37:
            setFlag(FLAG_N, false);
            setFlag(FLAG_H, false);
            setFlag(FLAG_C, true);
            return 4;
        
        // CCF (4 cycles)
        case 0x3F:
            setFlag(FLAG_N, false);
            setFlag(FLAG_H, false);
            setFlag(FLAG_C, !getFlag(FLAG_C));
            return 4;
        
        // LD (HL+), A (8 cycles)
        case 0x22:
            write8(m_regs.hl, m_regs.a);
            m_regs.hl++;
            return 8;
        
        // LD (HL-), A (8 cycles)
        case 0x32:
            write8(m_regs.hl, m_regs.a);
            m_regs.hl--;
            return 8;
        
        // LD A, (HL+) (8 cycles)
        case 0x2A:
            m_regs.a = read8(m_regs.hl);
            m_regs.hl++;
            return 8;
        
        // LD A, (HL-) (8 cycles)
        case 0x3A:
            m_regs.a = read8(m_regs.hl);
            m_regs.hl--;
            return 8;
        
        // LD r, r' (4 cycles, except (HL) = 8 cycles)
        case 0x40: m_regs.b = m_regs.b; return 4;
        case 0x41: m_regs.b = m_regs.c; return 4;
        case 0x42: m_regs.b = m_regs.d; return 4;
        case 0x43: m_regs.b = m_regs.e; return 4;
        case 0x44: m_regs.b = m_regs.h; return 4;
        case 0x45: m_regs.b = m_regs.l; return 4;
        case 0x46: m_regs.b = read8(m_regs.hl); return 8;
        case 0x47: m_regs.b = m_regs.a; return 4;
        
        case 0x48: m_regs.c = m_regs.b; return 4;
        case 0x49: m_regs.c = m_regs.c; return 4;
        case 0x4A: m_regs.c = m_regs.d; return 4;
        case 0x4B: m_regs.c = m_regs.e; return 4;
        case 0x4C: m_regs.c = m_regs.h; return 4;
        case 0x4D: m_regs.c = m_regs.l; return 4;
        case 0x4E: m_regs.c = read8(m_regs.hl); return 8;
        case 0x4F: m_regs.c = m_regs.a; return 4;
        
        case 0x50: m_regs.d = m_regs.b; return 4;
        case 0x51: m_regs.d = m_regs.c; return 4;
        case 0x52: m_regs.d = m_regs.d; return 4;
        case 0x53: m_regs.d = m_regs.e; return 4;
        case 0x54: m_regs.d = m_regs.h; return 4;
        case 0x55: m_regs.d = m_regs.l; return 4;
        case 0x56: m_regs.d = read8(m_regs.hl); return 8;
        case 0x57: m_regs.d = m_regs.a; return 4;
        
        case 0x58: m_regs.e = m_regs.b; return 4;
        case 0x59: m_regs.e = m_regs.c; return 4;
        case 0x5A: m_regs.e = m_regs.d; return 4;
        case 0x5B: m_regs.e = m_regs.e; return 4;
        case 0x5C: m_regs.e = m_regs.h; return 4;
        case 0x5D: m_regs.e = m_regs.l; return 4;
        case 0x5E: m_regs.e = read8(m_regs.hl); return 8;
        case 0x5F: m_regs.e = m_regs.a; return 4;
        
        case 0x60: m_regs.h = m_regs.b; return 4;
        case 0x61: m_regs.h = m_regs.c; return 4;
        case 0x62: m_regs.h = m_regs.d; return 4;
        case 0x63: m_regs.h = m_regs.e; return 4;
        case 0x64: m_regs.h = m_regs.h; return 4;
        case 0x65: m_regs.h = m_regs.l; return 4;
        case 0x66: m_regs.h = read8(m_regs.hl); return 8;
        case 0x67: m_regs.h = m_regs.a; return 4;
        
        case 0x68: m_regs.l = m_regs.b; return 4;
        case 0x69: m_regs.l = m_regs.c; return 4;
        case 0x6A: m_regs.l = m_regs.d; return 4;
        case 0x6B: m_regs.l = m_regs.e; return 4;
        case 0x6C: m_regs.l = m_regs.h; return 4;
        case 0x6D: m_regs.l = m_regs.l; return 4;
        case 0x6E: m_regs.l = read8(m_regs.hl); return 8;
        case 0x6F: m_regs.l = m_regs.a; return 4;
        
        case 0x70: write8(m_regs.hl, m_regs.b); return 8;
        case 0x71: write8(m_regs.hl, m_regs.c); return 8;
        case 0x72: write8(m_regs.hl, m_regs.d); return 8;
        case 0x73: write8(m_regs.hl, m_regs.e); return 8;
        case 0x74: write8(m_regs.hl, m_regs.h); return 8;
        case 0x75: write8(m_regs.hl, m_regs.l); return 8;
        case 0x77: write8(m_regs.hl, m_regs.a); return 8;
        
        case 0x78: m_regs.a = m_regs.b; return 4;
        case 0x79: m_regs.a = m_regs.c; return 4;
        case 0x7A: m_regs.a = m_regs.d; return 4;
        case 0x7B: m_regs.a = m_regs.e; return 4;
        case 0x7C: m_regs.a = m_regs.h; return 4;
        case 0x7D: m_regs.a = m_regs.l; return 4;
        case 0x7E: m_regs.a = read8(m_regs.hl); return 8;
        case 0x7F: m_regs.a = m_regs.a; return 4;
        
        // HALT (4 cycles) - With HALT bug handling
        case 0x76:
            if (!m_ime) {
                u8 ie = m_mmu ? m_mmu->read(0xFFFF) : 0;
                if (m_if & ie & 0x1F) {
                    // HALT bug: PC doesn't increment on next fetch
                    m_haltBug = true;
                    return 4;
                }
            }
            m_halted = true;
            return 4;
        
        // ADD A, r (4 cycles for registers, 8 for (HL))
        case 0x80: case 0x81: case 0x82: case 0x83:
        case 0x84: case 0x85: case 0x86: case 0x87: {
            u8 value;
            bool isHL = false;
            switch (opcode & 0x07) {
                case 0: value = m_regs.b; break;
                case 1: value = m_regs.c; break;
                case 2: value = m_regs.d; break;
                case 3: value = m_regs.e; break;
                case 4: value = m_regs.h; break;
                case 5: value = m_regs.l; break;
                case 6: value = read8(m_regs.hl); isHL = true; break;
                default: value = m_regs.a; break;
            }
            u16 result = m_regs.a + value;
            setFlag(FLAG_H, ((m_regs.a & 0x0F) + (value & 0x0F)) > 0x0F);
            setFlag(FLAG_C, result > 0xFF);
            m_regs.a = result & 0xFF;
            setFlag(FLAG_Z, m_regs.a == 0);
            setFlag(FLAG_N, false);
            return isHL ? 8 : 4;
        }
        
        // ADC A, r (4 cycles for registers, 8 for (HL))
        case 0x88: case 0x89: case 0x8A: case 0x8B:
        case 0x8C: case 0x8D: case 0x8E: case 0x8F: {
            u8 value;
            bool isHL = false;
            switch (opcode & 0x07) {
                case 0: value = m_regs.b; break;
                case 1: value = m_regs.c; break;
                case 2: value = m_regs.d; break;
                case 3: value = m_regs.e; break;
                case 4: value = m_regs.h; break;
                case 5: value = m_regs.l; break;
                case 6: value = read8(m_regs.hl); isHL = true; break;
                default: value = m_regs.a; break;
            }
            u8 carry = getFlag(FLAG_C) ? 1 : 0;
            u16 result = m_regs.a + value + carry;
            setFlag(FLAG_H, ((m_regs.a & 0x0F) + (value & 0x0F) + carry) > 0x0F);
            setFlag(FLAG_C, result > 0xFF);
            m_regs.a = result & 0xFF;
            setFlag(FLAG_Z, m_regs.a == 0);
            setFlag(FLAG_N, false);
            return isHL ? 8 : 4;
        }
        
        // SUB r (4 cycles for registers, 8 for (HL))
        case 0x90: case 0x91: case 0x92: case 0x93:
        case 0x94: case 0x95: case 0x96: case 0x97: {
            u8 value;
            bool isHL = false;
            switch (opcode & 0x07) {
                case 0: value = m_regs.b; break;
                case 1: value = m_regs.c; break;
                case 2: value = m_regs.d; break;
                case 3: value = m_regs.e; break;
                case 4: value = m_regs.h; break;
                case 5: value = m_regs.l; break;
                case 6: value = read8(m_regs.hl); isHL = true; break;
                default: value = m_regs.a; break;
            }
            setFlag(FLAG_H, (m_regs.a & 0x0F) < (value & 0x0F));
            setFlag(FLAG_C, m_regs.a < value);
            m_regs.a -= value;
            setFlag(FLAG_Z, m_regs.a == 0);
            setFlag(FLAG_N, true);
            return isHL ? 8 : 4;
        }
        
        // SBC A, r (4 cycles for registers, 8 for (HL))
        case 0x98: case 0x99: case 0x9A: case 0x9B:
        case 0x9C: case 0x9D: case 0x9E: case 0x9F: {
            u8 value;
            bool isHL = false;
            switch (opcode & 0x07) {
                case 0: value = m_regs.b; break;
                case 1: value = m_regs.c; break;
                case 2: value = m_regs.d; break;
                case 3: value = m_regs.e; break;
                case 4: value = m_regs.h; break;
                case 5: value = m_regs.l; break;
                case 6: value = read8(m_regs.hl); isHL = true; break;
                default: value = m_regs.a; break;
            }
            u8 carry = getFlag(FLAG_C) ? 1 : 0;
            s16 result = m_regs.a - value - carry;
            setFlag(FLAG_H, ((m_regs.a & 0x0F) - (value & 0x0F) - carry) < 0);
            setFlag(FLAG_C, result < 0);
            m_regs.a = result & 0xFF;
            setFlag(FLAG_Z, m_regs.a == 0);
            setFlag(FLAG_N, true);
            return isHL ? 8 : 4;
        }
        
        // AND r (4 cycles for registers, 8 for (HL))
        case 0xA0: case 0xA1: case 0xA2: case 0xA3:
        case 0xA4: case 0xA5: case 0xA6: case 0xA7: {
            u8 value;
            bool isHL = false;
            switch (opcode & 0x07) {
                case 0: value = m_regs.b; break;
                case 1: value = m_regs.c; break;
                case 2: value = m_regs.d; break;
                case 3: value = m_regs.e; break;
                case 4: value = m_regs.h; break;
                case 5: value = m_regs.l; break;
                case 6: value = read8(m_regs.hl); isHL = true; break;
                default: value = m_regs.a; break;
            }
            m_regs.a &= value;
            setFlag(FLAG_Z, m_regs.a == 0);
            setFlag(FLAG_N, false);
            setFlag(FLAG_H, true);
            setFlag(FLAG_C, false);
            return isHL ? 8 : 4;
        }
        
        // XOR r (4 cycles for registers, 8 for (HL))
        case 0xA8: case 0xA9: case 0xAA: case 0xAB:
        case 0xAC: case 0xAD: case 0xAE: case 0xAF: {
            u8 value;
            bool isHL = false;
            switch (opcode & 0x07) {
                case 0: value = m_regs.b; break;
                case 1: value = m_regs.c; break;
                case 2: value = m_regs.d; break;
                case 3: value = m_regs.e; break;
                case 4: value = m_regs.h; break;
                case 5: value = m_regs.l; break;
                case 6: value = read8(m_regs.hl); isHL = true; break;
                default: value = m_regs.a; break;
            }
            m_regs.a ^= value;
            setFlag(FLAG_Z, m_regs.a == 0);
            setFlag(FLAG_N, false);
            setFlag(FLAG_H, false);
            setFlag(FLAG_C, false);
            return isHL ? 8 : 4;
        }
        
        // OR r (4 cycles for registers, 8 for (HL))
        case 0xB0: case 0xB1: case 0xB2: case 0xB3:
        case 0xB4: case 0xB5: case 0xB6: case 0xB7: {
            u8 value;
            bool isHL = false;
            switch (opcode & 0x07) {
                case 0: value = m_regs.b; break;
                case 1: value = m_regs.c; break;
                case 2: value = m_regs.d; break;
                case 3: value = m_regs.e; break;
                case 4: value = m_regs.h; break;
                case 5: value = m_regs.l; break;
                case 6: value = read8(m_regs.hl); isHL = true; break;
                default: value = m_regs.a; break;
            }
            m_regs.a |= value;
            setFlag(FLAG_Z, m_regs.a == 0);
            setFlag(FLAG_N, false);
            setFlag(FLAG_H, false);
            setFlag(FLAG_C, false);
            return isHL ? 8 : 4;
        }
        
        // CP r (4 cycles for registers, 8 for (HL))
        case 0xB8: case 0xB9: case 0xBA: case 0xBB:
        case 0xBC: case 0xBD: case 0xBE: case 0xBF: {
            u8 value;
            bool isHL = false;
            switch (opcode & 0x07) {
                case 0: value = m_regs.b; break;
                case 1: value = m_regs.c; break;
                case 2: value = m_regs.d; break;
                case 3: value = m_regs.e; break;
                case 4: value = m_regs.h; break;
                case 5: value = m_regs.l; break;
                case 6: value = read8(m_regs.hl); isHL = true; break;
                default: value = m_regs.a; break;
            }
            setFlag(FLAG_Z, m_regs.a == value);
            setFlag(FLAG_N, true);
            setFlag(FLAG_H, (m_regs.a & 0x0F) < (value & 0x0F));
            setFlag(FLAG_C, m_regs.a < value);
            return isHL ? 8 : 4;
        }
        
        // ADD A, n (8 cycles)
        case 0xC6: {
            u8 value = fetch8();
            u16 result = m_regs.a + value;
            setFlag(FLAG_H, ((m_regs.a & 0x0F) + (value & 0x0F)) > 0x0F);
            setFlag(FLAG_C, result > 0xFF);
            m_regs.a = result & 0xFF;
            setFlag(FLAG_Z, m_regs.a == 0);
            setFlag(FLAG_N, false);
            return 8;
        }
        
        // SUB n (8 cycles)
        case 0xD6: {
            u8 value = fetch8();
            setFlag(FLAG_H, (m_regs.a & 0x0F) < (value & 0x0F));
            setFlag(FLAG_C, m_regs.a < value);
            m_regs.a -= value;
            setFlag(FLAG_Z, m_regs.a == 0);
            setFlag(FLAG_N, true);
            return 8;
        }
        
        // AND n (8 cycles)
        case 0xE6: {
            m_regs.a &= fetch8();
            setFlag(FLAG_Z, m_regs.a == 0);
            setFlag(FLAG_N, false);
            setFlag(FLAG_H, true);
            setFlag(FLAG_C, false);
            return 8;
        }
        
        // OR n (8 cycles)
        case 0xF6: {
            m_regs.a |= fetch8();
            setFlag(FLAG_Z, m_regs.a == 0);
            setFlag(FLAG_N, false);
            setFlag(FLAG_H, false);
            setFlag(FLAG_C, false);
            return 8;
        }
        
        // ADC A, n (8 cycles)
        case 0xCE: {
            u8 value = fetch8();
            u8 carry = getFlag(FLAG_C) ? 1 : 0;
            u16 result = m_regs.a + value + carry;
            setFlag(FLAG_H, ((m_regs.a & 0x0F) + (value & 0x0F) + carry) > 0x0F);
            setFlag(FLAG_C, result > 0xFF);
            m_regs.a = result & 0xFF;
            setFlag(FLAG_Z, m_regs.a == 0);
            setFlag(FLAG_N, false);
            return 8;
        }
        
        // SBC A, n (8 cycles)
        case 0xDE: {
            u8 value = fetch8();
            u8 carry = getFlag(FLAG_C) ? 1 : 0;
            s16 result = m_regs.a - value - carry;
            setFlag(FLAG_H, ((m_regs.a & 0x0F) - (value & 0x0F) - carry) < 0);
            setFlag(FLAG_C, result < 0);
            m_regs.a = result & 0xFF;
            setFlag(FLAG_Z, m_regs.a == 0);
            setFlag(FLAG_N, true);
            return 8;
        }
        
        // XOR n (8 cycles)
        case 0xEE: {
            m_regs.a ^= fetch8();
            setFlag(FLAG_Z, m_regs.a == 0);
            setFlag(FLAG_N, false);
            setFlag(FLAG_H, false);
            setFlag(FLAG_C, false);
            return 8;
        }
        
        // CP n (8 cycles)
        case 0xFE: {
            u8 value = fetch8();
            setFlag(FLAG_Z, m_regs.a == value);
            setFlag(FLAG_N, true);
            setFlag(FLAG_H, (m_regs.a & 0x0F) < (value & 0x0F));
            setFlag(FLAG_C, m_regs.a < value);
            return 8;
        }
        
        // RET cc (20 if taken, 8 if not)
        case 0xC0: if (!getFlag(FLAG_Z)) { m_regs.pc = pop(); return 20; } return 8;
        case 0xC8: if (getFlag(FLAG_Z)) { m_regs.pc = pop(); return 20; } return 8;
        case 0xD0: if (!getFlag(FLAG_C)) { m_regs.pc = pop(); return 20; } return 8;
        case 0xD8: if (getFlag(FLAG_C)) { m_regs.pc = pop(); return 20; } return 8;
        
        // POP (12 cycles)
        case 0xC1: m_regs.bc = pop(); return 12;
        case 0xD1: m_regs.de = pop(); return 12;
        case 0xE1: m_regs.hl = pop(); return 12;
        case 0xF1: m_regs.af = pop() & 0xFFF0; return 12; // Lower 4 bits of F are always 0
        
        // JP cc, nn (16 if taken, 12 if not)
        case 0xC2: {
            u16 addr = fetch16();
            if (!getFlag(FLAG_Z)) { m_regs.pc = addr; return 16; }
            return 12;
        }
        case 0xCA: {
            u16 addr = fetch16();
            if (getFlag(FLAG_Z)) { m_regs.pc = addr; return 16; }
            return 12;
        }
        case 0xD2: {
            u16 addr = fetch16();
            if (!getFlag(FLAG_C)) { m_regs.pc = addr; return 16; }
            return 12;
        }
        case 0xDA: {
            u16 addr = fetch16();
            if (getFlag(FLAG_C)) { m_regs.pc = addr; return 16; }
            return 12;
        }
        
        // JP nn (16 cycles)
        case 0xC3:
            m_regs.pc = fetch16();
            return 16;
        
        // CALL cc, nn (24 if taken, 12 if not)
        case 0xC4: {
            u16 addr = fetch16();
            if (!getFlag(FLAG_Z)) {
                push(m_regs.pc);
                m_regs.pc = addr;
                return 24;
            }
            return 12;
        }
        case 0xCC: {
            u16 addr = fetch16();
            if (getFlag(FLAG_Z)) {
                push(m_regs.pc);
                m_regs.pc = addr;
                return 24;
            }
            return 12;
        }
        case 0xD4: {
            u16 addr = fetch16();
            if (!getFlag(FLAG_C)) {
                push(m_regs.pc);
                m_regs.pc = addr;
                return 24;
            }
            return 12;
        }
        case 0xDC: {
            u16 addr = fetch16();
            if (getFlag(FLAG_C)) {
                push(m_regs.pc);
                m_regs.pc = addr;
                return 24;
            }
            return 12;
        }
        
        // PUSH (16 cycles)
        case 0xC5: push(m_regs.bc); return 16;
        case 0xD5: push(m_regs.de); return 16;
        case 0xE5: push(m_regs.hl); return 16;
        case 0xF5: push(m_regs.af); return 16;
        
        // RST (16 cycles)
        case 0xC7: push(m_regs.pc); m_regs.pc = 0x00; return 16;
        case 0xCF: push(m_regs.pc); m_regs.pc = 0x08; return 16;
        case 0xD7: push(m_regs.pc); m_regs.pc = 0x10; return 16;
        case 0xDF: push(m_regs.pc); m_regs.pc = 0x18; return 16;
        case 0xE7: push(m_regs.pc); m_regs.pc = 0x20; return 16;
        case 0xEF: push(m_regs.pc); m_regs.pc = 0x28; return 16;
        case 0xF7: push(m_regs.pc); m_regs.pc = 0x30; return 16;
        case 0xFF: push(m_regs.pc); m_regs.pc = 0x38; return 16;
        
        // RET (16 cycles)
        case 0xC9:
            m_regs.pc = pop();
            return 16;
        
        // RETI (16 cycles)
        case 0xD9:
            m_regs.pc = pop();
            m_ime = true;
            return 16;
        
        // JP (HL) (4 cycles)
        case 0xE9:
            m_regs.pc = m_regs.hl;
            return 4;
        
        // LD SP, HL (8 cycles)
        case 0xF9:
            m_regs.sp = m_regs.hl;
            return 8;
        
        // CALL nn (24 cycles)
        case 0xCD: {
            u16 addr = fetch16();
            push(m_regs.pc);
            m_regs.pc = addr;
            return 24;
        }
        
        // CB prefix (returns cycles from CB instruction)
        case 0xCB:
            return executeCBInstruction(fetch8());
        
        // LDH (n), A (12 cycles)
        case 0xE0:
            write8(0xFF00 + fetch8(), m_regs.a);
            return 12;
        
        // LD (C), A (8 cycles)
        case 0xE2:
            write8(0xFF00 + m_regs.c, m_regs.a);
            return 8;
        
        // LDH A, (n) (12 cycles)
        case 0xF0:
            m_regs.a = read8(0xFF00 + fetch8());
            return 12;
        
        // LD A, (C) (8 cycles)
        case 0xF2:
            m_regs.a = read8(0xFF00 + m_regs.c);
            return 8;
        
        // LD (nn), A (16 cycles)
        case 0xEA:
            write8(fetch16(), m_regs.a);
            return 16;
        
        // LD A, (nn) (16 cycles)
        case 0xFA:
            m_regs.a = read8(fetch16());
            return 16;
        
        // DI (4 cycles)
        case 0xF3:
            m_ime = false;
            return 4;
        
        // EI (4 cycles)
        case 0xFB:
            m_enableIMENextInstruction = true;
            return 4;
        
        // ADD HL, rr (8 cycles)
        case 0x09: {
            u32 result = m_regs.hl + m_regs.bc;
            setFlag(FLAG_N, false);
            setFlag(FLAG_H, ((m_regs.hl & 0x0FFF) + (m_regs.bc & 0x0FFF)) > 0x0FFF);
            setFlag(FLAG_C, result > 0xFFFF);
            m_regs.hl = result & 0xFFFF;
            return 8;
        }
        case 0x19: {
            u32 result = m_regs.hl + m_regs.de;
            setFlag(FLAG_N, false);
            setFlag(FLAG_H, ((m_regs.hl & 0x0FFF) + (m_regs.de & 0x0FFF)) > 0x0FFF);
            setFlag(FLAG_C, result > 0xFFFF);
            m_regs.hl = result & 0xFFFF;
            return 8;
        }
        case 0x29: {
            u32 result = m_regs.hl + m_regs.hl;
            setFlag(FLAG_N, false);
            setFlag(FLAG_H, ((m_regs.hl & 0x0FFF) + (m_regs.hl & 0x0FFF)) > 0x0FFF);
            setFlag(FLAG_C, result > 0xFFFF);
            m_regs.hl = result & 0xFFFF;
            return 8;
        }
        case 0x39: {
            u32 result = m_regs.hl + m_regs.sp;
            setFlag(FLAG_N, false);
            setFlag(FLAG_H, ((m_regs.hl & 0x0FFF) + (m_regs.sp & 0x0FFF)) > 0x0FFF);
            setFlag(FLAG_C, result > 0xFFFF);
            m_regs.hl = result & 0xFFFF;
            return 8;
        }
        
        // ADD SP, n (16 cycles)
        case 0xE8: {
            s8 value = static_cast<s8>(fetch8());
            u32 result = m_regs.sp + value;
            setFlag(FLAG_Z, false);
            setFlag(FLAG_N, false);
            setFlag(FLAG_H, ((m_regs.sp & 0x0F) + (value & 0x0F)) > 0x0F);
            setFlag(FLAG_C, ((m_regs.sp & 0xFF) + (value & 0xFF)) > 0xFF);
            m_regs.sp = result & 0xFFFF;
            return 16;
        }
        
        // LD HL, SP+n (12 cycles)
        case 0xF8: {
            s8 value = static_cast<s8>(fetch8());
            u32 result = m_regs.sp + value;
            setFlag(FLAG_Z, false);
            setFlag(FLAG_N, false);
            setFlag(FLAG_H, ((m_regs.sp & 0x0F) + (value & 0x0F)) > 0x0F);
            setFlag(FLAG_C, ((m_regs.sp & 0xFF) + (value & 0xFF)) > 0xFF);
            m_regs.hl = result & 0xFFFF;
            return 12;
        }
        
        default:
            std::cerr << "Unknown opcode: 0x" << std::hex << (int)opcode << std::dec << std::endl;
            return 4;
    }
}

u32 CPU::executeCBInstruction(u8 opcode) {
    u8* reg = nullptr;
    u8 value = 0;
    bool useMemory = false;
    
    switch (opcode & 0x07) {
        case 0: reg = &m_regs.b; break;
        case 1: reg = &m_regs.c; break;
        case 2: reg = &m_regs.d; break;
        case 3: reg = &m_regs.e; break;
        case 4: reg = &m_regs.h; break;
        case 5: reg = &m_regs.l; break;
        case 6:
            value = read8(m_regs.hl);
            useMemory = true;
            break;
        case 7: reg = &m_regs.a; break;
    }
    
    if (!useMemory && reg) {
        value = *reg;
    }
    
    u8 bit = (opcode >> 3) & 0x07;
    
    if (opcode < 0x40) {
        // Rotate and shift instructions
        switch (opcode >> 3) {
            case 0: // RLC
            {
                bool carry = (value & 0x80) != 0;
                value = (value << 1) | (carry ? 1 : 0);
                setFlag(FLAG_Z, value == 0);
                setFlag(FLAG_N, false);
                setFlag(FLAG_H, false);
                setFlag(FLAG_C, carry);
                break;
            }
            case 1: // RRC
            {
                bool carry = (value & 0x01) != 0;
                value = (value >> 1) | (carry ? 0x80 : 0);
                setFlag(FLAG_Z, value == 0);
                setFlag(FLAG_N, false);
                setFlag(FLAG_H, false);
                setFlag(FLAG_C, carry);
                break;
            }
            case 2: // RL
            {
                bool oldCarry = getFlag(FLAG_C);
                bool newCarry = (value & 0x80) != 0;
                value = (value << 1) | (oldCarry ? 1 : 0);
                setFlag(FLAG_Z, value == 0);
                setFlag(FLAG_N, false);
                setFlag(FLAG_H, false);
                setFlag(FLAG_C, newCarry);
                break;
            }
            case 3: // RR
            {
                bool oldCarry = getFlag(FLAG_C);
                bool newCarry = (value & 0x01) != 0;
                value = (value >> 1) | (oldCarry ? 0x80 : 0);
                setFlag(FLAG_Z, value == 0);
                setFlag(FLAG_N, false);
                setFlag(FLAG_H, false);
                setFlag(FLAG_C, newCarry);
                break;
            }
            case 4: // SLA
            {
                bool carry = (value & 0x80) != 0;
                value <<= 1;
                setFlag(FLAG_Z, value == 0);
                setFlag(FLAG_N, false);
                setFlag(FLAG_H, false);
                setFlag(FLAG_C, carry);
                break;
            }
            case 5: // SRA
            {
                bool carry = (value & 0x01) != 0;
                value = (value >> 1) | (value & 0x80);
                setFlag(FLAG_Z, value == 0);
                setFlag(FLAG_N, false);
                setFlag(FLAG_H, false);
                setFlag(FLAG_C, carry);
                break;
            }
            case 6: // SWAP
            {
                value = ((value & 0x0F) << 4) | ((value & 0xF0) >> 4);
                setFlag(FLAG_Z, value == 0);
                setFlag(FLAG_N, false);
                setFlag(FLAG_H, false);
                setFlag(FLAG_C, false);
                break;
            }
            case 7: // SRL
            {
                bool carry = (value & 0x01) != 0;
                value >>= 1;
                setFlag(FLAG_Z, value == 0);
                setFlag(FLAG_N, false);
                setFlag(FLAG_H, false);
                setFlag(FLAG_C, carry);
                break;
            }
        }
    } else if (opcode < 0x80) {
        // BIT
        bool bitSet = (value & (1 << bit)) != 0;
        setFlag(FLAG_Z, !bitSet);
        setFlag(FLAG_N, false);
        setFlag(FLAG_H, true);
        // BIT doesn't write back, different cycle count
        return useMemory ? 12 : 8;
    } else if (opcode < 0xC0) {
        // RES
        value &= ~(1 << bit);
    } else {
        // SET
        value |= (1 << bit);
    }
    
    // Write back result (except for BIT, which returned above)
    if (opcode < 0x40 || opcode >= 0x80) {
        if (useMemory) {
            write8(m_regs.hl, value);
        } else if (reg) {
            *reg = value;
        }
    }
    
    // Return cycles: (HL) operations take more cycles
    // Rotate/Shift/RES/SET: 8 for registers, 16 for (HL)
    return useMemory ? 16 : 8;
}


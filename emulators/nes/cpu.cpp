#include "cpu.h"
#include "memory.h"
#include <iostream>

namespace nes {

// Cycle counts for each instruction (base cycles, not including page cross penalties)
static const u8 INSTRUCTION_CYCLES[256] = {
    7, 6, 2, 8, 3, 3, 5, 5, 3, 2, 2, 2, 4, 4, 6, 6,  // 0x00-0x0F
    2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,  // 0x10-0x1F
    6, 6, 2, 8, 3, 3, 5, 5, 4, 2, 2, 2, 4, 4, 6, 6,  // 0x20-0x2F
    2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,  // 0x30-0x3F
    6, 6, 2, 8, 3, 3, 5, 5, 3, 2, 2, 2, 3, 4, 6, 6,  // 0x40-0x4F
    2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,  // 0x50-0x5F
    6, 6, 2, 8, 3, 3, 5, 5, 4, 2, 2, 2, 5, 4, 6, 6,  // 0x60-0x6F
    2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,  // 0x70-0x7F
    2, 6, 2, 6, 3, 3, 3, 3, 2, 2, 2, 2, 4, 4, 4, 4,  // 0x80-0x8F
    2, 6, 2, 6, 4, 4, 4, 4, 2, 5, 2, 5, 5, 5, 5, 5,  // 0x90-0x9F
    2, 6, 2, 6, 3, 3, 3, 3, 2, 2, 2, 2, 4, 4, 4, 4,  // 0xA0-0xAF
    2, 5, 2, 5, 4, 4, 4, 4, 2, 4, 2, 4, 4, 4, 4, 4,  // 0xB0-0xBF
    2, 6, 2, 8, 3, 3, 5, 5, 2, 2, 2, 2, 4, 4, 6, 6,  // 0xC0-0xCF
    2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,  // 0xD0-0xDF
    2, 6, 2, 8, 3, 3, 5, 5, 2, 2, 2, 2, 4, 4, 6, 6,  // 0xE0-0xEF
    2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7   // 0xF0-0xFF
};

// Page cross penalty for instructions that have it
static const bool PAGE_CROSS_PENALTY[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0x00-0x0F
    1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0,  // 0x10-0x1F
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0x20-0x2F
    1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0,  // 0x30-0x3F
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0x40-0x4F
    1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0,  // 0x50-0x5F
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0x60-0x6F
    1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0,  // 0x70-0x7F
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0x80-0x8F
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0x90-0x9F
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0xA0-0xAF
    1, 1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1, 1,  // 0xB0-0xBF
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0xC0-0xCF
    1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0,  // 0xD0-0xDF
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0xE0-0xEF
    1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0   // 0xF0-0xFF
};

CPU::CPU()
    : m_memory(nullptr)
    , m_cycles(0)
    , m_stallCycles(0)
    , m_nmiPending(false)
    , m_irqPending(false) {
    reset();
}

void CPU::reset() {
    m_regs.a = 0;
    m_regs.x = 0;
    m_regs.y = 0;
    m_regs.s = 0xFD;  // Stack pointer starts at 0xFD after reset
    m_regs.p = 0x24;  // IRQ disabled, unused bit set
    
    // Read reset vector
    if (m_memory) {
        m_regs.pc = read16(0xFFFC);
    } else {
        m_regs.pc = 0x8000;  // Default if no memory
    }
    
    m_cycles = 0;
    m_stallCycles = 0;
    m_nmiPending = false;
    m_irqPending = false;
}

void CPU::step() {
    // Handle stall cycles (from DMA)
    if (m_stallCycles > 0) {
        m_stallCycles--;
        m_cycles++;
        return;
    }
    
    // Handle NMI (highest priority)
    if (m_nmiPending) {
        m_nmiPending = false;
        push16(m_regs.pc);
        push((m_regs.p | FLAG_U) & ~FLAG_B);  // Push P with U set, B clear
        setFlag(FLAG_I, true);
        m_regs.pc = read16(0xFFFA);
        m_cycles += 7;
        return;
    }
    
    // Handle IRQ
    if (m_irqPending && !getFlag(FLAG_I)) {
        m_irqPending = false;
        push16(m_regs.pc);
        push((m_regs.p | FLAG_U) & ~FLAG_B);
        setFlag(FLAG_I, true);
        m_regs.pc = read16(0xFFFE);
        m_cycles += 7;
        return;
    }
    
    executeInstruction();
}

void CPU::nmi() {
    m_nmiPending = true;
}

void CPU::irq() {
    m_irqPending = true;
}

void CPU::triggerOAMDMA(u8 page) {
    (void)page;  // Page is handled by memory.cpp DMA transfer
    
    // OAM DMA takes 513 or 514 cycles depending on odd/even cycle
    u32 dmaCycles = 513;
    if (m_cycles & 1) {
        dmaCycles = 514;  // Extra cycle on odd cycle
    }
    m_stallCycles += dmaCycles;
}

u8 CPU::read(u16 address) {
    if (m_memory) {
        return m_memory->cpuRead(address);
    }
    return 0;
}

void CPU::write(u16 address, u8 value) {
    if (m_memory) {
        m_memory->cpuWrite(address, value);
    }
}

u16 CPU::read16(u16 address) {
    u8 lo = read(address);
    u8 hi = read(address + 1);
    return (hi << 8) | lo;
}

u16 CPU::read16Bug(u16 address) {
    // Emulate the 6502 page boundary bug for indirect addressing
    // If address is $xxFF, high byte wraps within page instead of crossing
    u16 lo = address;
    u16 hi = (address & 0xFF00) | ((address + 1) & 0x00FF);
    return read(lo) | (read(hi) << 8);
}

void CPU::push(u8 value) {
    write(0x0100 | m_regs.s, value);
    m_regs.s--;
}

void CPU::push16(u16 value) {
    push((value >> 8) & 0xFF);
    push(value & 0xFF);
}

u8 CPU::pull() {
    m_regs.s++;
    return read(0x0100 | m_regs.s);
}

u16 CPU::pull16() {
    u8 lo = pull();
    u8 hi = pull();
    return (hi << 8) | lo;
}

void CPU::setFlag(u8 flag, bool value) {
    if (value) {
        m_regs.p |= flag;
    } else {
        m_regs.p &= ~flag;
    }
}

bool CPU::getFlag(u8 flag) const {
    return (m_regs.p & flag) != 0;
}

void CPU::setZN(u8 value) {
    setFlag(FLAG_Z, value == 0);
    setFlag(FLAG_N, (value & 0x80) != 0);
}

u16 CPU::getAddress(AddressMode mode, bool& pageCrossed) {
    pageCrossed = false;
    
    switch (mode) {
        case AddressMode::Implied:
        case AddressMode::Accumulator:
            return 0;
            
        case AddressMode::Immediate:
            return m_regs.pc++;
            
        case AddressMode::ZeroPage:
            return read(m_regs.pc++);
            
        case AddressMode::ZeroPageX:
            return (read(m_regs.pc++) + m_regs.x) & 0xFF;
            
        case AddressMode::ZeroPageY:
            return (read(m_regs.pc++) + m_regs.y) & 0xFF;
            
        case AddressMode::Absolute: {
            u16 addr = read16(m_regs.pc);
            m_regs.pc += 2;
            return addr;
        }
            
        case AddressMode::AbsoluteX: {
            u16 base = read16(m_regs.pc);
            m_regs.pc += 2;
            u16 addr = base + m_regs.x;
            pageCrossed = ((base & 0xFF00) != (addr & 0xFF00));
            return addr;
        }
            
        case AddressMode::AbsoluteY: {
            u16 base = read16(m_regs.pc);
            m_regs.pc += 2;
            u16 addr = base + m_regs.y;
            pageCrossed = ((base & 0xFF00) != (addr & 0xFF00));
            return addr;
        }
            
        case AddressMode::Indirect: {
            u16 ptr = read16(m_regs.pc);
            m_regs.pc += 2;
            return read16Bug(ptr);  // Bug: doesn't cross page boundary
        }
            
        case AddressMode::IndirectX: {
            u8 ptr = read(m_regs.pc++);
            u8 lo = read((ptr + m_regs.x) & 0xFF);
            u8 hi = read((ptr + m_regs.x + 1) & 0xFF);
            return (hi << 8) | lo;
        }
            
        case AddressMode::IndirectY: {
            u8 ptr = read(m_regs.pc++);
            u8 lo = read(ptr);
            u8 hi = read((ptr + 1) & 0xFF);
            u16 base = (hi << 8) | lo;
            u16 addr = base + m_regs.y;
            pageCrossed = ((base & 0xFF00) != (addr & 0xFF00));
            return addr;
        }
            
        case AddressMode::Relative: {
            s8 offset = static_cast<s8>(read(m_regs.pc++));
            return m_regs.pc + offset;
        }
    }
    
    return 0;
}

void CPU::executeInstruction() {
    u8 opcode = read(m_regs.pc++);
    m_cycles += INSTRUCTION_CYCLES[opcode];
    
    switch (opcode) {
        // ADC - Add with Carry
        case 0x69: ADC(AddressMode::Immediate); break;
        case 0x65: ADC(AddressMode::ZeroPage); break;
        case 0x75: ADC(AddressMode::ZeroPageX); break;
        case 0x6D: ADC(AddressMode::Absolute); break;
        case 0x7D: ADC(AddressMode::AbsoluteX); break;
        case 0x79: ADC(AddressMode::AbsoluteY); break;
        case 0x61: ADC(AddressMode::IndirectX); break;
        case 0x71: ADC(AddressMode::IndirectY); break;
        
        // AND - Logical AND
        case 0x29: AND(AddressMode::Immediate); break;
        case 0x25: AND(AddressMode::ZeroPage); break;
        case 0x35: AND(AddressMode::ZeroPageX); break;
        case 0x2D: AND(AddressMode::Absolute); break;
        case 0x3D: AND(AddressMode::AbsoluteX); break;
        case 0x39: AND(AddressMode::AbsoluteY); break;
        case 0x21: AND(AddressMode::IndirectX); break;
        case 0x31: AND(AddressMode::IndirectY); break;
        
        // ASL - Arithmetic Shift Left
        case 0x0A: ASL(AddressMode::Accumulator); break;
        case 0x06: ASL(AddressMode::ZeroPage); break;
        case 0x16: ASL(AddressMode::ZeroPageX); break;
        case 0x0E: ASL(AddressMode::Absolute); break;
        case 0x1E: ASL(AddressMode::AbsoluteX); break;
        
        // BCC - Branch if Carry Clear
        case 0x90: BCC(); break;
        
        // BCS - Branch if Carry Set
        case 0xB0: BCS(); break;
        
        // BEQ - Branch if Equal (Zero Set)
        case 0xF0: BEQ(); break;
        
        // BIT - Bit Test
        case 0x24: BIT(AddressMode::ZeroPage); break;
        case 0x2C: BIT(AddressMode::Absolute); break;
        
        // BMI - Branch if Minus (Negative Set)
        case 0x30: BMI(); break;
        
        // BNE - Branch if Not Equal (Zero Clear)
        case 0xD0: BNE(); break;
        
        // BPL - Branch if Plus (Negative Clear)
        case 0x10: BPL(); break;
        
        // BRK - Force Interrupt
        case 0x00: BRK(); break;
        
        // BVC - Branch if Overflow Clear
        case 0x50: BVC(); break;
        
        // BVS - Branch if Overflow Set
        case 0x70: BVS(); break;
        
        // CLC - Clear Carry Flag
        case 0x18: CLC(); break;
        
        // CLD - Clear Decimal Mode
        case 0xD8: CLD(); break;
        
        // CLI - Clear Interrupt Disable
        case 0x58: CLI(); break;
        
        // CLV - Clear Overflow Flag
        case 0xB8: CLV(); break;
        
        // CMP - Compare
        case 0xC9: CMP(AddressMode::Immediate); break;
        case 0xC5: CMP(AddressMode::ZeroPage); break;
        case 0xD5: CMP(AddressMode::ZeroPageX); break;
        case 0xCD: CMP(AddressMode::Absolute); break;
        case 0xDD: CMP(AddressMode::AbsoluteX); break;
        case 0xD9: CMP(AddressMode::AbsoluteY); break;
        case 0xC1: CMP(AddressMode::IndirectX); break;
        case 0xD1: CMP(AddressMode::IndirectY); break;
        
        // CPX - Compare X Register
        case 0xE0: CPX(AddressMode::Immediate); break;
        case 0xE4: CPX(AddressMode::ZeroPage); break;
        case 0xEC: CPX(AddressMode::Absolute); break;
        
        // CPY - Compare Y Register
        case 0xC0: CPY(AddressMode::Immediate); break;
        case 0xC4: CPY(AddressMode::ZeroPage); break;
        case 0xCC: CPY(AddressMode::Absolute); break;
        
        // DEC - Decrement Memory
        case 0xC6: DEC(AddressMode::ZeroPage); break;
        case 0xD6: DEC(AddressMode::ZeroPageX); break;
        case 0xCE: DEC(AddressMode::Absolute); break;
        case 0xDE: DEC(AddressMode::AbsoluteX); break;
        
        // DEX - Decrement X Register
        case 0xCA: DEX(); break;
        
        // DEY - Decrement Y Register
        case 0x88: DEY(); break;
        
        // EOR - Exclusive OR
        case 0x49: EOR(AddressMode::Immediate); break;
        case 0x45: EOR(AddressMode::ZeroPage); break;
        case 0x55: EOR(AddressMode::ZeroPageX); break;
        case 0x4D: EOR(AddressMode::Absolute); break;
        case 0x5D: EOR(AddressMode::AbsoluteX); break;
        case 0x59: EOR(AddressMode::AbsoluteY); break;
        case 0x41: EOR(AddressMode::IndirectX); break;
        case 0x51: EOR(AddressMode::IndirectY); break;
        
        // INC - Increment Memory
        case 0xE6: INC(AddressMode::ZeroPage); break;
        case 0xF6: INC(AddressMode::ZeroPageX); break;
        case 0xEE: INC(AddressMode::Absolute); break;
        case 0xFE: INC(AddressMode::AbsoluteX); break;
        
        // INX - Increment X Register
        case 0xE8: INX(); break;
        
        // INY - Increment Y Register
        case 0xC8: INY(); break;
        
        // JMP - Jump
        case 0x4C: JMP(AddressMode::Absolute); break;
        case 0x6C: JMP(AddressMode::Indirect); break;
        
        // JSR - Jump to Subroutine
        case 0x20: JSR(); break;
        
        // LDA - Load Accumulator
        case 0xA9: LDA(AddressMode::Immediate); break;
        case 0xA5: LDA(AddressMode::ZeroPage); break;
        case 0xB5: LDA(AddressMode::ZeroPageX); break;
        case 0xAD: LDA(AddressMode::Absolute); break;
        case 0xBD: LDA(AddressMode::AbsoluteX); break;
        case 0xB9: LDA(AddressMode::AbsoluteY); break;
        case 0xA1: LDA(AddressMode::IndirectX); break;
        case 0xB1: LDA(AddressMode::IndirectY); break;
        
        // LDX - Load X Register
        case 0xA2: LDX(AddressMode::Immediate); break;
        case 0xA6: LDX(AddressMode::ZeroPage); break;
        case 0xB6: LDX(AddressMode::ZeroPageY); break;
        case 0xAE: LDX(AddressMode::Absolute); break;
        case 0xBE: LDX(AddressMode::AbsoluteY); break;
        
        // LDY - Load Y Register
        case 0xA0: LDY(AddressMode::Immediate); break;
        case 0xA4: LDY(AddressMode::ZeroPage); break;
        case 0xB4: LDY(AddressMode::ZeroPageX); break;
        case 0xAC: LDY(AddressMode::Absolute); break;
        case 0xBC: LDY(AddressMode::AbsoluteX); break;
        
        // LSR - Logical Shift Right
        case 0x4A: LSR(AddressMode::Accumulator); break;
        case 0x46: LSR(AddressMode::ZeroPage); break;
        case 0x56: LSR(AddressMode::ZeroPageX); break;
        case 0x4E: LSR(AddressMode::Absolute); break;
        case 0x5E: LSR(AddressMode::AbsoluteX); break;
        
        // NOP - No Operation
        case 0xEA: NOP(); break;
        
        // ORA - Logical OR
        case 0x09: ORA(AddressMode::Immediate); break;
        case 0x05: ORA(AddressMode::ZeroPage); break;
        case 0x15: ORA(AddressMode::ZeroPageX); break;
        case 0x0D: ORA(AddressMode::Absolute); break;
        case 0x1D: ORA(AddressMode::AbsoluteX); break;
        case 0x19: ORA(AddressMode::AbsoluteY); break;
        case 0x01: ORA(AddressMode::IndirectX); break;
        case 0x11: ORA(AddressMode::IndirectY); break;
        
        // PHA - Push Accumulator
        case 0x48: PHA(); break;
        
        // PHP - Push Processor Status
        case 0x08: PHP(); break;
        
        // PLA - Pull Accumulator
        case 0x68: PLA(); break;
        
        // PLP - Pull Processor Status
        case 0x28: PLP(); break;
        
        // ROL - Rotate Left
        case 0x2A: ROL(AddressMode::Accumulator); break;
        case 0x26: ROL(AddressMode::ZeroPage); break;
        case 0x36: ROL(AddressMode::ZeroPageX); break;
        case 0x2E: ROL(AddressMode::Absolute); break;
        case 0x3E: ROL(AddressMode::AbsoluteX); break;
        
        // ROR - Rotate Right
        case 0x6A: ROR(AddressMode::Accumulator); break;
        case 0x66: ROR(AddressMode::ZeroPage); break;
        case 0x76: ROR(AddressMode::ZeroPageX); break;
        case 0x6E: ROR(AddressMode::Absolute); break;
        case 0x7E: ROR(AddressMode::AbsoluteX); break;
        
        // RTI - Return from Interrupt
        case 0x40: RTI(); break;
        
        // RTS - Return from Subroutine
        case 0x60: RTS(); break;
        
        // SBC - Subtract with Carry
        case 0xE9: SBC(AddressMode::Immediate); break;
        case 0xE5: SBC(AddressMode::ZeroPage); break;
        case 0xF5: SBC(AddressMode::ZeroPageX); break;
        case 0xED: SBC(AddressMode::Absolute); break;
        case 0xFD: SBC(AddressMode::AbsoluteX); break;
        case 0xF9: SBC(AddressMode::AbsoluteY); break;
        case 0xE1: SBC(AddressMode::IndirectX); break;
        case 0xF1: SBC(AddressMode::IndirectY); break;
        
        // SEC - Set Carry Flag
        case 0x38: SEC(); break;
        
        // SED - Set Decimal Flag
        case 0xF8: SED(); break;
        
        // SEI - Set Interrupt Disable
        case 0x78: SEI(); break;
        
        // STA - Store Accumulator
        case 0x85: STA(AddressMode::ZeroPage); break;
        case 0x95: STA(AddressMode::ZeroPageX); break;
        case 0x8D: STA(AddressMode::Absolute); break;
        case 0x9D: STA(AddressMode::AbsoluteX); break;
        case 0x99: STA(AddressMode::AbsoluteY); break;
        case 0x81: STA(AddressMode::IndirectX); break;
        case 0x91: STA(AddressMode::IndirectY); break;
        
        // STX - Store X Register
        case 0x86: STX(AddressMode::ZeroPage); break;
        case 0x96: STX(AddressMode::ZeroPageY); break;
        case 0x8E: STX(AddressMode::Absolute); break;
        
        // STY - Store Y Register
        case 0x84: STY(AddressMode::ZeroPage); break;
        case 0x94: STY(AddressMode::ZeroPageX); break;
        case 0x8C: STY(AddressMode::Absolute); break;
        
        // TAX - Transfer A to X
        case 0xAA: TAX(); break;
        
        // TAY - Transfer A to Y
        case 0xA8: TAY(); break;
        
        // TSX - Transfer Stack Pointer to X
        case 0xBA: TSX(); break;
        
        // TXA - Transfer X to A
        case 0x8A: TXA(); break;
        
        // TXS - Transfer X to Stack Pointer
        case 0x9A: TXS(); break;
        
        // TYA - Transfer Y to A
        case 0x98: TYA(); break;
        
        // ========== Unofficial opcodes ==========
        
        // LAX - LDA + LDX
        case 0xA7: LAX(AddressMode::ZeroPage); break;
        case 0xB7: LAX(AddressMode::ZeroPageY); break;
        case 0xAF: LAX(AddressMode::Absolute); break;
        case 0xBF: LAX(AddressMode::AbsoluteY); break;
        case 0xA3: LAX(AddressMode::IndirectX); break;
        case 0xB3: LAX(AddressMode::IndirectY); break;
        
        // SAX - Store A & X
        case 0x87: SAX(AddressMode::ZeroPage); break;
        case 0x97: SAX(AddressMode::ZeroPageY); break;
        case 0x8F: SAX(AddressMode::Absolute); break;
        case 0x83: SAX(AddressMode::IndirectX); break;
        
        // DCP - DEC + CMP
        case 0xC7: DCP(AddressMode::ZeroPage); break;
        case 0xD7: DCP(AddressMode::ZeroPageX); break;
        case 0xCF: DCP(AddressMode::Absolute); break;
        case 0xDF: DCP(AddressMode::AbsoluteX); break;
        case 0xDB: DCP(AddressMode::AbsoluteY); break;
        case 0xC3: DCP(AddressMode::IndirectX); break;
        case 0xD3: DCP(AddressMode::IndirectY); break;
        
        // ISB/ISC - INC + SBC
        case 0xE7: ISB(AddressMode::ZeroPage); break;
        case 0xF7: ISB(AddressMode::ZeroPageX); break;
        case 0xEF: ISB(AddressMode::Absolute); break;
        case 0xFF: ISB(AddressMode::AbsoluteX); break;
        case 0xFB: ISB(AddressMode::AbsoluteY); break;
        case 0xE3: ISB(AddressMode::IndirectX); break;
        case 0xF3: ISB(AddressMode::IndirectY); break;
        
        // SLO - ASL + ORA
        case 0x07: SLO(AddressMode::ZeroPage); break;
        case 0x17: SLO(AddressMode::ZeroPageX); break;
        case 0x0F: SLO(AddressMode::Absolute); break;
        case 0x1F: SLO(AddressMode::AbsoluteX); break;
        case 0x1B: SLO(AddressMode::AbsoluteY); break;
        case 0x03: SLO(AddressMode::IndirectX); break;
        case 0x13: SLO(AddressMode::IndirectY); break;
        
        // RLA - ROL + AND
        case 0x27: RLA(AddressMode::ZeroPage); break;
        case 0x37: RLA(AddressMode::ZeroPageX); break;
        case 0x2F: RLA(AddressMode::Absolute); break;
        case 0x3F: RLA(AddressMode::AbsoluteX); break;
        case 0x3B: RLA(AddressMode::AbsoluteY); break;
        case 0x23: RLA(AddressMode::IndirectX); break;
        case 0x33: RLA(AddressMode::IndirectY); break;
        
        // SRE - LSR + EOR
        case 0x47: SRE(AddressMode::ZeroPage); break;
        case 0x57: SRE(AddressMode::ZeroPageX); break;
        case 0x4F: SRE(AddressMode::Absolute); break;
        case 0x5F: SRE(AddressMode::AbsoluteX); break;
        case 0x5B: SRE(AddressMode::AbsoluteY); break;
        case 0x43: SRE(AddressMode::IndirectX); break;
        case 0x53: SRE(AddressMode::IndirectY); break;
        
        // RRA - ROR + ADC
        case 0x67: RRA(AddressMode::ZeroPage); break;
        case 0x77: RRA(AddressMode::ZeroPageX); break;
        case 0x6F: RRA(AddressMode::Absolute); break;
        case 0x7F: RRA(AddressMode::AbsoluteX); break;
        case 0x7B: RRA(AddressMode::AbsoluteY); break;
        case 0x63: RRA(AddressMode::IndirectX); break;
        case 0x73: RRA(AddressMode::IndirectY); break;
        
        // ANC - AND + set carry from bit 7
        case 0x0B:
        case 0x2B: ANC(AddressMode::Immediate); break;
        
        // ALR - AND + LSR
        case 0x4B: ALR(AddressMode::Immediate); break;
        
        // ARR - AND + ROR with weird flags
        case 0x6B: ARR(AddressMode::Immediate); break;
        
        // AXS - (A & X) - imm -> X
        case 0xCB: AXS(AddressMode::Immediate); break;
        
        // SHY - Store Y & (high byte + 1)
        case 0x9C: SHY(AddressMode::AbsoluteX); break;
        
        // SHX - Store X & (high byte + 1)
        case 0x9E: SHX(AddressMode::AbsoluteY); break;
        
        // Unofficial NOPs (various addressing modes)
        case 0x1A: case 0x3A: case 0x5A: case 0x7A: case 0xDA: case 0xFA:
            NOP(); break;  // 1-byte NOPs
        case 0x80: case 0x82: case 0x89: case 0xC2: case 0xE2:
            m_regs.pc++; break;  // 2-byte NOPs (skip immediate)
        case 0x04: case 0x44: case 0x64:
            m_regs.pc++; break;  // 2-byte NOPs (zero page)
        case 0x14: case 0x34: case 0x54: case 0x74: case 0xD4: case 0xF4:
            m_regs.pc++; break;  // 2-byte NOPs (zero page X)
        case 0x0C:
            m_regs.pc += 2; break;  // 3-byte NOP (absolute)
        case 0x1C: case 0x3C: case 0x5C: case 0x7C: case 0xDC: case 0xFC: {
            bool pageCrossed;
            getAddress(AddressMode::AbsoluteX, pageCrossed);
            if (pageCrossed) m_cycles++;
            break;  // 3-byte NOPs (absolute X) with page cross penalty
        }
        
        // Unofficial SBC (same as official)
        case 0xEB: SBC(AddressMode::Immediate); break;
        
        // KIL/JAM opcodes - halt the CPU (we treat as NOP for compatibility)
        case 0x02: case 0x12: case 0x22: case 0x32: case 0x42: case 0x52:
        case 0x62: case 0x72: case 0x92: case 0xB2: case 0xD2: case 0xF2:
            break;  // Do nothing
        
        default:
            // Unknown opcode - treat as NOP
            break;
    }
}

// ========== Instruction implementations ==========

void CPU::LDA(AddressMode mode) {
    bool pageCrossed;
    u16 addr = getAddress(mode, pageCrossed);
    m_regs.a = read(addr);
    setZN(m_regs.a);
    if (pageCrossed && PAGE_CROSS_PENALTY[0xA9]) m_cycles++;
}

void CPU::LDX(AddressMode mode) {
    bool pageCrossed;
    u16 addr = getAddress(mode, pageCrossed);
    m_regs.x = read(addr);
    setZN(m_regs.x);
    if (pageCrossed && PAGE_CROSS_PENALTY[0xA2]) m_cycles++;
}

void CPU::LDY(AddressMode mode) {
    bool pageCrossed;
    u16 addr = getAddress(mode, pageCrossed);
    m_regs.y = read(addr);
    setZN(m_regs.y);
    if (pageCrossed && PAGE_CROSS_PENALTY[0xA0]) m_cycles++;
}

void CPU::STA(AddressMode mode) {
    bool pageCrossed;
    u16 addr = getAddress(mode, pageCrossed);
    write(addr, m_regs.a);
}

void CPU::STX(AddressMode mode) {
    bool pageCrossed;
    u16 addr = getAddress(mode, pageCrossed);
    write(addr, m_regs.x);
}

void CPU::STY(AddressMode mode) {
    bool pageCrossed;
    u16 addr = getAddress(mode, pageCrossed);
    write(addr, m_regs.y);
}

void CPU::TAX() {
    m_regs.x = m_regs.a;
    setZN(m_regs.x);
}

void CPU::TAY() {
    m_regs.y = m_regs.a;
    setZN(m_regs.y);
}

void CPU::TXA() {
    m_regs.a = m_regs.x;
    setZN(m_regs.a);
}

void CPU::TYA() {
    m_regs.a = m_regs.y;
    setZN(m_regs.a);
}

void CPU::TSX() {
    m_regs.x = m_regs.s;
    setZN(m_regs.x);
}

void CPU::TXS() {
    m_regs.s = m_regs.x;
}

void CPU::PHA() {
    push(m_regs.a);
}

void CPU::PHP() {
    push(m_regs.p | FLAG_B | FLAG_U);  // B and U always pushed as 1
}

void CPU::PLA() {
    m_regs.a = pull();
    setZN(m_regs.a);
}

void CPU::PLP() {
    m_regs.p = (pull() & ~FLAG_B) | FLAG_U;  // B ignored, U always 1
}

void CPU::ADC(AddressMode mode) {
    bool pageCrossed;
    u16 addr = getAddress(mode, pageCrossed);
    u8 value = read(addr);
    
    u16 sum = m_regs.a + value + (getFlag(FLAG_C) ? 1 : 0);
    
    setFlag(FLAG_C, sum > 0xFF);
    setFlag(FLAG_V, (~(m_regs.a ^ value) & (m_regs.a ^ sum) & 0x80) != 0);
    
    m_regs.a = sum & 0xFF;
    setZN(m_regs.a);
    
    if (pageCrossed) m_cycles++;
}

void CPU::SBC(AddressMode mode) {
    bool pageCrossed;
    u16 addr = getAddress(mode, pageCrossed);
    u8 value = read(addr);
    
    u16 diff = m_regs.a - value - (getFlag(FLAG_C) ? 0 : 1);
    
    setFlag(FLAG_C, diff < 0x100);
    setFlag(FLAG_V, ((m_regs.a ^ value) & (m_regs.a ^ diff) & 0x80) != 0);
    
    m_regs.a = diff & 0xFF;
    setZN(m_regs.a);
    
    if (pageCrossed) m_cycles++;
}

void CPU::CMP(AddressMode mode) {
    bool pageCrossed;
    u16 addr = getAddress(mode, pageCrossed);
    u8 value = read(addr);
    
    setFlag(FLAG_C, m_regs.a >= value);
    setZN(m_regs.a - value);
    
    if (pageCrossed) m_cycles++;
}

void CPU::CPX(AddressMode mode) {
    bool pageCrossed;
    u16 addr = getAddress(mode, pageCrossed);
    u8 value = read(addr);
    
    setFlag(FLAG_C, m_regs.x >= value);
    setZN(m_regs.x - value);
}

void CPU::CPY(AddressMode mode) {
    bool pageCrossed;
    u16 addr = getAddress(mode, pageCrossed);
    u8 value = read(addr);
    
    setFlag(FLAG_C, m_regs.y >= value);
    setZN(m_regs.y - value);
}

void CPU::INC(AddressMode mode) {
    bool pageCrossed;
    u16 addr = getAddress(mode, pageCrossed);
    u8 value = read(addr) + 1;
    write(addr, value);
    setZN(value);
}

void CPU::INX() {
    m_regs.x++;
    setZN(m_regs.x);
}

void CPU::INY() {
    m_regs.y++;
    setZN(m_regs.y);
}

void CPU::DEC(AddressMode mode) {
    bool pageCrossed;
    u16 addr = getAddress(mode, pageCrossed);
    u8 value = read(addr) - 1;
    write(addr, value);
    setZN(value);
}

void CPU::DEX() {
    m_regs.x--;
    setZN(m_regs.x);
}

void CPU::DEY() {
    m_regs.y--;
    setZN(m_regs.y);
}

void CPU::AND(AddressMode mode) {
    bool pageCrossed;
    u16 addr = getAddress(mode, pageCrossed);
    m_regs.a &= read(addr);
    setZN(m_regs.a);
    if (pageCrossed) m_cycles++;
}

void CPU::ORA(AddressMode mode) {
    bool pageCrossed;
    u16 addr = getAddress(mode, pageCrossed);
    m_regs.a |= read(addr);
    setZN(m_regs.a);
    if (pageCrossed) m_cycles++;
}

void CPU::EOR(AddressMode mode) {
    bool pageCrossed;
    u16 addr = getAddress(mode, pageCrossed);
    m_regs.a ^= read(addr);
    setZN(m_regs.a);
    if (pageCrossed) m_cycles++;
}

void CPU::BIT(AddressMode mode) {
    bool pageCrossed;
    u16 addr = getAddress(mode, pageCrossed);
    u8 value = read(addr);
    
    setFlag(FLAG_Z, (m_regs.a & value) == 0);
    setFlag(FLAG_V, (value & 0x40) != 0);
    setFlag(FLAG_N, (value & 0x80) != 0);
}

void CPU::ASL(AddressMode mode) {
    if (mode == AddressMode::Accumulator) {
        setFlag(FLAG_C, (m_regs.a & 0x80) != 0);
        m_regs.a <<= 1;
        setZN(m_regs.a);
    } else {
        bool pageCrossed;
        u16 addr = getAddress(mode, pageCrossed);
        u8 value = read(addr);
        setFlag(FLAG_C, (value & 0x80) != 0);
        value <<= 1;
        write(addr, value);
        setZN(value);
    }
}

void CPU::LSR(AddressMode mode) {
    if (mode == AddressMode::Accumulator) {
        setFlag(FLAG_C, (m_regs.a & 0x01) != 0);
        m_regs.a >>= 1;
        setZN(m_regs.a);
    } else {
        bool pageCrossed;
        u16 addr = getAddress(mode, pageCrossed);
        u8 value = read(addr);
        setFlag(FLAG_C, (value & 0x01) != 0);
        value >>= 1;
        write(addr, value);
        setZN(value);
    }
}

void CPU::ROL(AddressMode mode) {
    bool carry = getFlag(FLAG_C);
    
    if (mode == AddressMode::Accumulator) {
        setFlag(FLAG_C, (m_regs.a & 0x80) != 0);
        m_regs.a = (m_regs.a << 1) | (carry ? 1 : 0);
        setZN(m_regs.a);
    } else {
        bool pageCrossed;
        u16 addr = getAddress(mode, pageCrossed);
        u8 value = read(addr);
        setFlag(FLAG_C, (value & 0x80) != 0);
        value = (value << 1) | (carry ? 1 : 0);
        write(addr, value);
        setZN(value);
    }
}

void CPU::ROR(AddressMode mode) {
    bool carry = getFlag(FLAG_C);
    
    if (mode == AddressMode::Accumulator) {
        setFlag(FLAG_C, (m_regs.a & 0x01) != 0);
        m_regs.a = (m_regs.a >> 1) | (carry ? 0x80 : 0);
        setZN(m_regs.a);
    } else {
        bool pageCrossed;
        u16 addr = getAddress(mode, pageCrossed);
        u8 value = read(addr);
        setFlag(FLAG_C, (value & 0x01) != 0);
        value = (value >> 1) | (carry ? 0x80 : 0);
        write(addr, value);
        setZN(value);
    }
}

void CPU::JMP(AddressMode mode) {
    bool pageCrossed;
    m_regs.pc = getAddress(mode, pageCrossed);
}

void CPU::JSR() {
    push16(m_regs.pc + 1);  // Push address of next instruction - 1
    m_regs.pc = read16(m_regs.pc);
}

void CPU::RTS() {
    m_regs.pc = pull16() + 1;
}

void CPU::RTI() {
    m_regs.p = (pull() & ~FLAG_B) | FLAG_U;
    m_regs.pc = pull16();
}

void CPU::branch(bool condition) {
    s8 offset = static_cast<s8>(read(m_regs.pc++));
    
    if (condition) {
        u16 oldPC = m_regs.pc;
        m_regs.pc += offset;
        m_cycles++;  // Branch taken penalty
        
        // Extra cycle if page boundary crossed
        if ((oldPC & 0xFF00) != (m_regs.pc & 0xFF00)) {
            m_cycles++;
        }
    }
}

void CPU::BCC() { branch(!getFlag(FLAG_C)); }
void CPU::BCS() { branch(getFlag(FLAG_C)); }
void CPU::BEQ() { branch(getFlag(FLAG_Z)); }
void CPU::BMI() { branch(getFlag(FLAG_N)); }
void CPU::BNE() { branch(!getFlag(FLAG_Z)); }
void CPU::BPL() { branch(!getFlag(FLAG_N)); }
void CPU::BVC() { branch(!getFlag(FLAG_V)); }
void CPU::BVS() { branch(getFlag(FLAG_V)); }

void CPU::CLC() { setFlag(FLAG_C, false); }
void CPU::CLD() { setFlag(FLAG_D, false); }
void CPU::CLI() { setFlag(FLAG_I, false); }
void CPU::CLV() { setFlag(FLAG_V, false); }
void CPU::SEC() { setFlag(FLAG_C, true); }
void CPU::SED() { setFlag(FLAG_D, true); }
void CPU::SEI() { setFlag(FLAG_I, true); }

void CPU::BRK() {
    m_regs.pc++;  // BRK skips the next byte
    push16(m_regs.pc);
    push(m_regs.p | FLAG_B | FLAG_U);
    setFlag(FLAG_I, true);
    m_regs.pc = read16(0xFFFE);
}

void CPU::NOP() {
    // Do nothing
}

// ========== Unofficial opcodes ==========

void CPU::LAX(AddressMode mode) {
    bool pageCrossed;
    u16 addr = getAddress(mode, pageCrossed);
    m_regs.a = m_regs.x = read(addr);
    setZN(m_regs.a);
    if (pageCrossed) m_cycles++;
}

void CPU::SAX(AddressMode mode) {
    bool pageCrossed;
    u16 addr = getAddress(mode, pageCrossed);
    write(addr, m_regs.a & m_regs.x);
}

void CPU::DCP(AddressMode mode) {
    bool pageCrossed;
    u16 addr = getAddress(mode, pageCrossed);
    u8 value = read(addr) - 1;
    write(addr, value);
    setFlag(FLAG_C, m_regs.a >= value);
    setZN(m_regs.a - value);
}

void CPU::ISB(AddressMode mode) {
    bool pageCrossed;
    u16 addr = getAddress(mode, pageCrossed);
    u8 value = read(addr) + 1;
    write(addr, value);
    
    // SBC
    u16 diff = m_regs.a - value - (getFlag(FLAG_C) ? 0 : 1);
    setFlag(FLAG_C, diff < 0x100);
    setFlag(FLAG_V, ((m_regs.a ^ value) & (m_regs.a ^ diff) & 0x80) != 0);
    m_regs.a = diff & 0xFF;
    setZN(m_regs.a);
}

void CPU::SLO(AddressMode mode) {
    bool pageCrossed;
    u16 addr = getAddress(mode, pageCrossed);
    u8 value = read(addr);
    setFlag(FLAG_C, (value & 0x80) != 0);
    value <<= 1;
    write(addr, value);
    m_regs.a |= value;
    setZN(m_regs.a);
}

void CPU::RLA(AddressMode mode) {
    bool pageCrossed;
    u16 addr = getAddress(mode, pageCrossed);
    u8 value = read(addr);
    bool carry = getFlag(FLAG_C);
    setFlag(FLAG_C, (value & 0x80) != 0);
    value = (value << 1) | (carry ? 1 : 0);
    write(addr, value);
    m_regs.a &= value;
    setZN(m_regs.a);
}

void CPU::SRE(AddressMode mode) {
    bool pageCrossed;
    u16 addr = getAddress(mode, pageCrossed);
    u8 value = read(addr);
    setFlag(FLAG_C, (value & 0x01) != 0);
    value >>= 1;
    write(addr, value);
    m_regs.a ^= value;
    setZN(m_regs.a);
}

void CPU::RRA(AddressMode mode) {
    bool pageCrossed;
    u16 addr = getAddress(mode, pageCrossed);
    u8 value = read(addr);
    bool carry = getFlag(FLAG_C);
    setFlag(FLAG_C, (value & 0x01) != 0);
    value = (value >> 1) | (carry ? 0x80 : 0);
    write(addr, value);
    
    // ADC
    u16 sum = m_regs.a + value + (getFlag(FLAG_C) ? 1 : 0);
    setFlag(FLAG_C, sum > 0xFF);
    setFlag(FLAG_V, (~(m_regs.a ^ value) & (m_regs.a ^ sum) & 0x80) != 0);
    m_regs.a = sum & 0xFF;
    setZN(m_regs.a);
}

void CPU::ANC(AddressMode mode) {
    bool pageCrossed;
    u16 addr = getAddress(mode, pageCrossed);
    m_regs.a &= read(addr);
    setZN(m_regs.a);
    setFlag(FLAG_C, (m_regs.a & 0x80) != 0);
}

void CPU::ALR(AddressMode mode) {
    bool pageCrossed;
    u16 addr = getAddress(mode, pageCrossed);
    m_regs.a &= read(addr);
    setFlag(FLAG_C, (m_regs.a & 0x01) != 0);
    m_regs.a >>= 1;
    setZN(m_regs.a);
}

void CPU::ARR(AddressMode mode) {
    bool pageCrossed;
    u16 addr = getAddress(mode, pageCrossed);
    m_regs.a &= read(addr);
    m_regs.a = (m_regs.a >> 1) | (getFlag(FLAG_C) ? 0x80 : 0);
    setZN(m_regs.a);
    setFlag(FLAG_C, (m_regs.a & 0x40) != 0);
    setFlag(FLAG_V, ((m_regs.a & 0x40) ^ ((m_regs.a & 0x20) << 1)) != 0);
}

void CPU::AXS(AddressMode mode) {
    bool pageCrossed;
    u16 addr = getAddress(mode, pageCrossed);
    u8 value = read(addr);
    u8 result = (m_regs.a & m_regs.x) - value;
    setFlag(FLAG_C, (m_regs.a & m_regs.x) >= value);
    m_regs.x = result;
    setZN(m_regs.x);
}

void CPU::SHY(AddressMode mode) {
    bool pageCrossed;
    u16 addr = getAddress(mode, pageCrossed);
    u8 hi = (addr >> 8) + 1;
    u8 value = m_regs.y & hi;
    // Weird behavior when page cross occurs
    if (pageCrossed) {
        addr = (value << 8) | (addr & 0xFF);
    }
    write(addr, value);
}

void CPU::SHX(AddressMode mode) {
    bool pageCrossed;
    u16 addr = getAddress(mode, pageCrossed);
    u8 hi = (addr >> 8) + 1;
    u8 value = m_regs.x & hi;
    // Weird behavior when page cross occurs
    if (pageCrossed) {
        addr = (value << 8) | (addr & 0xFF);
    }
    write(addr, value);
}

void CPU::saveState(std::ofstream& file) const {
    file.write(reinterpret_cast<const char*>(&m_regs), sizeof(m_regs));
    file.write(reinterpret_cast<const char*>(&m_cycles), sizeof(m_cycles));
    file.write(reinterpret_cast<const char*>(&m_stallCycles), sizeof(m_stallCycles));
    file.write(reinterpret_cast<const char*>(&m_nmiPending), sizeof(m_nmiPending));
    file.write(reinterpret_cast<const char*>(&m_irqPending), sizeof(m_irqPending));
}

void CPU::loadState(std::ifstream& file) {
    file.read(reinterpret_cast<char*>(&m_regs), sizeof(m_regs));
    file.read(reinterpret_cast<char*>(&m_cycles), sizeof(m_cycles));
    file.read(reinterpret_cast<char*>(&m_stallCycles), sizeof(m_stallCycles));
    file.read(reinterpret_cast<char*>(&m_nmiPending), sizeof(m_nmiPending));
    file.read(reinterpret_cast<char*>(&m_irqPending), sizeof(m_irqPending));
}

} // namespace nes

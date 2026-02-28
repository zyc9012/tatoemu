#include "m6502.h"
#include <cstring>

// ========================================
// Constructor and Initialization
// ========================================

M6502::M6502(Variant variant)
    : m_variant(variant)
    , m_pc(0)
    , m_prevPC(0)
    , m_a(0)
    , m_x(0)
    , m_y(0)
    , m_s(0xFF)
    , m_p(static_cast<uint8_t>(StatusFlag::U) | static_cast<uint8_t>(StatusFlag::I))
    , m_irqState(0)
    , m_nmiState(0)
    , m_pendingIRQ(false)
    , m_pendingNMI(false)
    , m_afterCLI(false)
    , m_holdIRQ(false)
    , m_holdNMI(false)
    , m_nmiDelay(0)
    , m_cyclesRemaining(0)
    , m_cyclesExecuted(0)
    , m_shouldStop(false)
    , m_fetchingOpcode(false)
{
}

void M6502::reset() {
    // Read reset vector from $FFFC-$FFFD
    uint8_t lo = m_mem.read(0xFFFC, m_mem.userData);
    uint8_t hi = m_mem.read(0xFFFD, m_mem.userData);
    m_pc = (hi << 8) | lo;
    
    // Initialize stack pointer to top of stack
    m_s = 0xFF;
    
    // Set initial processor status: Interrupt disable, Unused flag
    m_p = static_cast<uint8_t>(StatusFlag::U) | 
          static_cast<uint8_t>(StatusFlag::I);
    
    // Clear registers
    m_a = 0;
    m_x = 0;
    m_y = 0;
    
    // Clear interrupt state
    m_irqState = 0;
    m_nmiState = 0;
    m_pendingIRQ = false;
    m_pendingNMI = false;
    m_afterCLI = false;
    m_holdIRQ = false;
    m_holdNMI = false;
    m_nmiDelay = 0;
    
    m_cyclesExecuted = 0;
    m_shouldStop = false;
}

// ========================================
// Execution Loop
// ========================================

int M6502::execute(int cycles) {
    m_cyclesRemaining = cycles;
    m_shouldStop = false;
    
    while (m_cyclesRemaining > 0 && !m_shouldStop) {
        // Save PC for debugging
        m_prevPC = m_pc;
        
        // Handle NMI delay (NES specific)
        if (m_nmiDelay > 0) {
            m_nmiDelay--;
            if (m_nmiDelay == 0) {
                serviceNMI();
            }
        }
        
        // Check for pending interrupts before fetching next instruction
        if (m_pendingIRQ) {
            serviceIRQ();
        }
        
        // Fetch and execute instruction
        uint8_t opcode = readOpcode();
        executeInstruction(opcode);
        
        // Check for pending NMI after instruction execution
        if (m_pendingNMI) {
            serviceNMI();
        }
        
        // Handle CLI edge case: IRQ can be triggered after CLI
        if (m_afterCLI) {
            m_afterCLI = false;
            if (m_irqState != static_cast<uint8_t>(LineState::CLEAR)) {
                m_pendingIRQ = true;
            }
        }
        else {
            // Check for pending interrupts after instruction execution
            if (m_pendingIRQ) {
                serviceIRQ();
            }
        }
    }
    
    int executed = cycles - m_cyclesRemaining;
    m_cyclesExecuted += executed;
    return executed;
}

// ========================================
// Interrupt Handling
// ========================================

void M6502::serviceNMI() {
    // NMI sequence: 7 cycles total
    // 1. Fetch opcode (already done)
    // 2-3. Push PCH, PCL
    // 4. Push P (with B=0)
    // 5. Set I flag
    // 6-7. Load NMI vector
    
    if (m_holdNMI) {
        m_holdNMI = false;
        m_nmiState = static_cast<uint8_t>(LineState::CLEAR);
    }
    
    m_pendingNMI = false;
    
    m_cyclesRemaining -= 2;  // Extra cycles for interrupt
    push16(m_pc);
    push(m_p & ~static_cast<uint8_t>(StatusFlag::B));
    setFlag(StatusFlag::I, true);
    
    uint8_t lo = read(0xFFFA);
    uint8_t hi = read(0xFFFB);
    m_pc = (hi << 8) | lo;
}

void M6502::serviceIRQ() {
    // Only service IRQ if interrupt disable flag is clear
    if (!getFlag(StatusFlag::I)) {
        m_cyclesRemaining -= 2;  // Extra cycles for interrupt
        push16(m_pc);
        push(m_p & ~static_cast<uint8_t>(StatusFlag::B));
        setFlag(StatusFlag::I, true);
        
        if (m_holdIRQ) {
            m_holdIRQ = false;
            m_irqState = static_cast<uint8_t>(LineState::CLEAR);
        }
        
        uint8_t lo = read(0xFFFE);
        uint8_t hi = read(0xFFFF);
        m_pc = (hi << 8) | lo;
    }
    
    m_pendingIRQ = false;
}

void M6502::setInterruptLine(InterruptLine line, LineState state) {
    uint8_t stateValue = static_cast<uint8_t>(state);
    
    if (line == InterruptLine::NMI) {
        // NMI is edge-triggered
        if (m_nmiState == static_cast<uint8_t>(LineState::CLEAR) && 
            stateValue == static_cast<uint8_t>(LineState::ASSERT)) {
            m_pendingNMI = true;
        }
        m_nmiState = stateValue;
    } else if (line == InterruptLine::IRQ) {
        // IRQ is level-triggered
        m_irqState = stateValue;
        if (stateValue == static_cast<uint8_t>(LineState::ASSERT)) {
            m_pendingIRQ = true;
        }
    }
}

// ========================================
// State Management
// ========================================

void M6502::getContext(void* dst) const {
    if (!dst) return;
    State st;
    st.pc = m_pc;
    st.prevPC = m_prevPC;
    st.a = m_a;
    st.x = m_x;
    st.y = m_y;
    st.s = m_s;
    st.p = m_p;
    st.irqState = m_irqState;
    st.nmiState = m_nmiState;
    st.pendingIRQ = m_pendingIRQ;
    st.pendingNMI = m_pendingNMI;
    st.afterCLI = m_afterCLI;
    st.holdIRQ = m_holdIRQ;
    st.holdNMI = m_holdNMI;
    st.nmiDelay = m_nmiDelay;
    st.cyclesExecuted = m_cyclesExecuted;
    std::memcpy(dst, &st, sizeof(st));
}

void M6502::setContext(const void* src) {
    if (!src) return;
    State st;
    std::memcpy(&st, src, sizeof(st));
    m_pc = st.pc;
    m_prevPC = st.prevPC;
    m_a = st.a;
    m_x = st.x;
    m_y = st.y;
    m_s = st.s;
    m_p = st.p;
    m_irqState = st.irqState;
    m_nmiState = st.nmiState;
    m_pendingIRQ = st.pendingIRQ;
    m_pendingNMI = st.pendingNMI;
    m_afterCLI = st.afterCLI;
    m_holdIRQ = st.holdIRQ;
    m_holdNMI = st.holdNMI;
    m_nmiDelay = st.nmiDelay;
    m_cyclesExecuted = st.cyclesExecuted;
}

// ========================================
// Load/Store Instructions
// ========================================

/**
 * LDA - Load Accumulator
 * Loads a byte into the accumulator.
 * Flags: N, Z
 */
void M6502::op_LDA(uint8_t value) {
    m_a = value;
    updateNZ(m_a);
}

/**
 * LDX - Load X Register
 * Loads a byte into the X register.
 * Flags: N, Z
 */
void M6502::op_LDX(uint8_t value) {
    m_x = value;
    updateNZ(m_x);
}

/**
 * LDY - Load Y Register
 * Loads a byte into the Y register.
 * Flags: N, Z
 */
void M6502::op_LDY(uint8_t value) {
    m_y = value;
    updateNZ(m_y);
}

/**
 * STA - Store Accumulator
 * Stores the accumulator to memory.
 */
void M6502::op_STA(uint16_t addr) {
    write(addr, m_a);
}

/**
 * STX - Store X Register
 * Stores the X register to memory.
 */
void M6502::op_STX(uint16_t addr) {
    write(addr, m_x);
}

/**
 * STY - Store Y Register
 * Stores the Y register to memory.
 */
void M6502::op_STY(uint16_t addr) {
    write(addr, m_y);
}

// ========================================
// Transfer Instructions
// ========================================

/**
 * TAX - Transfer Accumulator to X
 * Copies the accumulator to the X register.
 * Flags: N, Z
 */
void M6502::op_TAX() {
    read(m_pc);  // Dummy read for cycle accuracy
    m_x = m_a;
    updateNZ(m_x);
}

/**
 * TAY - Transfer Accumulator to Y
 * Copies the accumulator to the Y register.
 * Flags: N, Z
 */
void M6502::op_TAY() {
    read(m_pc);  // Dummy read for cycle accuracy
    m_y = m_a;
    updateNZ(m_y);
}

/**
 * TXA - Transfer X to Accumulator
 * Copies the X register to the accumulator.
 * Flags: N, Z
 */
void M6502::op_TXA() {
    read(m_pc);  // Dummy read for cycle accuracy
    m_a = m_x;
    updateNZ(m_a);
}

/**
 * TYA - Transfer Y to Accumulator
 * Copies the Y register to the accumulator.
 * Flags: N, Z
 */
void M6502::op_TYA() {
    read(m_pc);  // Dummy read for cycle accuracy
    m_a = m_y;
    updateNZ(m_a);
}

/**
 * TSX - Transfer Stack Pointer to X
 * Copies the stack pointer to the X register.
 * Flags: N, Z
 */
void M6502::op_TSX() {
    read(m_pc);  // Dummy read for cycle accuracy
    m_x = m_s;
    updateNZ(m_x);
}

/**
 * TXS - Transfer X to Stack Pointer
 * Copies the X register to the stack pointer.
 * No flags affected.
 */
void M6502::op_TXS() {
    read(m_pc);  // Dummy read for cycle accuracy
    m_s = m_x;
}

// ========================================
// Stack Instructions
// ========================================

/**
 * PHA - Push Accumulator
 * Pushes the accumulator onto the stack.
 */
void M6502::op_PHA() {
    read(m_pc);  // Dummy read for cycle accuracy
    push(m_a);
}

/**
 * PHP - Push Processor Status
 * Pushes the processor status (with B and U flags set) onto the stack.
 */
void M6502::op_PHP() {
    read(m_pc);  // Dummy read for cycle accuracy
    push(m_p | static_cast<uint8_t>(StatusFlag::B) | static_cast<uint8_t>(StatusFlag::U));
}

/**
 * PLA - Pull Accumulator
 * Pulls a byte from the stack into the accumulator.
 * Flags: N, Z
 */
void M6502::op_PLA() {
    read(m_pc);  // Dummy read for cycle accuracy
    read(0x0100 | m_s);  // Dummy read of current stack position
    m_a = pop();
    updateNZ(m_a);
}

/**
 * PLP - Pull Processor Status
 * Pulls a byte from the stack into the processor status.
 * The U flag is always set, B flag is ignored.
 */
void M6502::op_PLP() {
    read(m_pc);  // Dummy read for cycle accuracy
    read(0x0100 | m_s);  // Dummy read of current stack position
    
    bool oldI = getFlag(StatusFlag::I);
    m_p = pop();
    m_p |= static_cast<uint8_t>(StatusFlag::U);  // U flag always set
    
    // Check if we're enabling interrupts (clearing I flag)
    if (oldI && !getFlag(StatusFlag::I) && 
        m_irqState != static_cast<uint8_t>(LineState::CLEAR)) {
        m_afterCLI = true;
    }
}

// ========================================
// Logical Instructions
// ========================================

/**
 * AND - Logical AND
 * Performs bitwise AND between accumulator and memory.
 * Flags: N, Z
 */
void M6502::op_AND(uint8_t value) {
    m_a &= value;
    updateNZ(m_a);
}

/**
 * ORA - Logical OR
 * Performs bitwise OR between accumulator and memory.
 * Flags: N, Z
 */
void M6502::op_ORA(uint8_t value) {
    m_a |= value;
    updateNZ(m_a);
}

/**
 * EOR - Exclusive OR
 * Performs bitwise XOR between accumulator and memory.
 * Flags: N, Z
 */
void M6502::op_EOR(uint8_t value) {
    m_a ^= value;
    updateNZ(m_a);
}

/**
 * BIT - Bit Test
 * Tests bits in memory with the accumulator.
 * Flags: N (bit 7 of memory), V (bit 6 of memory), Z (A & memory == 0)
 */
void M6502::op_BIT(uint8_t value) {
    setFlag(StatusFlag::N, (value & 0x80) != 0);
    setFlag(StatusFlag::V, (value & 0x40) != 0);
    setFlag(StatusFlag::Z, (m_a & value) == 0);
}

// ========================================
// Arithmetic Instructions
// ========================================

/**
 * ADC - Add with Carry
 * Adds memory to accumulator with carry.
 * In decimal mode (if not 2A03), uses BCD arithmetic.
 * Flags: N, V, Z, C
 */
void M6502::op_ADC(uint8_t value) {
    if (getFlag(StatusFlag::D) && m_variant != Variant::NMOS_2A03) {
        // Decimal mode (BCD)
        int carry = getFlag(StatusFlag::C) ? 1 : 0;
        int lo = (m_a & 0x0F) + (value & 0x0F) + carry;
        int hi = (m_a & 0xF0) + (value & 0xF0);
        
        if (lo > 0x09) {
            lo += 0x06;
            hi += 0x10;
        }
        
        // V flag: signed overflow
        bool overflow = (~(m_a ^ value) & (m_a ^ hi) & 0x80) != 0;
        setFlag(StatusFlag::V, overflow);
        
        if (hi > 0x90) {
            hi += 0x60;
        }
        
        setFlag(StatusFlag::C, hi > 0xFF);
        m_a = (lo & 0x0F) | (hi & 0xF0);
        updateNZ(m_a);
    } else {
        // Binary mode
        int carry = getFlag(StatusFlag::C) ? 1 : 0;
        int result = m_a + value + carry;
        
        // V flag: signed overflow (both operands same sign, result different sign)
        bool overflow = (~(m_a ^ value) & (m_a ^ result) & 0x80) != 0;
        setFlag(StatusFlag::V, overflow);
        
        setFlag(StatusFlag::C, result > 0xFF);
        m_a = result & 0xFF;
        updateNZ(m_a);
    }
}

/**
 * SBC - Subtract with Carry
 * Subtracts memory from accumulator with borrow (carry).
 * In decimal mode (if not 2A03), uses BCD arithmetic.
 * Flags: N, V, Z, C
 */
void M6502::op_SBC(uint8_t value) {
    if (getFlag(StatusFlag::D) && m_variant != Variant::NMOS_2A03) {
        // Decimal mode (BCD)
        int borrow = getFlag(StatusFlag::C) ? 0 : 1;
        int result = m_a - value - borrow;
        int lo = (m_a & 0x0F) - (value & 0x0F) - borrow;
        int hi = (m_a & 0xF0) - (value & 0xF0);
        
        if (lo < 0) {
            lo -= 0x06;
            hi -= 0x10;
        }
        
        // V flag: signed overflow
        bool overflow = ((m_a ^ value) & (m_a ^ result) & 0x80) != 0;
        setFlag(StatusFlag::V, overflow);
        
        if (hi < 0) {
            hi -= 0x60;
        }
        
        setFlag(StatusFlag::C, result >= 0);
        m_a = (lo & 0x0F) | (hi & 0xF0);
        updateNZ(m_a);
    } else {
        // Binary mode (equivalent to ADC with inverted operand)
        op_ADC(~value);
    }
}

/**
 * CMP - Compare Accumulator
 * Compares accumulator with memory.
 * Flags: N, Z, C
 */
void M6502::op_CMP(uint8_t value) {
    int result = m_a - value;
    setFlag(StatusFlag::C, m_a >= value);
    updateNZ(result & 0xFF);
}

/**
 * CPX - Compare X Register
 * Compares X register with memory.
 * Flags: N, Z, C
 */
void M6502::op_CPX(uint8_t value) {
    int result = m_x - value;
    setFlag(StatusFlag::C, m_x >= value);
    updateNZ(result & 0xFF);
}

/**
 * CPY - Compare Y Register
 * Compares Y register with memory.
 * Flags: N, Z, C
 */
void M6502::op_CPY(uint8_t value) {
    int result = m_y - value;
    setFlag(StatusFlag::C, m_y >= value);
    updateNZ(result & 0xFF);
}

// ========================================
// Increment/Decrement Instructions
// ========================================

/**
 * INC - Increment Memory
 * Increments a memory location by 1.
 * Flags: N, Z
 */
void M6502::op_INC(uint16_t addr) {
    uint8_t value = read(addr);
    write(addr, value);  // Dummy write for cycle accuracy
    value++;
    write(addr, value);
    updateNZ(value);
}

/**
 * INX - Increment X Register
 * Increments the X register by 1.
 * Flags: N, Z
 */
void M6502::op_INX() {
    read(m_pc);  // Dummy read for cycle accuracy
    m_x++;
    updateNZ(m_x);
}

/**
 * INY - Increment Y Register
 * Increments the Y register by 1.
 * Flags: N, Z
 */
void M6502::op_INY() {
    read(m_pc);  // Dummy read for cycle accuracy
    m_y++;
    updateNZ(m_y);
}

/**
 * DEC - Decrement Memory
 * Decrements a memory location by 1.
 * Flags: N, Z
 */
void M6502::op_DEC(uint16_t addr) {
    uint8_t value = read(addr);
    write(addr, value);  // Dummy write for cycle accuracy
    value--;
    write(addr, value);
    updateNZ(value);
}

/**
 * DEX - Decrement X Register
 * Decrements the X register by 1.
 * Flags: N, Z
 */
void M6502::op_DEX() {
    read(m_pc);  // Dummy read for cycle accuracy
    m_x--;
    updateNZ(m_x);
}

/**
 * DEY - Decrement Y Register
 * Decrements the Y register by 1.
 * Flags: N, Z
 */
void M6502::op_DEY() {
    read(m_pc);  // Dummy read for cycle accuracy
    m_y--;
    updateNZ(m_y);
}

// ========================================
// Shift/Rotate Instructions
// ========================================

/**
 * ASL - Arithmetic Shift Left
 * Shifts all bits left one position. 0 is shifted into bit 0,
 * and bit 7 is shifted into the carry flag.
 * Flags: N, Z, C
 */
uint8_t M6502::op_ASL(uint8_t value) {
    setFlag(StatusFlag::C, (value & 0x80) != 0);
    value <<= 1;
    updateNZ(value);
    return value;
}

/**
 * LSR - Logical Shift Right
 * Shifts all bits right one position. 0 is shifted into bit 7,
 * and bit 0 is shifted into the carry flag.
 * Flags: N (always 0), Z, C
 */
uint8_t M6502::op_LSR(uint8_t value) {
    setFlag(StatusFlag::C, (value & 0x01) != 0);
    value >>= 1;
    updateNZ(value);
    return value;
}

/**
 * ROL - Rotate Left
 * Rotates all bits left one position. The carry is shifted into bit 0,
 * and bit 7 is shifted into the carry.
 * Flags: N, Z, C
 */
uint8_t M6502::op_ROL(uint8_t value) {
    bool oldCarry = getFlag(StatusFlag::C);
    setFlag(StatusFlag::C, (value & 0x80) != 0);
    value = (value << 1) | (oldCarry ? 1 : 0);
    updateNZ(value);
    return value;
}

/**
 * ROR - Rotate Right
 * Rotates all bits right one position. The carry is shifted into bit 7,
 * and bit 0 is shifted into the carry.
 * Flags: N, Z, C
 */
uint8_t M6502::op_ROR(uint8_t value) {
    bool oldCarry = getFlag(StatusFlag::C);
    setFlag(StatusFlag::C, (value & 0x01) != 0);
    value = (value >> 1) | (oldCarry ? 0x80 : 0);
    updateNZ(value);
    return value;
}

// ========================================
// Jump/Branch Instructions
// ========================================

/**
 * JMP - Jump
 * Sets the program counter to the target address.
 */
void M6502::op_JMP(uint16_t addr) {
    m_pc = addr;
}

/**
 * JSR - Jump to Subroutine
 * Pushes the return address (PC - 1) onto the stack and jumps to target.
 */
void M6502::op_JSR(uint16_t addr) {
    read(0x0100 | m_s);  // Dummy read for cycle accuracy
    push16(m_pc - 1);    // Push return address minus 1
    m_pc = addr;
}

/**
 * RTS - Return from Subroutine
 * Pulls the return address from the stack and increments it.
 */
void M6502::op_RTS() {
    read(m_pc);  // Dummy read for cycle accuracy
    read(0x0100 | m_s);  // Dummy read of current stack position
    m_pc = pop16();
    m_pc++;  // Increment return address
    read(m_pc);  // Dummy read for cycle accuracy
}

/**
 * RTI - Return from Interrupt
 * Pulls the processor status and return address from the stack.
 */
void M6502::op_RTI() {
    read(m_pc);  // Dummy read for cycle accuracy
    read(0x0100 | m_s);  // Dummy read of current stack position
    
    bool oldI = getFlag(StatusFlag::I);
    m_p = pop();
    m_p |= static_cast<uint8_t>(StatusFlag::U);  // U flag always set
    m_pc = pop16();
    
    // Check if we're enabling interrupts
    if (oldI && !getFlag(StatusFlag::I) && 
        m_irqState != static_cast<uint8_t>(LineState::CLEAR)) {
        m_afterCLI = true;
    }
}

/**
 * BRK - Break
 * Software interrupt. Pushes PC+2 and status onto stack,
 * sets interrupt disable flag, and jumps to IRQ vector.
 */
void M6502::op_BRK() {
    readPC();  // Read and discard padding byte
    push16(m_pc);
    push(m_p | static_cast<uint8_t>(StatusFlag::B) | static_cast<uint8_t>(StatusFlag::U));
    setFlag(StatusFlag::I, true);
    
    uint8_t lo = read(0xFFFE);
    uint8_t hi = read(0xFFFF);
    m_pc = (hi << 8) | lo;
}

/**
 * Branch - Generic branch operation
 * If condition is true, adds signed offset to PC.
 * Extra cycle if branch is taken, another if page boundary is crossed.
 */
void M6502::op_Branch(bool condition) {
    int8_t offset = addr_REL();
    
    if (condition) {
        uint16_t newPC = m_pc + offset;
        
        // Dummy read for branch taken
        read(m_pc);
        
        // Extra cycle if page boundary crossed
        if ((m_pc & 0xFF00) != (newPC & 0xFF00)) {
            read((m_pc & 0xFF00) | (newPC & 0xFF));
        }
        
        m_pc = newPC;
    }
}

// ========================================
// Flag Instructions
// ========================================

void M6502::op_CLC() { 
    read(m_pc);  // Dummy read
    setFlag(StatusFlag::C, false); 
}

void M6502::op_CLD() { 
    read(m_pc);  // Dummy read
    setFlag(StatusFlag::D, false); 
}

void M6502::op_CLI() { 
    read(m_pc);  // Dummy read
    if (m_irqState != static_cast<uint8_t>(LineState::CLEAR) && getFlag(StatusFlag::I)) {
        m_afterCLI = true;
    }
    setFlag(StatusFlag::I, false); 
}

void M6502::op_CLV() { 
    read(m_pc);  // Dummy read
    setFlag(StatusFlag::V, false); 
}

void M6502::op_SEC() { 
    read(m_pc);  // Dummy read
    setFlag(StatusFlag::C, true); 
}

void M6502::op_SED() { 
    read(m_pc);  // Dummy read
    setFlag(StatusFlag::D, true); 
}

void M6502::op_SEI() { 
    read(m_pc);  // Dummy read
    setFlag(StatusFlag::I, true); 
}

// ========================================
// Misc Instructions
// ========================================

void M6502::op_NOP() {
    read(m_pc);  // Dummy read for cycle accuracy
}

void M6502::op_KIL() {
    // Illegal opcode that halts the CPU
    // Implemented as infinite loop (stop execution)
    m_shouldStop = true;
    m_pc--;  // Stay on this instruction
}

// ========================================
// Illegal/Undocumented Instructions
// ========================================

/**
 * LAX - Load A and X
 * Loads memory into both A and X registers.
 * Flags: N, Z
 */
void M6502::op_LAX(uint8_t value) {
    m_a = value;
    m_x = value;
    updateNZ(m_a);
}

/**
 * SAX - Store A AND X
 * Stores the bitwise AND of A and X into memory.
 */
void M6502::op_SAX(uint16_t addr) {
    write(addr, m_a & m_x);
}

/**
 * DCP - Decrement and Compare
 * Decrements memory and compares with accumulator.
 * Flags: N, Z, C
 */
void M6502::op_DCP(uint16_t addr) {
    uint8_t value = read(addr);
    write(addr, value);  // Dummy write for cycle accuracy
    value--;
    write(addr, value);
    op_CMP(value);
}

/**
 * ISB - Increment and Subtract with Carry
 * Increments memory and subtracts from accumulator with borrow.
 * Flags: N, V, Z, C
 */
void M6502::op_ISB(uint16_t addr) {
    uint8_t value = read(addr);
    write(addr, value);  // Dummy write for cycle accuracy
    value++;
    write(addr, value);
    op_SBC(value);
}

/**
 * SLO - Shift Left and OR
 * Shifts memory left and ORs result with accumulator.
 * Flags: N, Z, C
 */
void M6502::op_SLO(uint16_t addr) {
    uint8_t value = read(addr);
    write(addr, value);  // Dummy write for cycle accuracy
    value = op_ASL(value);
    write(addr, value);
    op_ORA(value);
}

/**
 * RLA - Rotate Left and AND
 * Rotates memory left and ANDs result with accumulator.
 * Flags: N, Z, C
 */
void M6502::op_RLA(uint16_t addr) {
    uint8_t value = read(addr);
    write(addr, value);  // Dummy write for cycle accuracy
    value = op_ROL(value);
    write(addr, value);
    op_AND(value);
}

/**
 * SRE - Shift Right and EOR
 * Shifts memory right and XORs result with accumulator.
 * Flags: N, Z, C
 */
void M6502::op_SRE(uint16_t addr) {
    uint8_t value = read(addr);
    write(addr, value);  // Dummy write for cycle accuracy
    value = op_LSR(value);
    write(addr, value);
    op_EOR(value);
}

/**
 * RRA - Rotate Right and Add with Carry
 * Rotates memory right and adds result to accumulator with carry.
 * Flags: N, V, Z, C
 */
void M6502::op_RRA(uint16_t addr) {
    uint8_t value = read(addr);
    write(addr, value);  // Dummy write for cycle accuracy
    value = op_ROR(value);
    write(addr, value);
    op_ADC(value);
}

/**
 * ANC - AND with Carry
 * ANDs memory with accumulator, then sets carry to bit 7.
 * Flags: N, Z, C
 */
void M6502::op_ANC(uint8_t value) {
    m_a &= value;
    updateNZ(m_a);
    setFlag(StatusFlag::C, (m_a & 0x80) != 0);
}

/**
 * ASR/ALR - AND + LSR
 * ANDs memory with accumulator, then shifts right.
 * Flags: N (always 0), Z, C
 */
void M6502::op_ASR(uint8_t value) {
    m_a &= value;
    setFlag(StatusFlag::C, (m_a & 0x01) != 0);
    m_a >>= 1;
    updateNZ(m_a);
}

/**
 * ARR - AND + ROR
 * ANDs memory with accumulator, then rotates right.
 * Has special decimal mode handling.
 * Flags: N, V, Z, C
 */
void M6502::op_ARR(uint8_t value) {
    m_a &= value;
    
    if (getFlag(StatusFlag::D) && m_variant != Variant::NMOS_2A03) {
        // Decimal mode - complex BCD logic
        bool oldCarry = getFlag(StatusFlag::C);
        m_a = (m_a >> 1) | (oldCarry ? 0x80 : 0);
        updateNZ(m_a);
        
        // Complex V and C flag behavior in decimal mode
        setFlag(StatusFlag::V, ((m_a ^ (m_a >> 1)) & 0x20) != 0);
        
        int lo = m_a & 0x0F;
        int hi = m_a & 0xF0;
        
        if (lo + (lo & 0x01) > 0x05) {
            m_a = (m_a & 0xF0) | ((m_a + 0x06) & 0x0F);
        }
        
        if (hi + (hi & 0x10) > 0x50) {
            m_a = (m_a + 0x60) & 0xFF;
            setFlag(StatusFlag::C, true);
        } else {
            setFlag(StatusFlag::C, false);
        }
    } else {
        // Binary mode
        bool oldCarry = getFlag(StatusFlag::C);
        setFlag(StatusFlag::C, (m_a & 0x01) != 0);
        m_a = (m_a >> 1) | (oldCarry ? 0x80 : 0);
        updateNZ(m_a);
        
        // V flag set based on bit 6 XOR bit 5
        setFlag(StatusFlag::V, ((m_a ^ (m_a >> 1)) & 0x20) != 0);
        
        // C flag from bit 6
        if (m_a & 0x40) {
            setFlag(StatusFlag::C, true);
        }
    }
}

/**
 * AXA/XAA - Unstable illegal opcode
 * Transfers X to A, ANDs with immediate and magic constant.
 * Operation: A = (A | 0xEE) & X & operand
 * Note: Highly unstable, behavior varies by chip
 * Flags: N, Z
 */
void M6502::op_AXA(uint8_t value) {
    m_a = (m_a | 0xEE) & m_x & value;
    updateNZ(m_a);
}

/**
 * SAH/AHX - Store A & X & (H+1)
 * Stores (A AND X AND high_byte+1) at memory.
 * Note: Unstable on page boundary crossing
 * Flags: None
 */
void M6502::op_SAH(uint16_t addr, uint8_t high) {
    uint8_t value = m_a & m_x & (high + 1);
    write(addr, value);
}

/**
 * SSH/TAS - Stack Trick
 * Sets S = A & X, then stores S & (H+1) at memory.
 * Flags: None
 */
void M6502::op_SSH(uint16_t addr, uint8_t high) {
    m_s = m_a & m_x;
    uint8_t value = m_s & (high + 1);
    write(addr, value);
}

/**
 * SXH/SHX - Store X & (H+1)
 * Stores (X AND high_byte+1) at memory.
 * Note: Unstable on page boundary crossing
 * Flags: None
 */
void M6502::op_SXH(uint16_t addr, uint8_t high) {
    uint8_t value = m_x & (high + 1);
    write(addr, value);
}

/**
 * SYH/SHY - Store Y & (H+1)
 * Stores (Y AND high_byte+1) at memory.
 * Note: Unstable on page boundary crossing
 * Flags: None
 */
void M6502::op_SYH(uint16_t addr, uint8_t high) {
    uint8_t value = m_y & (high + 1);
    write(addr, value);
}

/**
 * OAL/LAX/ATX - Load A and X with magic constant
 * Operation: A = X = ((A | 0xEE) & operand)
 * Note: Magic constant 0xEE varies by chip
 * Flags: N, Z
 */
void M6502::op_OAL(uint8_t value) {
    m_a = (m_a | 0xEE) & value;
    m_x = m_a;
    updateNZ(m_a);
}

/**
 * AST/LAS - AND Stack
 * Loads S & memory into A, X, and S.
 * Operation: A = X = S = (S & memory)
 * Flags: N, Z
 */
void M6502::op_AST(uint8_t value) {
    m_s &= value;
    m_a = m_s;
    m_x = m_s;
    updateNZ(m_a);
}

/**
 * ASX/SBX/AXS - Subtract from X
 * Performs (A & X) - operand -> X without borrow.
 * Like CMP + DEX combined.
 * Flags: N, Z, C
 */
void M6502::op_ASX(uint8_t value) {
    uint16_t tmp = (m_a & m_x) - value;
    setFlag(StatusFlag::C, (tmp & 0x100) == 0);  // No borrow
    m_x = tmp & 0xFF;
    updateNZ(m_x);
}

// ========================================
// Instruction Dispatch
// ========================================

/**
 * Execute a single instruction based on opcode.
 * This is the heart of the CPU emulator - it decodes the opcode
 * and executes the appropriate instruction with its addressing mode.
 */
void M6502::executeInstruction(uint8_t opcode) {
    switch (opcode) {
        // ADC
        case 0x69: op_ADC(addr_IMM()); break;
        case 0x65: op_ADC(read(addr_ZP())); break;
        case 0x75: op_ADC(read(addr_ZPX())); break;
        case 0x6D: op_ADC(read(addr_ABS())); break;
        case 0x7D: op_ADC(read(addr_ABX(true))); break;
        case 0x79: op_ADC(read(addr_ABY(true))); break;
        case 0x61: op_ADC(read(addr_IDX())); break;
        case 0x71: op_ADC(read(addr_IDY(true))); break;
        
        // AND
        case 0x29: op_AND(addr_IMM()); break;
        case 0x25: op_AND(read(addr_ZP())); break;
        case 0x35: op_AND(read(addr_ZPX())); break;
        case 0x2D: op_AND(read(addr_ABS())); break;
        case 0x3D: op_AND(read(addr_ABX(true))); break;
        case 0x39: op_AND(read(addr_ABY(true))); break;
        case 0x21: op_AND(read(addr_IDX())); break;
        case 0x31: op_AND(read(addr_IDY(true))); break;
        
        // ASL
        case 0x0A: { read(m_pc); m_a = op_ASL(m_a); break; }
        case 0x06: { uint16_t addr = addr_ZP(); uint8_t val = read(addr); write(addr, val); write(addr, op_ASL(val)); break; }
        case 0x16: { uint16_t addr = addr_ZPX(); uint8_t val = read(addr); write(addr, val); write(addr, op_ASL(val)); break; }
        case 0x0E: { uint16_t addr = addr_ABS(); uint8_t val = read(addr); write(addr, val); write(addr, op_ASL(val)); break; }
        case 0x1E: { uint16_t addr = addr_ABX(false); uint8_t val = read(addr); write(addr, val); write(addr, op_ASL(val)); break; }
        
        // Branch instructions
        case 0x90: op_Branch(!getFlag(StatusFlag::C)); break;  // BCC
        case 0xB0: op_Branch(getFlag(StatusFlag::C)); break;   // BCS
        case 0xF0: op_Branch(getFlag(StatusFlag::Z)); break;   // BEQ
        case 0x30: op_Branch(getFlag(StatusFlag::N)); break;   // BMI
        case 0xD0: op_Branch(!getFlag(StatusFlag::Z)); break;  // BNE
        case 0x10: op_Branch(!getFlag(StatusFlag::N)); break;  // BPL
        case 0x50: op_Branch(!getFlag(StatusFlag::V)); break;  // BVC
        case 0x70: op_Branch(getFlag(StatusFlag::V)); break;   // BVS
        
        // BIT
        case 0x24: op_BIT(read(addr_ZP())); break;
        case 0x2C: op_BIT(read(addr_ABS())); break;
        
        // BRK
        case 0x00: op_BRK(); break;
        
        // CMP
        case 0xC9: op_CMP(addr_IMM()); break;
        case 0xC5: op_CMP(read(addr_ZP())); break;
        case 0xD5: op_CMP(read(addr_ZPX())); break;
        case 0xCD: op_CMP(read(addr_ABS())); break;
        case 0xDD: op_CMP(read(addr_ABX(true))); break;
        case 0xD9: op_CMP(read(addr_ABY(true))); break;
        case 0xC1: op_CMP(read(addr_IDX())); break;
        case 0xD1: op_CMP(read(addr_IDY(true))); break;
        
        // CPX
        case 0xE0: op_CPX(addr_IMM()); break;
        case 0xE4: op_CPX(read(addr_ZP())); break;
        case 0xEC: op_CPX(read(addr_ABS())); break;
        
        // CPY
        case 0xC0: op_CPY(addr_IMM()); break;
        case 0xC4: op_CPY(read(addr_ZP())); break;
        case 0xCC: op_CPY(read(addr_ABS())); break;
        
        // DEC
        case 0xC6: op_DEC(addr_ZP()); break;
        case 0xD6: op_DEC(addr_ZPX()); break;
        case 0xCE: op_DEC(addr_ABS()); break;
        case 0xDE: op_DEC(addr_ABX(false)); break;
        
        // DEX, DEY
        case 0xCA: op_DEX(); break;
        case 0x88: op_DEY(); break;
        
        // EOR
        case 0x49: op_EOR(addr_IMM()); break;
        case 0x45: op_EOR(read(addr_ZP())); break;
        case 0x55: op_EOR(read(addr_ZPX())); break;
        case 0x4D: op_EOR(read(addr_ABS())); break;
        case 0x5D: op_EOR(read(addr_ABX(true))); break;
        case 0x59: op_EOR(read(addr_ABY(true))); break;
        case 0x41: op_EOR(read(addr_IDX())); break;
        case 0x51: op_EOR(read(addr_IDY(true))); break;
        
        // Flag instructions
        case 0x18: op_CLC(); break;
        case 0xD8: op_CLD(); break;
        case 0x58: op_CLI(); break;
        case 0xB8: op_CLV(); break;
        case 0x38: op_SEC(); break;
        case 0xF8: op_SED(); break;
        case 0x78: op_SEI(); break;
        
        // INC
        case 0xE6: op_INC(addr_ZP()); break;
        case 0xF6: op_INC(addr_ZPX()); break;
        case 0xEE: op_INC(addr_ABS()); break;
        case 0xFE: op_INC(addr_ABX(false)); break;
        
        // INX, INY
        case 0xE8: op_INX(); break;
        case 0xC8: op_INY(); break;
        
        // JMP
        case 0x4C: op_JMP(addr_ABS()); break;
        case 0x6C: op_JMP(addr_IND()); break;
        
        // JSR
        case 0x20: op_JSR(addr_ABS()); break;
        
        // LDA
        case 0xA9: op_LDA(addr_IMM()); break;
        case 0xA5: op_LDA(read(addr_ZP())); break;
        case 0xB5: op_LDA(read(addr_ZPX())); break;
        case 0xAD: op_LDA(read(addr_ABS())); break;
        case 0xBD: op_LDA(read(addr_ABX(true))); break;
        case 0xB9: op_LDA(read(addr_ABY(true))); break;
        case 0xA1: op_LDA(read(addr_IDX())); break;
        case 0xB1: op_LDA(read(addr_IDY(true))); break;
        
        // LDX
        case 0xA2: op_LDX(addr_IMM()); break;
        case 0xA6: op_LDX(read(addr_ZP())); break;
        case 0xB6: op_LDX(read(addr_ZPY())); break;
        case 0xAE: op_LDX(read(addr_ABS())); break;
        case 0xBE: op_LDX(read(addr_ABY(true))); break;
        
        // LDY
        case 0xA0: op_LDY(addr_IMM()); break;
        case 0xA4: op_LDY(read(addr_ZP())); break;
        case 0xB4: op_LDY(read(addr_ZPX())); break;
        case 0xAC: op_LDY(read(addr_ABS())); break;
        case 0xBC: op_LDY(read(addr_ABX(true))); break;
        
        // LSR
        case 0x4A: { read(m_pc); m_a = op_LSR(m_a); break; }
        case 0x46: { uint16_t addr = addr_ZP(); uint8_t val = read(addr); write(addr, val); write(addr, op_LSR(val)); break; }
        case 0x56: { uint16_t addr = addr_ZPX(); uint8_t val = read(addr); write(addr, val); write(addr, op_LSR(val)); break; }
        case 0x4E: { uint16_t addr = addr_ABS(); uint8_t val = read(addr); write(addr, val); write(addr, op_LSR(val)); break; }
        case 0x5E: { uint16_t addr = addr_ABX(false); uint8_t val = read(addr); write(addr, val); write(addr, op_LSR(val)); break; }
        
        // NOP
        case 0xEA: op_NOP(); break;
        // Various illegal NOP variants
        case 0x1A: case 0x3A: case 0x5A: case 0x7A:
        case 0xDA: case 0xFA: op_NOP(); break;
        case 0x80: case 0x82: case 0x89: case 0xC2: case 0xE2:
            readPC(); op_NOP(); break;  // NOP immediate
        case 0x04: case 0x44: case 0x64:
            read(addr_ZP()); break;  // NOP zero page
        case 0x14: case 0x34: case 0x54: case 0x74:
        case 0xD4: case 0xF4:
            read(addr_ZPX()); break;  // NOP zero page,X
        case 0x0C:
            read(addr_ABS()); break;  // NOP absolute
        case 0x1C: case 0x3C: case 0x5C: case 0x7C:
        case 0xDC: case 0xFC:
            read(addr_ABX(true)); break;  // NOP absolute,X
        
        // ORA
        case 0x09: op_ORA(addr_IMM()); break;
        case 0x05: op_ORA(read(addr_ZP())); break;
        case 0x15: op_ORA(read(addr_ZPX())); break;
        case 0x0D: op_ORA(read(addr_ABS())); break;
        case 0x1D: op_ORA(read(addr_ABX(true))); break;
        case 0x19: op_ORA(read(addr_ABY(true))); break;
        case 0x01: op_ORA(read(addr_IDX())); break;
        case 0x11: op_ORA(read(addr_IDY(true))); break;
        
        // Stack operations
        case 0x48: op_PHA(); break;
        case 0x08: op_PHP(); break;
        case 0x68: op_PLA(); break;
        case 0x28: op_PLP(); break;
        
        // ROL
        case 0x2A: { read(m_pc); m_a = op_ROL(m_a); break; }
        case 0x26: { uint16_t addr = addr_ZP(); uint8_t val = read(addr); write(addr, val); write(addr, op_ROL(val)); break; }
        case 0x36: { uint16_t addr = addr_ZPX(); uint8_t val = read(addr); write(addr, val); write(addr, op_ROL(val)); break; }
        case 0x2E: { uint16_t addr = addr_ABS(); uint8_t val = read(addr); write(addr, val); write(addr, op_ROL(val)); break; }
        case 0x3E: { uint16_t addr = addr_ABX(false); uint8_t val = read(addr); write(addr, val); write(addr, op_ROL(val)); break; }
        
        // ROR
        case 0x6A: { read(m_pc); m_a = op_ROR(m_a); break; }
        case 0x66: { uint16_t addr = addr_ZP(); uint8_t val = read(addr); write(addr, val); write(addr, op_ROR(val)); break; }
        case 0x76: { uint16_t addr = addr_ZPX(); uint8_t val = read(addr); write(addr, val); write(addr, op_ROR(val)); break; }
        case 0x6E: { uint16_t addr = addr_ABS(); uint8_t val = read(addr); write(addr, val); write(addr, op_ROR(val)); break; }
        case 0x7E: { uint16_t addr = addr_ABX(false); uint8_t val = read(addr); write(addr, val); write(addr, op_ROR(val)); break; }
        
        // RTI, RTS
        case 0x40: op_RTI(); break;
        case 0x60: op_RTS(); break;
        
        // SBC
        case 0xE9: case 0xEB: op_SBC(addr_IMM()); break;  // 0xEB is illegal
        case 0xE5: op_SBC(read(addr_ZP())); break;
        case 0xF5: op_SBC(read(addr_ZPX())); break;
        case 0xED: op_SBC(read(addr_ABS())); break;
        case 0xFD: op_SBC(read(addr_ABX(true))); break;
        case 0xF9: op_SBC(read(addr_ABY(true))); break;
        case 0xE1: op_SBC(read(addr_IDX())); break;
        case 0xF1: op_SBC(read(addr_IDY(true))); break;
        
        // STA
        case 0x85: op_STA(addr_ZP()); break;
        case 0x95: op_STA(addr_ZPX()); break;
        case 0x8D: op_STA(addr_ABS()); break;
        case 0x9D: op_STA(addr_ABX(false)); break;
        case 0x99: op_STA(addr_ABY(false)); break;
        case 0x81: op_STA(addr_IDX()); break;
        case 0x91: op_STA(addr_IDY(false)); break;
        
        // STX
        case 0x86: op_STX(addr_ZP()); break;
        case 0x96: op_STX(addr_ZPY()); break;
        case 0x8E: op_STX(addr_ABS()); break;
        
        // STY
        case 0x84: op_STY(addr_ZP()); break;
        case 0x94: op_STY(addr_ZPX()); break;
        case 0x8C: op_STY(addr_ABS()); break;
        
        // Transfer instructions
        case 0xAA: op_TAX(); break;
        case 0xA8: op_TAY(); break;
        case 0xBA: op_TSX(); break;
        case 0x8A: op_TXA(); break;
        case 0x9A: op_TXS(); break;
        case 0x98: op_TYA(); break;
        
        // Illegal opcodes
        case 0xA7: op_LAX(read(addr_ZP())); break;
        case 0xB7: op_LAX(read(addr_ZPY())); break;
        case 0xAF: op_LAX(read(addr_ABS())); break;
        case 0xBF: op_LAX(read(addr_ABY(true))); break;
        case 0xA3: op_LAX(read(addr_IDX())); break;
        case 0xB3: op_LAX(read(addr_IDY(true))); break;
        
        case 0x87: op_SAX(addr_ZP()); break;
        case 0x97: op_SAX(addr_ZPY()); break;
        case 0x8F: op_SAX(addr_ABS()); break;
        case 0x83: op_SAX(addr_IDX()); break;
        
        case 0xC7: op_DCP(addr_ZP()); break;
        case 0xD7: op_DCP(addr_ZPX()); break;
        case 0xCF: op_DCP(addr_ABS()); break;
        case 0xDF: op_DCP(addr_ABX(false)); break;
        case 0xDB: op_DCP(addr_ABY(false)); break;
        case 0xC3: op_DCP(addr_IDX()); break;
        case 0xD3: op_DCP(addr_IDY(false)); break;
        
        case 0xE7: op_ISB(addr_ZP()); break;
        case 0xF7: op_ISB(addr_ZPX()); break;
        case 0xEF: op_ISB(addr_ABS()); break;
        case 0xFF: op_ISB(addr_ABX(false)); break;
        case 0xFB: op_ISB(addr_ABY(false)); break;
        case 0xE3: op_ISB(addr_IDX()); break;
        case 0xF3: op_ISB(addr_IDY(false)); break;
        
        case 0x07: op_SLO(addr_ZP()); break;
        case 0x17: op_SLO(addr_ZPX()); break;
        case 0x0F: op_SLO(addr_ABS()); break;
        case 0x1F: op_SLO(addr_ABX(false)); break;
        case 0x1B: op_SLO(addr_ABY(false)); break;
        case 0x03: op_SLO(addr_IDX()); break;
        case 0x13: op_SLO(addr_IDY(false)); break;
        
        case 0x27: op_RLA(addr_ZP()); break;
        case 0x37: op_RLA(addr_ZPX()); break;
        case 0x2F: op_RLA(addr_ABS()); break;
        case 0x3F: op_RLA(addr_ABX(false)); break;
        case 0x3B: op_RLA(addr_ABY(false)); break;
        case 0x23: op_RLA(addr_IDX()); break;
        case 0x33: op_RLA(addr_IDY(false)); break;
        
        case 0x47: op_SRE(addr_ZP()); break;
        case 0x57: op_SRE(addr_ZPX()); break;
        case 0x4F: op_SRE(addr_ABS()); break;
        case 0x5F: op_SRE(addr_ABX(false)); break;
        case 0x5B: op_SRE(addr_ABY(false)); break;
        case 0x43: op_SRE(addr_IDX()); break;
        case 0x53: op_SRE(addr_IDY(false)); break;
        
        case 0x67: op_RRA(addr_ZP()); break;
        case 0x77: op_RRA(addr_ZPX()); break;
        case 0x6F: op_RRA(addr_ABS()); break;
        case 0x7F: op_RRA(addr_ABX(false)); break;
        case 0x7B: op_RRA(addr_ABY(false)); break;
        case 0x63: op_RRA(addr_IDX()); break;
        case 0x73: op_RRA(addr_IDY(false)); break;
        
        // Additional illegal opcodes
        case 0x0B: case 0x2B: op_ANC(addr_IMM()); break;  // ANC
        case 0x4B: op_ASR(addr_IMM()); break;  // ASR/ALR
        case 0x6B: op_ARR(addr_IMM()); break;  // ARR
        case 0x8B: op_AXA(addr_IMM()); break;  // AXA/XAA (unstable)
        case 0xAB: op_OAL(addr_IMM()); break;  // OAL/LAX immediate
        case 0xCB: op_ASX(addr_IMM()); break;  // ASX/SBX
        
        // Unstable store opcodes with high byte
        case 0x93: { 
            uint8_t lo = readPC();
            uint8_t hi = readPC();
            read((hi << 8) | ((lo + m_y) & 0xFF));  // Dummy read
            uint16_t addr = ((hi << 8) | lo) + m_y;
            op_SAH(addr, hi); 
            break; 
        }
        case 0x9F: {
            uint8_t lo = readPC();
            uint8_t hi = readPC();
            read((hi << 8) | ((lo + m_y) & 0xFF));  // Dummy read
            uint16_t addr = ((hi << 8) | lo) + m_y;
            op_SAH(addr, hi);
            break;
        }
        case 0x9B: {
            uint8_t lo = readPC();
            uint8_t hi = readPC();
            read((hi << 8) | ((lo + m_y) & 0xFF));  // Dummy read
            uint16_t addr = ((hi << 8) | lo) + m_y;
            op_SSH(addr, hi);
            break;
        }
        case 0x9C: {
            uint8_t lo = readPC();
            uint8_t hi = readPC();
            read((hi << 8) | ((lo + m_x) & 0xFF));  // Dummy read
            uint16_t addr = ((hi << 8) | lo) + m_x;
            op_SYH(addr, hi);
            break;
        }
        case 0x9E: {
            uint8_t lo = readPC();
            uint8_t hi = readPC();
            read((hi << 8) | ((lo + m_y) & 0xFF));  // Dummy read
            uint16_t addr = ((hi << 8) | lo) + m_y;
            op_SXH(addr, hi);
            break;
        }
        case 0xBB: op_AST(read(addr_ABY(true))); break;  // AST/LAS
        
        // KIL instructions (hang the CPU)
        case 0x02: case 0x12: case 0x22: case 0x32:
        case 0x42: case 0x52: case 0x62: case 0x72:
        case 0x92: case 0xB2: case 0xD2: case 0xF2:
            op_KIL(); break;
    }
}

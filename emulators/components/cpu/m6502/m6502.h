#pragma once

#include <cstdint>
#include <cstdlib>
#include <cstring>

/**
 * @brief Modern C++ implementation of the MOS 6502 CPU and its variants
 * 
 * The 6502 is an 8-bit microprocessor with:
 * - 8-bit accumulator (A)
 * - Two 8-bit index registers (X, Y)
 * - 8-bit stack pointer (S) that addresses $0100-$01FF
 * - 8-bit processor status register (P) with 7 flags
 * - 16-bit program counter (PC)
 * 
 * This implementation supports:
 * - Standard 6502 instruction set
 * - 2A03 variant (NES - no decimal mode)
 * - Accurate cycle counting for timing
 * - Illegal/undocumented opcodes
 */
class M6502 {
public:
    // CPU variant types
    enum class Variant : uint8_t {
        MOS_6502,    // Original 6502 with decimal mode
        NMOS_2A03    // NES variant without decimal mode
    };

    // Interrupt line types
    enum class InterruptLine : uint8_t {
        IRQ,         // Maskable interrupt request
        NMI,         // Non-maskable interrupt
        RESET        // Reset line
    };

    // Interrupt line states
    enum class LineState : uint8_t {
        CLEAR,       // Interrupt line is inactive
        ASSERT       // Interrupt line is active
    };

    // Processor status flags (bit positions in P register)
    enum class StatusFlag : uint8_t {
        C = 0x01,    // Carry
        Z = 0x02,    // Zero
        I = 0x04,    // Interrupt disable
        D = 0x08,    // Decimal mode
        B = 0x10,    // Break command
        U = 0x20,    // Unused (always 1)
        V = 0x40,    // Overflow
        N = 0x80     // Negative
    };

    // Memory access callbacks
    using ReadHandler = uint8_t (*)(uint16_t);
    using WriteHandler = void (*)(uint16_t, uint8_t);

    /**
     * @brief Construct a new M6502 CPU with specified variant
     * @param variant CPU variant (6502 or 2A03)
     */
    explicit M6502(Variant variant = Variant::MOS_6502);

    /**
     * @brief Set memory read callback
     * @param callback Function to call when CPU reads from memory
     */
    void setReadHandler(ReadHandler h) { m_read = h; }

    /**
     * @brief Set memory write callback
     * @param callback Function to call when CPU writes to memory
     */
    void setWriteHandler(WriteHandler h) { m_write = h; }

    /**
     * @brief Reset the CPU to initial state
     * 
     * Reads the reset vector from $FFFC-$FFFD and sets PC to that address.
     * Initializes stack pointer to $01FF, sets interrupt disable flag.
     */
    void reset();

    /**
     * @brief Execute CPU for specified number of cycles
     * @param cycles Number of cycles to execute
     * @return Actual number of cycles executed
     * 
     * The CPU will execute instructions until the cycle count is exhausted.
     * Due to multi-cycle instructions, the actual cycles executed may exceed
     * the requested amount slightly.
     */
    int execute(int cycles);

    /**
     * @brief Set interrupt line state
     * @param line Which interrupt line to set
     * @param state New state of the line
     */
    void setInterruptLine(InterruptLine line, LineState state);

    /**
     * @brief Force CPU to stop execution at next opportunity
     * 
     * Useful for synchronization with other system components.
     */
    void stop() { m_shouldStop = true; }

    // State access (for debugging and save states)
    uint16_t getPC() const { return m_pc; }
    void setPC(uint16_t pc) { m_pc = pc; }
    uint16_t getPrevPC() const { return m_prevPC; }
    uint8_t getA() const { return m_a; }
    uint8_t getX() const { return m_x; }
    uint8_t getY() const { return m_y; }
    uint8_t getS() const { return m_s; }
    uint8_t getP() const { return m_p; }
    int getCyclesExecuted() const { return m_cyclesExecuted; }

    // Advanced state management
    struct State {
        uint16_t pc;
        uint16_t prevPC;
        uint8_t a, x, y, s, p;
        uint8_t irqState, nmiState;
        bool pendingIRQ;
        bool pendingNMI;
        bool afterCLI;
        bool holdIRQ;
        bool holdNMI;
        int nmiDelay;
        int cyclesExecuted;
    };

    void getContext(void* dst) const;
    void setContext(const void* src);
    static constexpr size_t contextSize() { return sizeof(State); }

    // Special features for NES emulation
    void setNMIDelay(int cycles) { m_nmiDelay = cycles; }
    void setHoldNMI(bool hold) { m_holdNMI = hold; }
    void setHoldIRQ(bool hold) { m_holdIRQ = hold; }
    void holdIRQ() { m_holdIRQ = true; }
    void holdNMI() { m_holdNMI = true; }
    bool isFetchingOpcode() const { return m_fetchingOpcode; }

private:
    // CPU variant
    Variant m_variant;

    // Registers
    uint16_t m_pc;          // Program counter
    uint16_t m_prevPC;      // Previous PC (for debugging)
    uint8_t m_a;            // Accumulator
    uint8_t m_x;            // X index register
    uint8_t m_y;            // Y index register
    uint8_t m_s;            // Stack pointer (0x0100 + m_s)
    uint8_t m_p;            // Processor status

    // Interrupt state
    uint8_t m_irqState;     // IRQ line state
    uint8_t m_nmiState;     // NMI line state
    bool m_pendingIRQ;      // IRQ is pending
    bool m_pendingNMI;      // NMI is pending
    bool m_afterCLI;        // Last instruction was CLI
    bool m_holdIRQ;         // Hold IRQ acknowledgment (NES specific)
    bool m_holdNMI;         // Hold NMI acknowledgment (NES specific)
    int m_nmiDelay;         // Delay NMI by N cycles (NES specific)

    // Execution state
    int m_cyclesRemaining;  // Cycles left in current execution
    int m_cyclesExecuted;   // Total cycles executed
    bool m_shouldStop;      // Stop execution flag
    bool m_fetchingOpcode;  // Currently fetching opcode

    // Memory callbacks
    ReadHandler m_read = nullptr;
    WriteHandler m_write = nullptr;

    // Flag manipulation helpers
    inline void setFlag(StatusFlag flag, bool value) {
        if (value) {
            m_p |= static_cast<uint8_t>(flag);
        } else {
            m_p &= ~static_cast<uint8_t>(flag);
        }
    }

    inline bool getFlag(StatusFlag flag) const {
        return (m_p & static_cast<uint8_t>(flag)) != 0;
    }

    inline void updateNZ(uint8_t value) {
        setFlag(StatusFlag::Z, value == 0);
        setFlag(StatusFlag::N, (value & 0x80) != 0);
    }

    // Memory access with cycle counting
    inline uint8_t read(uint16_t addr) {
        m_cyclesRemaining--;
        return m_read(addr);
    }

    inline void write(uint16_t addr, uint8_t value) {
        m_cyclesRemaining--;
        m_write(addr, value);
    }

    inline uint8_t readPC() {
        uint8_t value = read(m_pc);
        m_pc++;
        return value;
    }

    inline uint8_t readOpcode() {
        m_fetchingOpcode = true;
        uint8_t opcode = read(m_pc);
        m_pc++;
        m_fetchingOpcode = false;
        return opcode;
    }

    // Stack operations
    inline void push(uint8_t value) {
        write(0x0100 | m_s, value);
        m_s--;
    }

    inline uint8_t pop() {
        m_s++;
        return read(0x0100 | m_s);
    }

    inline void push16(uint16_t value) {
        push(value >> 8);    // High byte first
        push(value & 0xFF);  // Then low byte
    }

    inline uint16_t pop16() {
        uint8_t lo = pop();
        uint8_t hi = pop();
        return (hi << 8) | lo;
    }

    // Interrupt handling
    void serviceNMI();
    void serviceIRQ();

    // ========================================
    // Addressing Mode Helpers
    // ========================================
    
    /**
     * Addressing modes determine how operands are accessed from memory.
     * Each mode returns the effective address or value needed for the operation.
     */

    // Implied - no operand needed
    inline void addr_IMP() { }

    // Accumulator - operate on A register
    inline uint8_t& addr_ACC() { return m_a; }

    // Immediate - operand is the next byte after opcode
    inline uint8_t addr_IMM() {
        return readPC();
    }

    // Zero Page - operand is in page 0 ($00-$FF)
    inline uint16_t addr_ZP() {
        return readPC();
    }

    // Zero Page, X - zero page address + X register (wraps within page 0)
    inline uint16_t addr_ZPX() {
        uint8_t base = readPC();
        read(base);  // Dummy read for cycle accuracy
        return (base + m_x) & 0xFF;
    }

    // Zero Page, Y - zero page address + Y register (wraps within page 0)
    inline uint16_t addr_ZPY() {
        uint8_t base = readPC();
        read(base);  // Dummy read for cycle accuracy
        return (base + m_y) & 0xFF;
    }

    // Absolute - full 16-bit address
    inline uint16_t addr_ABS() {
        uint8_t lo = readPC();
        uint8_t hi = readPC();
        return (hi << 8) | lo;
    }

    // Absolute, X - absolute address + X register
    // pageCross: if true, adds cycle when page boundary is crossed
    inline uint16_t addr_ABX(bool pageCross = false) {
        uint8_t lo = readPC();
        uint8_t hi = readPC();
        uint16_t base = (hi << 8) | lo;
        uint16_t addr = base + m_x;
        
        if (pageCross && (base & 0xFF00) != (addr & 0xFF00)) {
            // Page boundary crossed - dummy read for cycle accuracy
            read((hi << 8) | ((lo + m_x) & 0xFF));
        } else if (!pageCross) {
            // Always do dummy read for write/modify operations
            read((hi << 8) | ((lo + m_x) & 0xFF));
        }
        
        return addr;
    }

    // Absolute, Y - absolute address + Y register
    // pageCross: if true, adds cycle when page boundary is crossed
    inline uint16_t addr_ABY(bool pageCross = false) {
        uint8_t lo = readPC();
        uint8_t hi = readPC();
        uint16_t base = (hi << 8) | lo;
        uint16_t addr = base + m_y;
        
        if (pageCross && (base & 0xFF00) != (addr & 0xFF00)) {
            // Page boundary crossed - dummy read for cycle accuracy
            read((hi << 8) | ((lo + m_y) & 0xFF));
        } else if (!pageCross) {
            // Always do dummy read for write/modify operations
            read((hi << 8) | ((lo + m_y) & 0xFF));
        }
        
        return addr;
    }

    // Indexed Indirect, X - (zero page + X) points to address
    inline uint16_t addr_IDX() {
        uint8_t base = readPC();
        read(base);  // Dummy read for cycle accuracy
        uint8_t ptr = (base + m_x) & 0xFF;
        uint8_t lo = read(ptr);
        uint8_t hi = read((ptr + 1) & 0xFF);
        return (hi << 8) | lo;
    }

    // Indirect Indexed, Y - (zero page) + Y points to address
    // pageCross: if true, adds cycle when page boundary is crossed
    inline uint16_t addr_IDY(bool pageCross = false) {
        uint8_t ptr = readPC();
        uint8_t lo = read(ptr);
        uint8_t hi = read((ptr + 1) & 0xFF);
        uint16_t base = (hi << 8) | lo;
        uint16_t addr = base + m_y;
        
        if (pageCross && (base & 0xFF00) != (addr & 0xFF00)) {
            // Page boundary crossed - dummy read for cycle accuracy
            read((hi << 8) | ((lo + m_y) & 0xFF));
        } else if (!pageCross) {
            // Always do dummy read for write/modify operations
            read((hi << 8) | ((lo + m_y) & 0xFF));
        }
        
        return addr;
    }

    // Relative - signed offset for branch instructions
    inline int8_t addr_REL() {
        return static_cast<int8_t>(readPC());
    }

    // Indirect - used only by JMP ($xxxx)
    // Has famous 6502 bug: doesn't cross page boundaries correctly
    inline uint16_t addr_IND() {
        uint8_t ptrLo = readPC();
        uint8_t ptrHi = readPC();
        uint16_t ptr = (ptrHi << 8) | ptrLo;
        
        uint8_t lo = read(ptr);
        // Bug: if ptr is $xxFF, wraps to $xx00 instead of $(xx+1)00
        uint8_t hi = read((ptr & 0xFF00) | ((ptr + 1) & 0xFF));
        
        return (hi << 8) | lo;
    }

    // ========================================
    // Instruction Implementations
    // ========================================
    
    /**
     * Each instruction is implemented as a separate method.
     * Instructions modify CPU state, flags, and memory as specified
     * by the 6502 architecture.
     */

    // Load/Store Operations
    void op_LDA(uint8_t value);   // Load accumulator
    void op_LDX(uint8_t value);   // Load X register
    void op_LDY(uint8_t value);   // Load Y register
    void op_STA(uint16_t addr);   // Store accumulator
    void op_STX(uint16_t addr);   // Store X register
    void op_STY(uint16_t addr);   // Store Y register

    // Transfer Operations
    void op_TAX();  // Transfer A to X
    void op_TAY();  // Transfer A to Y
    void op_TXA();  // Transfer X to A
    void op_TYA();  // Transfer Y to A
    void op_TSX();  // Transfer S to X
    void op_TXS();  // Transfer X to S

    // Stack Operations
    void op_PHA();  // Push A
    void op_PHP();  // Push P
    void op_PLA();  // Pull A
    void op_PLP();  // Pull P

    // Logic Operations
    void op_AND(uint8_t value);  // Logical AND
    void op_ORA(uint8_t value);  // Logical OR
    void op_EOR(uint8_t value);  // Logical XOR
    void op_BIT(uint8_t value);  // Bit test

    // Arithmetic Operations
    void op_ADC(uint8_t value);  // Add with carry
    void op_SBC(uint8_t value);  // Subtract with carry
    void op_CMP(uint8_t value);  // Compare with A
    void op_CPX(uint8_t value);  // Compare with X
    void op_CPY(uint8_t value);  // Compare with Y

    // Increment/Decrement
    void op_INC(uint16_t addr);  // Increment memory
    void op_INX();               // Increment X
    void op_INY();               // Increment Y
    void op_DEC(uint16_t addr);  // Decrement memory
    void op_DEX();               // Decrement X
    void op_DEY();               // Decrement Y

    // Shift/Rotate Operations
    uint8_t op_ASL(uint8_t value);  // Arithmetic shift left
    uint8_t op_LSR(uint8_t value);  // Logical shift right
    uint8_t op_ROL(uint8_t value);  // Rotate left
    uint8_t op_ROR(uint8_t value);  // Rotate right

    // Jump/Branch Operations
    void op_JMP(uint16_t addr);          // Jump
    void op_JSR(uint16_t addr);          // Jump to subroutine
    void op_RTS();                       // Return from subroutine
    void op_RTI();                       // Return from interrupt
    void op_BRK();                       // Break (software interrupt)
    void op_Branch(bool condition);      // Generic branch

    // Flag Operations
    void op_CLC();  // Clear carry
    void op_CLD();  // Clear decimal
    void op_CLI();  // Clear interrupt disable
    void op_CLV();  // Clear overflow
    void op_SEC();  // Set carry
    void op_SED();  // Set decimal
    void op_SEI();  // Set interrupt disable

    // Misc
    void op_NOP();  // No operation
    void op_KIL();  // Illegal opcode (hangs CPU)

    // Illegal/Undocumented opcodes (for compatibility)
    void op_LAX(uint8_t value);  // LDA + LDX
    void op_SAX(uint16_t addr);  // STA & STX
    void op_DCP(uint16_t addr);  // DEC + CMP
    void op_ISB(uint16_t addr);  // INC + SBC
    void op_SLO(uint16_t addr);  // ASL + ORA
    void op_RLA(uint16_t addr);  // ROL + AND
    void op_SRE(uint16_t addr);  // LSR + EOR
    void op_RRA(uint16_t addr);  // ROR + ADC
    void op_ANC(uint8_t value);  // AND + Set Carry from bit 7
    void op_ASR(uint8_t value);  // AND + LSR
    void op_ARR(uint8_t value);  // AND + ROR
    void op_AXA(uint8_t value);  // Unstable: (A | 0xEE) & X & operand
    void op_SAH(uint16_t addr, uint8_t high);  // Store A & X & (H+1)
    void op_SSH(uint16_t addr, uint8_t high);  // S = A & X; Store S & (H+1)
    void op_SXH(uint16_t addr, uint8_t high);  // Store X & (H+1)
    void op_SYH(uint16_t addr, uint8_t high);  // Store Y & (H+1)
    void op_OAL(uint8_t value);  // (A | 0xEE) & operand -> A, X
    void op_AST(uint8_t value);  // (S & operand) -> A, X, S
    void op_ASX(uint8_t value);  // (A & X) - operand -> X

    // Instruction dispatch
    void executeInstruction(uint8_t opcode);
};

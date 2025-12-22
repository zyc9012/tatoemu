#pragma once

#include "../types.h"
#include <fstream>
#include <functional>

namespace nes {

class Memory;

// CPU status flags
enum CPUFlag : u8 {
    FLAG_C = 0x01,  // Carry
    FLAG_Z = 0x02,  // Zero
    FLAG_I = 0x04,  // Interrupt Disable
    FLAG_D = 0x08,  // Decimal (not used on NES, but implemented)
    FLAG_B = 0x10,  // Break (not a real flag, only exists on stack)
    FLAG_U = 0x20,  // Unused (always 1)
    FLAG_V = 0x40,  // Overflow
    FLAG_N = 0x80   // Negative
};

// Addressing modes
enum class AddressMode {
    Implied,
    Accumulator,
    Immediate,
    ZeroPage,
    ZeroPageX,
    ZeroPageY,
    Absolute,
    AbsoluteX,
    AbsoluteY,
    Indirect,
    IndirectX,
    IndirectY,
    Relative
};

// CPU registers
struct CPURegisters {
    u8 a;           // Accumulator
    u8 x;           // X index
    u8 y;           // Y index
    u8 s;           // Stack pointer
    u8 p;           // Status register
    u16 pc;         // Program counter
};

class CPU {
public:
    CPU();
    ~CPU() = default;

    void setMemory(Memory* memory) { m_memory = memory; }
    void reset();
    void step();
    
    // Interrupt handling
    void nmi();
    void irq();
    
    // For cycle-accurate emulation
    u32 getCycles() const { return m_cycles; }
    void resetCycles() { m_cycles = 0; }
    bool isStalled() const { return m_stallCycles > 0; }
    void stall(u32 cycles) { m_stallCycles += cycles; }
    
    // DMA handling
    void triggerOAMDMA(u8 page);
    
    // State access
    const CPURegisters& getRegisters() const { return m_regs; }
    CPURegisters& getRegisters() { return m_regs; }
    
    // Save/Load state
    void saveState(std::ofstream& file) const;
    void loadState(std::ifstream& file);

private:
    // Memory access (goes through Memory bus)
    u8 read(u16 address);
    void write(u16 address, u8 value);
    u16 read16(u16 address);
    u16 read16Bug(u16 address); // Emulates page boundary bug
    
    // Stack operations
    void push(u8 value);
    void push16(u16 value);
    u8 pull();
    u16 pull16();
    
    // Flag operations
    void setFlag(u8 flag, bool value);
    bool getFlag(u8 flag) const;
    void setZN(u8 value);
    
    // Addressing mode helpers - returns effective address
    u16 getAddress(AddressMode mode, bool& pageCrossed);
    
    // Execute instruction
    void executeInstruction();
    
    // Instruction implementations organized by type
    // Load/Store
    void LDA(AddressMode mode);
    void LDX(AddressMode mode);
    void LDY(AddressMode mode);
    void STA(AddressMode mode);
    void STX(AddressMode mode);
    void STY(AddressMode mode);
    
    // Transfer
    void TAX();
    void TAY();
    void TXA();
    void TYA();
    void TSX();
    void TXS();
    
    // Stack
    void PHA();
    void PHP();
    void PLA();
    void PLP();
    
    // Arithmetic
    void ADC(AddressMode mode);
    void SBC(AddressMode mode);
    void CMP(AddressMode mode);
    void CPX(AddressMode mode);
    void CPY(AddressMode mode);
    
    // Increment/Decrement
    void INC(AddressMode mode);
    void INX();
    void INY();
    void DEC(AddressMode mode);
    void DEX();
    void DEY();
    
    // Logical
    void AND(AddressMode mode);
    void ORA(AddressMode mode);
    void EOR(AddressMode mode);
    void BIT(AddressMode mode);
    
    // Shift
    void ASL(AddressMode mode);
    void LSR(AddressMode mode);
    void ROL(AddressMode mode);
    void ROR(AddressMode mode);
    
    // Jump/Call
    void JMP(AddressMode mode);
    void JSR();
    void RTS();
    void RTI();
    
    // Branch
    void branch(bool condition);
    void BCC();
    void BCS();
    void BEQ();
    void BMI();
    void BNE();
    void BPL();
    void BVC();
    void BVS();
    
    // Flag operations
    void CLC();
    void CLD();
    void CLI();
    void CLV();
    void SEC();
    void SED();
    void SEI();
    
    // System
    void BRK();
    void NOP();
    
    // Unofficial opcodes (commonly used by games)
    void LAX(AddressMode mode);  // LDA + LDX
    void SAX(AddressMode mode);  // Store A & X
    void DCP(AddressMode mode);  // DEC + CMP
    void ISB(AddressMode mode);  // INC + SBC (also known as ISC)
    void SLO(AddressMode mode);  // ASL + ORA
    void RLA(AddressMode mode);  // ROL + AND
    void SRE(AddressMode mode);  // LSR + EOR
    void RRA(AddressMode mode);  // ROR + ADC
    void ANC(AddressMode mode);  // AND + set C from bit 7
    void ALR(AddressMode mode);  // AND + LSR
    void ARR(AddressMode mode);  // AND + ROR (with weird flag behavior)
    void AXS(AddressMode mode);  // (A & X) - immediate, store in X
    void SHY(AddressMode mode);  // Store Y & (high byte of addr + 1)
    void SHX(AddressMode mode);  // Store X & (high byte of addr + 1)
    
    Memory* m_memory;
    CPURegisters m_regs;
    
    u32 m_cycles;        // Cycles executed
    u32 m_stallCycles;   // Cycles to stall (for DMA)
    
    // Interrupt flags
    bool m_nmiPending;
    bool m_irqPending;
    u8 m_nmiDelay;
};

} // namespace nes

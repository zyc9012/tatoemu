#pragma once

#include "../types.h"
#include <fstream>

namespace cps {

class MemoryBase;

// Motorola 68000 CPU emulator (shared between CPS1 and CPS2)
// 
// This is a basic implementation of the 68000 CPU that covers the most
// commonly used instructions. It should be sufficient to run many CPS1
// games, but may need enhancements for full accuracy:
// 
// - Not all addressing modes are fully implemented
// - Cycle counts are approximate
// - Some instructions are stubs (marked with TODO)
// - Exception handling is simplified
// - No bus error or address error exceptions
class CPU {
public:
    CPU();
    ~CPU() = default;

    void reset();
    void step();
    
    u32 getCycles() const { return m_cycles; }
    void setMemory(MemoryBase* memory) { m_memory = memory; }
    
    // Interrupt handling
    void irq(u8 level);  // IRQ with priority level (1-7)
    void resetInterrupt();
    
    // Save/Load state
    void saveState(std::ofstream& file);
    void loadState(std::ifstream& file);

private:
    MemoryBase* m_memory;
    u32 m_cycles;
    
    // 68000 registers (8 data registers, 8 address registers)
    u32 m_dataRegs[8];      // D0-D7
    u32 m_addrRegs[8];      // A0-A7 (A7 is stack pointer)
    u32 m_pc;               // Program counter
    u16 m_sr;               // Status register (includes CCR + system byte)
    u32 m_usp;              // User stack pointer
    u32 m_ssp;              // Supervisor stack pointer
    
    // Status register flags (lower byte is CCR)
    enum StatusFlags : u16 {
        FLAG_C = 0x0001,    // Carry
        FLAG_V = 0x0002,    // Overflow
        FLAG_Z = 0x0004,    // Zero
        FLAG_N = 0x0008,    // Negative
        FLAG_X = 0x0010,    // Extend
        FLAG_I0 = 0x0100,   // Interrupt mask bit 0
        FLAG_I1 = 0x0200,   // Interrupt mask bit 1
        FLAG_I2 = 0x0400,   // Interrupt mask bit 2
        FLAG_S = 0x2000,    // Supervisor mode
        FLAG_T = 0x8000     // Trace mode
    };
    
    // Instruction execution
    void executeInstruction();
    u16 fetchWord();
    u32 fetchLong();
    
    // Flag operations
    void setFlag(StatusFlags flag, bool value);
    bool getFlag(StatusFlags flag) const;
    void setFlags(bool n, bool z, bool v, bool c);
    void updateZeroNegative(u32 value, u8 size);
    
    // Addressing modes
    enum class AddressMode {
        DataRegDirect,
        AddrRegDirect,
        AddrRegIndirect,
        AddrRegPostInc,
        AddrRegPreDec,
        AddrRegDisplacement,
        AddrRegIndex,
        AbsShort,
        AbsLong,
        PCDisplacement,
        PCIndex,
        Immediate
    };
    
    u32 getEffectiveAddress(u8 mode, u8 reg, u8 size);
    u32 readOperand(u8 mode, u8 reg, u8 size);
    void writeOperand(u8 mode, u8 reg, u8 size, u32 value);
    
    // Memory access helpers
    u8 read8(u32 address);
    u16 read16(u32 address);
    u32 read32(u32 address);
    void write8(u32 address, u8 value);
    void write16(u32 address, u16 value);
    void write32(u32 address, u32 value);
    
    // Stack operations
    void push16(u16 value);
    void push32(u32 value);
    u16 pop16();
    u32 pop32();
    
    // Exception handling
    void exception(u8 vector);
    
    // Instruction implementations (major groups)
    void executeMove(u16 opcode);
    void executeMoveq(u16 opcode);
    void executeAdd(u16 opcode);
    void executeSub(u16 opcode);
    void executeAnd(u16 opcode);
    void executeOr(u16 opcode);
    void executeEor(u16 opcode);
    void executeCmp(u16 opcode);
    void executeMul(u16 opcode);
    void executeDiv(u16 opcode);
    void executeShift(u16 opcode);
    void executeRotate(u16 opcode);
    void executeBranch(u16 opcode);
    void executeJump(u16 opcode);
    void executeLea(u16 opcode);
    void executePea(u16 opcode);
    void executeJsr(u16 opcode);
    void executeRts(u16 opcode);
    void executeRte(u16 opcode);
    void executeTrap(u16 opcode);
    void executeNop(u16 opcode);
    
    // Condition code testing
    bool testCondition(u8 condition);
};

} // namespace cps

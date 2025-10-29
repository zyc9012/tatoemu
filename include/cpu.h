#pragma once

#include "types.h"
#include <memory>
#include <fstream>

class MMU;

// CPU Registers
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
struct Registers {
    union {
        struct {
            u8 f; // Flags
            u8 a; // Accumulator
        };
        u16 af;
    };
    
    union {
        struct {
            u8 c;
            u8 b;
        };
        u16 bc;
    };
    
    union {
        struct {
            u8 e;
            u8 d;
        };
        u16 de;
    };
    
    union {
        struct {
            u8 l;
            u8 h;
        };
        u16 hl;
    };
    
    u16 sp; // Stack pointer
    u16 pc; // Program counter
};
#pragma GCC diagnostic pop

// CPU Flags
enum CPUFlags {
    FLAG_Z = 0x80,  // Zero flag
    FLAG_N = 0x40,  // Subtraction flag
    FLAG_H = 0x20,  // Half-carry flag
    FLAG_C = 0x10   // Carry flag
};

// Interrupt flags
enum Interrupts {
    INT_VBLANK = 0x01,
    INT_LCD_STAT = 0x02,
    INT_TIMER = 0x04,
    INT_SERIAL = 0x08,
    INT_JOYPAD = 0x10
};

class CPU {
public:
    CPU();
    ~CPU();

    void setMMU(MMU* mmu);
    void reset();
    void setGBCMode(bool enabled) { m_gbcMode = enabled; }
    u32 step(); // Execute one instruction, return cycles taken
    
    void requestInterrupt(u8 interrupt);
    bool isHalted() const { return m_halted; }
    
    Registers& getRegisters() { return m_regs; }
    
    // Save/Load state
    void saveState(std::ofstream& file) const;
    void loadState(std::ifstream& file);

private:
    u32 executeInstruction(u8 opcode);
    u32 executeCBInstruction(u8 opcode);
    u32 handleInterrupts();
    
    // Helper functions
    u8 read8(u16 address) const;
    void write8(u16 address, u8 value);
    u16 read16(u16 address) const;
    void write16(u16 address, u16 value);
    
    u8 fetch8();
    u16 fetch16();
    
    void push(u16 value);
    u16 pop();
    
    // Flag helpers
    void setFlag(u8 flag, bool value);
    bool getFlag(u8 flag) const;
    
    MMU* m_mmu;
    Registers m_regs;
    bool m_halted;
    bool m_haltBug;  // HALT bug flag
    bool m_ime; // Interrupt Master Enable
    bool m_enableIMENextInstruction;
    u8 m_if;    // Interrupt Flag register
    bool m_gbcMode; // Game Boy Color mode
};


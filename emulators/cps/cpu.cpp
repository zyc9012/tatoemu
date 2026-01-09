#include "cpu.h"
#include "memory_base.h"
#include <iostream>
#include <cstring>
#include <iomanip>
#include <cstdio>

namespace cps {

CPU::CPU()
    : m_memory(nullptr)
    , m_cycles(0)
    , m_pc(0)
    , m_sr(0)
    , m_usp(0)
    , m_ssp(0) {
    std::memset(m_dataRegs, 0, sizeof(m_dataRegs));
    std::memset(m_addrRegs, 0, sizeof(m_addrRegs));
}

void CPU::reset() {
    // Clear all registers
    std::memset(m_dataRegs, 0, sizeof(m_dataRegs));
    std::memset(m_addrRegs, 0, sizeof(m_addrRegs));
    
    // Set supervisor mode with Zero flag set
    // The Zero flag should be set on cold boot to match hardware behavior
    m_sr = FLAG_S | (7 << 8) | FLAG_Z;  // Supervisor mode, interrupt mask = 7, Zero flag set
    m_cycles = 0;
    
    // Read initial SSP and PC from reset vector
    if (m_memory) {
        m_ssp = m_memory->read32(0x000000);
        m_addrRegs[7] = m_ssp;  // A7 is SSP in supervisor mode
        m_pc = m_memory->read32(0x000004);
    }
}

void CPU::step() {
    executeInstruction();
}

u16 CPU::fetchWord() {
    u16 value = m_memory->read16(m_pc);
    m_pc += 2;
    m_cycles += 4;  // Basic read cycle
    return value;
}

u32 CPU::fetchLong() {
    u32 value = m_memory->read32(m_pc);
    m_pc += 4;
    m_cycles += 8;  // Two word reads
    return value;
}

u8 CPU::read8(u32 address) {
    m_cycles += 4;
    return m_memory->read8(address);
}

u16 CPU::read16(u32 address) {
    m_cycles += 4;
    return m_memory->read16(address);
}

u32 CPU::read32(u32 address) {
    m_cycles += 8;
    return m_memory->read32(address);
}

void CPU::write8(u32 address, u8 value) {
    m_cycles += 4;
    m_memory->write8(address, value);
}

void CPU::write16(u32 address, u16 value) {
    m_cycles += 4;
    m_memory->write16(address, value);
}

void CPU::write32(u32 address, u32 value) {
    m_cycles += 8;
    m_memory->write32(address, value);
}

void CPU::setFlag(StatusFlags flag, bool value) {
    if (value) {
        m_sr |= flag;
    } else {
        m_sr &= ~flag;
    }
}

bool CPU::getFlag(StatusFlags flag) const {
    return (m_sr & flag) != 0;
}

void CPU::setFlags(bool n, bool z, bool v, bool c) {
    setFlag(FLAG_N, n);
    setFlag(FLAG_Z, z);
    setFlag(FLAG_V, v);
    setFlag(FLAG_C, c);
}

void CPU::updateZeroNegative(u32 value, u8 size) {
    if (size == 1) {
        setFlag(FLAG_Z, (value & 0xFF) == 0);
        setFlag(FLAG_N, (value & 0x80) != 0);
    } else if (size == 2) {
        setFlag(FLAG_Z, (value & 0xFFFF) == 0);
        setFlag(FLAG_N, (value & 0x8000) != 0);
    } else {
        setFlag(FLAG_Z, value == 0);
        setFlag(FLAG_N, (value & 0x80000000) != 0);
    }
}

void CPU::push16(u16 value) {
    m_addrRegs[7] -= 2;
    write16(m_addrRegs[7], value);
}

void CPU::push32(u32 value) {
    m_addrRegs[7] -= 4;
    write32(m_addrRegs[7], value);
}

u16 CPU::pop16() {
    u16 value = read16(m_addrRegs[7]);
    m_addrRegs[7] += 2;
    return value;
}

u32 CPU::pop32() {
    u32 value = read32(m_addrRegs[7]);
    m_addrRegs[7] += 4;
    return value;
}

bool CPU::testCondition(u8 condition) {
    bool n = getFlag(FLAG_N);
    bool z = getFlag(FLAG_Z);
    bool v = getFlag(FLAG_V);
    bool c = getFlag(FLAG_C);
    
    switch (condition) {
        case 0x0: return true;                          // T - True
        case 0x1: return false;                         // F - False
        case 0x2: return !c && !z;                      // HI - High
        case 0x3: return c || z;                        // LS - Low or Same
        case 0x4: return !c;                            // CC - Carry Clear
        case 0x5: return c;                             // CS - Carry Set
        case 0x6: return !z;                            // NE - Not Equal
        case 0x7: return z;                             // EQ - Equal
        case 0x8: return !v;                            // VC - Overflow Clear
        case 0x9: return v;                             // VS - Overflow Set
        case 0xA: return !n;                            // PL - Plus
        case 0xB: return n;                             // MI - Minus
        case 0xC: return (n && v) || (!n && !v);        // GE - Greater or Equal
        case 0xD: return (n && !v) || (!n && v);        // LT - Less Than
        case 0xE: return (n && v && !z) || (!n && !v && !z);  // GT - Greater Than
        case 0xF: return z || (n && !v) || (!n && v);   // LE - Less or Equal
        default: return false;
    }
}

u32 CPU::getEffectiveAddress(u8 mode, u8 reg, u8 size) {
    switch (mode) {
        case 0: // Data register direct
        case 1: // Address register direct
            return 0;  // No address for register direct
            
        case 2: // Address register indirect
            return m_addrRegs[reg];
            
        case 3: // Address register indirect with postincrement
            {
                u32 addr = m_addrRegs[reg];
                m_addrRegs[reg] += size;
                return addr;
            }
            
        case 4: // Address register indirect with predecrement
            m_addrRegs[reg] -= size;
            return m_addrRegs[reg];
            
        case 5: // Address register indirect with displacement
            {
                s16 displacement = static_cast<s16>(fetchWord());
                return m_addrRegs[reg] + displacement;
            }
            
        case 6: // Address register indirect with index
            {
                u16 extension = fetchWord();
                u8 indexReg = (extension >> 12) & 0x0F;
                bool isAddrReg = (extension & 0x8000) != 0;
                s8 displacement = extension & 0xFF;
                
                u32 indexValue = isAddrReg ? m_addrRegs[indexReg & 7] : m_dataRegs[indexReg & 7];
                if ((extension & 0x0800) == 0) {
                    indexValue = static_cast<s16>(indexValue & 0xFFFF);  // Word index
                }
                
                return m_addrRegs[reg] + indexValue + displacement;
            }
            
        case 7: // Absolute and other modes
            switch (reg) {
                case 0: // Absolute short
                    return static_cast<s16>(fetchWord());
                    
                case 1: // Absolute long
                    return fetchLong();
                    
                case 2: // PC with displacement
                    {
                        u32 pcBase = m_pc;
                        s16 displacement = static_cast<s16>(fetchWord());
                        return pcBase + displacement;
                    }
                    
                case 3: // PC with index
                    {
                        u32 pcBase = m_pc;
                        u16 extension = fetchWord();
                        u8 indexReg = (extension >> 12) & 0x0F;
                        bool isAddrReg = (extension & 0x8000) != 0;
                        s8 displacement = extension & 0xFF;
                        
                        u32 indexValue = isAddrReg ? m_addrRegs[indexReg & 7] : m_dataRegs[indexReg & 7];
                        if ((extension & 0x0800) == 0) {
                            indexValue = static_cast<s16>(indexValue & 0xFFFF);
                        }
                        
                        return pcBase + indexValue + displacement;
                    }
                    
                case 4: // Immediate
                    return m_pc;  // Will fetch directly
                    
                default:
                    return 0;
            }
            
        default:
            return 0;
    }
}

u32 CPU::readOperand(u8 mode, u8 reg, u8 size) {
    if (mode == 0) {
        // Data register direct
        u32 value = m_dataRegs[reg];
        if (size == 1) return value & 0xFF;
        if (size == 2) return value & 0xFFFF;
        return value;
    } else if (mode == 1) {
        // Address register direct
        u32 value = m_addrRegs[reg];
        if (size == 1) return value & 0xFF;
        if (size == 2) return value & 0xFFFF;
        return value;
    } else if (mode == 7 && reg == 4) {
        // Immediate
        if (size == 1) {
            u16 word = fetchWord();
            return word & 0xFF;
        } else if (size == 2) {
            return fetchWord();
        } else {
            return fetchLong();
        }
    } else {
        // Memory addressing
        u32 addr = getEffectiveAddress(mode, reg, size);
        if (size == 1) return read8(addr);
        if (size == 2) return read16(addr);
        return read32(addr);
    }
}

void CPU::writeOperand(u8 mode, u8 reg, u8 size, u32 value) {
    if (mode == 0) {
        // Data register direct
        if (size == 1) {
            m_dataRegs[reg] = (m_dataRegs[reg] & 0xFFFFFF00) | (value & 0xFF);
        } else if (size == 2) {
            m_dataRegs[reg] = (m_dataRegs[reg] & 0xFFFF0000) | (value & 0xFFFF);
        } else {
            m_dataRegs[reg] = value;
        }
    } else if (mode == 1) {
        // Address register direct
        if (size == 1) {
            m_addrRegs[reg] = (m_addrRegs[reg] & 0xFFFFFF00) | (value & 0xFF);
        } else if (size == 2) {
            m_addrRegs[reg] = static_cast<s16>(value & 0xFFFF);  // Sign extend word to long
        } else {
            m_addrRegs[reg] = value;
        }
    } else {
        // Memory addressing
        u32 addr = getEffectiveAddress(mode, reg, size);
        if (size == 1) write8(addr, value & 0xFF);
        else if (size == 2) write16(addr, value & 0xFFFF);
        else write32(addr, value);
    }
}

void CPU::executeInstruction() {
    // Fetch opcode and log state before execution
    u32 pcBefore = m_pc;
    u16 opcode = fetchWord();
    
    // // Print debug state (can be redirected to file with pipe)
    // printf("%08X | %04X | ", pcBefore, opcode);
    // // Data registers D0-D7
    // for (int i = 0; i < 8; i++) {
    //     printf("%08X", m_dataRegs[i]);
    //     if (i < 7) printf(" ");
    // }
    // printf(" | ");
    // // Address registers A0-A7
    // for (int i = 0; i < 8; i++) {
    //     printf("%08X", m_addrRegs[i]);
    //     if (i < 7) printf(" ");
    // }
    // printf(" | %04X\n", m_sr);
    // fflush(stdout);
    
    // Decode by high nibble
    u8 highNibble = (opcode >> 12) & 0x0F;
    
    switch (highNibble) {
        case 0x0: // Bit manipulation, MOVEP, Immediate
            if ((opcode & 0xF100) == 0x0000) {
                // ORI, ANDI, SUBI, ADDI, EORI, CMPI to CCR/SR
                u8 operation = (opcode >> 9) & 0x07;
                
                if ((opcode & 0x00FF) == 0x003C) {
                    // Operation to CCR
                    u16 immediate = fetchWord() & 0xFF;
                    switch (operation) {
                        case 0: m_sr |= immediate; break;  // ORI to CCR
                        case 1: m_sr &= immediate; break;  // ANDI to CCR
                        case 5: m_sr ^= immediate; break;  // EORI to CCR
                    }
                } else if ((opcode & 0x00FF) == 0x007C) {
                    // Operation to SR (privileged)
                    u16 immediate = fetchWord();
                    switch (operation) {
                        case 0: m_sr |= immediate; break;  // ORI to SR
                        case 1: m_sr &= immediate; break;  // ANDI to SR
                        case 5: m_sr ^= immediate; break;  // EORI to SR
                    }
                } else {
                    // Immediate operations on data
                    u8 size = ((opcode >> 6) & 0x03);
                    if (size == 0) size = 1;
                    else if (size == 1) size = 2;
                    else if (size == 2) size = 4;
                    else { executeNop(opcode); break; }
                    
                    u8 mode = (opcode >> 3) & 0x07;
                    u8 reg = opcode & 0x07;
                    
                    u32 immediate;
                    if (size == 1) immediate = fetchWord() & 0xFF;
                    else if (size == 2) immediate = fetchWord();
                    else immediate = fetchLong();
                    
                    u32 value = readOperand(mode, reg, size);
                    u32 result = value;
                    
                    switch (operation) {
                        case 0: result = value | immediate; break;   // ORI
                        case 1: result = value & immediate; break;   // ANDI
                        case 2: result = value - immediate; break;   // SUBI
                        case 3: result = value + immediate; break;   // ADDI
                        case 5: result = value ^ immediate; break;   // EORI
                        case 6: /* CMPI - don't write back */ break;
                        default: executeNop(opcode); return;
                    }
                    
                    if (operation != 6) {
                        writeOperand(mode, reg, size, result);
                    }
                    updateZeroNegative(result, size);
                }
            } else if ((opcode & 0xF1C0) == 0x0100) {
                // BTST, BCHG, BCLR, BSET (dynamic bit number)
                u8 operation = (opcode >> 6) & 0x03;
                u8 bitReg = (opcode >> 9) & 0x07;
                u8 mode = (opcode >> 3) & 0x07;
                u8 reg = opcode & 0x07;
                
                u8 bitNum = m_dataRegs[bitReg] & (mode == 0 ? 31 : 7);
                u32 mask = 1 << bitNum;
                
                u32 value = readOperand(mode, reg, mode == 0 ? 4 : 1);
                bool bitSet = (value & mask) != 0;
                setFlag(FLAG_Z, !bitSet);
                
                switch (operation) {
                    case 1: value ^= mask; writeOperand(mode, reg, mode == 0 ? 4 : 1, value); break; // BCHG
                    case 2: value &= ~mask; writeOperand(mode, reg, mode == 0 ? 4 : 1, value); break; // BCLR
                    case 3: value |= mask; writeOperand(mode, reg, mode == 0 ? 4 : 1, value); break; // BSET
                }
            } else if ((opcode & 0xF1C0) == 0x0800) {
                // BTST, BCHG, BCLR, BSET (static bit number)
                u8 operation = (opcode >> 6) & 0x03;
                u8 mode = (opcode >> 3) & 0x07;
                u8 reg = opcode & 0x07;
                
                u8 bitNum = fetchWord() & (mode == 0 ? 31 : 7);
                u32 mask = 1 << bitNum;
                
                u32 value = readOperand(mode, reg, mode == 0 ? 4 : 1);
                bool bitSet = (value & mask) != 0;
                setFlag(FLAG_Z, !bitSet);
                
                switch (operation) {
                    case 0: break; // BTST - just test
                    case 1: value ^= mask; writeOperand(mode, reg, mode == 0 ? 4 : 1, value); break; // BCHG
                    case 2: value &= ~mask; writeOperand(mode, reg, mode == 0 ? 4 : 1, value); break; // BCLR
                    case 3: value |= mask; writeOperand(mode, reg, mode == 0 ? 4 : 1, value); break; // BSET
                }
            } else if ((opcode & 0xF038) == 0x0008) {
                // MOVEP (move peripheral data)
                executeNop(opcode);  // Rarely used in CPS1
            } else {
                executeNop(opcode);
            }
            break;
            
        case 0x1: // Move byte
        case 0x2: // Move long
        case 0x3: // Move word
            executeMove(opcode);
            break;
            
        case 0x4: // Miscellaneous
            if (opcode == 0x4E71) {
                // NOP - do nothing
                m_cycles += 4;
            } else if ((opcode & 0xFF00) == 0x4E40) {
                // TRAP
                executeTrap(opcode);
            } else if ((opcode & 0xFFF0) == 0x4E70) {
                // RTE, RTR, RTS
                if ((opcode & 0x000F) == 0x0003) {
                    executeRte(opcode);
                } else if ((opcode & 0x000F) == 0x0005) {
                    executeRts(opcode);
                } else {
                    executeNop(opcode);
                }
            } else if ((opcode & 0xFFC0) == 0x4E80) {
                // JSR
                executeJsr(opcode);
            } else if ((opcode & 0xFFC0) == 0x4EC0) {
                // JMP
                executeJump(opcode);
            } else if ((opcode & 0xFFC0) == 0x4840) {
                // PEA
                executePea(opcode);
            } else if ((opcode & 0xF1C0) == 0x41C0) {
                // LEA
                executeLea(opcode);
            } else if ((opcode & 0xFB80) == 0x4880) {
                // MOVEM - Move Multiple Registers
                u8 dir = (opcode >> 10) & 0x01;  // 0=reg to mem, 1=mem to reg
                u8 size = (opcode >> 6) & 0x01;  // 0=word, 1=long
                u8 mode = (opcode >> 3) & 0x07;
                u8 reg = opcode & 0x07;
                
                u16 registerList = fetchWord();
                u8 bytes = size ? 4 : 2;
                
                if (dir == 0) {
                    // Register to memory (MOVEM reg, <ea>)
                    u32 addr = getEffectiveAddress(mode, reg, bytes);
                    
                    // If predecrement mode, process in reverse order
                    if (mode == 4) {
                        for (int i = 15; i >= 0; i--) {
                            if (registerList & (1 << i)) {
                                u32 value = (i < 8) ? m_dataRegs[i] : m_addrRegs[i - 8];
                                if (!size) value &= 0xFFFF;  // Word size
                                
                                addr -= bytes;
                                if (size) write32(addr, value);
                                else write16(addr, value);
                            }
                        }
                        m_addrRegs[reg] = addr;  // Update address register
                    } else {
                        for (int i = 0; i < 16; i++) {
                            if (registerList & (1 << i)) {
                                u32 value = (i < 8) ? m_dataRegs[i] : m_addrRegs[i - 8];
                                if (!size) value &= 0xFFFF;
                                
                                if (size) write32(addr, value);
                                else write16(addr, value);
                                addr += bytes;
                            }
                        }
                    }
                } else {
                    // Memory to register (MOVEM <ea>, reg)
                    u32 addr = getEffectiveAddress(mode, reg, bytes);
                    
                    for (int i = 0; i < 16; i++) {
                        if (registerList & (1 << i)) {
                            u32 value;
                            if (size) {
                                value = read32(addr);
                            } else {
                                value = read16(addr);
                                // Sign extend word to long
                                value = static_cast<s32>(static_cast<s16>(value));
                            }
                            
                            if (i < 8) {
                                m_dataRegs[i] = value;
                            } else {
                                m_addrRegs[i - 8] = value;
                            }
                            addr += bytes;
                        }
                    }
                    
                    // Update address register for postincrement mode
                    if (mode == 3) {
                        m_addrRegs[reg] = addr;
                    }
                }
            } else if ((opcode & 0xFF00) == 0x4200) {
                // CLR
                u8 size = ((opcode >> 6) & 0x03);
                if (size == 0) size = 1;
                else if (size == 1) size = 2;
                else size = 4;
                
                u8 mode = (opcode >> 3) & 0x07;
                u8 reg = opcode & 0x07;
                
                writeOperand(mode, reg, size, 0);
                setFlag(FLAG_N, false);
                setFlag(FLAG_Z, true);
                setFlag(FLAG_V, false);
                setFlag(FLAG_C, false);
            } else if ((opcode & 0xFF00) == 0x4000) {
                // NEGX (negate with extend)
                u8 sizeCode = (opcode >> 6) & 0x03;
                u8 size = (sizeCode == 0) ? 1 : (sizeCode == 1) ? 2 : 4;
                u8 mode = (opcode >> 3) & 0x07;
                u8 reg = opcode & 0x07;
                
                u32 src = readOperand(mode, reg, size);
                u32 extend = getFlag(FLAG_X) ? 1 : 0;
                u32 result = 0 - src - extend;
                
                // Mask result based on size
                u32 mask = (size == 1) ? 0xFF : (size == 2) ? 0xFFFF : 0xFFFFFFFF;
                result &= mask;
                
                writeOperand(mode, reg, size, result);
                
                // Set flags
                bool resultZero = (result & mask) == 0;
                if (resultZero && !getFlag(FLAG_Z)) {
                    setFlag(FLAG_Z, false);  // Z cleared only if result non-zero
                } else if (!resultZero) {
                    setFlag(FLAG_Z, false);
                }
                setFlag(FLAG_N, (result & (mask ^ (mask >> 1))) != 0);
                setFlag(FLAG_V, false);  // Simplified
                setFlag(FLAG_C, src != 0 || extend != 0);
                setFlag(FLAG_X, src != 0 || extend != 0);
            } else if ((opcode & 0xFF00) == 0x4400) {
                // NEG (negate)
                u8 sizeCode = (opcode >> 6) & 0x03;
                u8 size = (sizeCode == 0) ? 1 : (sizeCode == 1) ? 2 : 4;
                u8 mode = (opcode >> 3) & 0x07;
                u8 reg = opcode & 0x07;
                
                u32 src = readOperand(mode, reg, size);
                u32 result = 0 - src;
                
                // Mask result based on size
                u32 mask = (size == 1) ? 0xFF : (size == 2) ? 0xFFFF : 0xFFFFFFFF;
                result &= mask;
                
                writeOperand(mode, reg, size, result);
                
                // Set flags
                setFlag(FLAG_Z, (result & mask) == 0);
                setFlag(FLAG_N, (result & (mask ^ (mask >> 1))) != 0);
                setFlag(FLAG_V, false);  // Simplified
                setFlag(FLAG_C, src != 0);
                setFlag(FLAG_X, src != 0);
            } else if ((opcode & 0xFFC0) == 0x4800) {
                // NBCD - Negate BCD with Extend (stub - treat as NEG for now)
                u8 mode = (opcode >> 3) & 0x07;
                u8 reg = opcode & 0x07;
                
                // For now, just do a simple byte negate (not true BCD)
                u32 src = readOperand(mode, reg, 1);
                u32 extend = getFlag(FLAG_X) ? 1 : 0;
                u32 result = (0 - src - extend) & 0xFF;
                
                writeOperand(mode, reg, 1, result);
                
                // Set flags (simplified)
                setFlag(FLAG_Z, result == 0);
                setFlag(FLAG_N, (result & 0x80) != 0);
                setFlag(FLAG_C, src != 0 || extend != 0);
                setFlag(FLAG_X, src != 0 || extend != 0);
            } else if ((opcode & 0xFF00) == 0x4600) {
                // NOT (complement)
                u8 size = ((opcode >> 6) & 0x03);
                if (size == 0) size = 1;
                else if (size == 1) size = 2;
                else size = 4;
                
                u8 mode = (opcode >> 3) & 0x07;
                u8 reg = opcode & 0x07;
                
                u32 value = readOperand(mode, reg, size);
                u32 result = ~value;
                writeOperand(mode, reg, size, result);
                
                updateZeroNegative(result, size);
                setFlag(FLAG_V, false);
                setFlag(FLAG_C, false);
            } else if ((opcode & 0xFF00) == 0x4800) {
                // NBCD, SWAP, PEA, EXT, MOVEM, TST
                if ((opcode & 0xFFF8) == 0x4840) {
                    // SWAP
                    u8 reg = opcode & 0x07;
                    u32 value = m_dataRegs[reg];
                    m_dataRegs[reg] = ((value & 0xFFFF) << 16) | (value >> 16);
                    updateZeroNegative(m_dataRegs[reg], 4);
                    setFlag(FLAG_V, false);
                    setFlag(FLAG_C, false);
                } else {
                    executeNop(opcode);
                }
            } else if ((opcode & 0xFF00) == 0x4A00) {
                // TST (test)
                u8 size = ((opcode >> 6) & 0x03);
                if (size == 0) size = 1;
                else if (size == 1) size = 2;
                else size = 4;
                
                u8 mode = (opcode >> 3) & 0x07;
                u8 reg = opcode & 0x07;
                
                u32 value = readOperand(mode, reg, size);
                updateZeroNegative(value, size);
                setFlag(FLAG_V, false);
                setFlag(FLAG_C, false);
            } else {
                executeNop(opcode);
            }
            break;
            
        case 0x5: // ADDQ, SUBQ, Scc, DBcc
            if ((opcode & 0xF1C0) == 0x51C0) {
                // DBcc - Decrement and Branch
                // Format: 0101 1ccc c110 1rrr
                u8 condition = (opcode >> 8) & 0x0F;
                u8 reg = opcode & 0x07;
                u32 pcBeforeDisplacement = m_pc; // PC points to displacement word
                s16 displacement = static_cast<s16>(fetchWord());
                
                if (!testCondition(condition)) {
                    // Condition false - decrement and check
                    m_dataRegs[reg] = (m_dataRegs[reg] & 0xFFFF0000) | ((m_dataRegs[reg] - 1) & 0xFFFF);
                    s16 counter = static_cast<s16>(m_dataRegs[reg] & 0xFFFF);
                    
                    if (counter != -1) {
                        // Branch back - displacement is relative to PC of displacement word
                        m_pc = pcBeforeDisplacement + displacement;
                    }
                }
                // If condition is true, just fall through (no decrement, no branch)
            } else if ((opcode & 0xF0C0) == 0x50C0) {
                // Scc - Set according to condition
                u8 condition = (opcode >> 8) & 0x0F;
                u8 mode = (opcode >> 3) & 0x07;
                u8 reg = opcode & 0x07;
                
                u8 value = testCondition(condition) ? 0xFF : 0x00;
                writeOperand(mode, reg, 1, value);
            } else {
                // ADDQ or SUBQ
                executeAdd(opcode);  // Will handle both
            }
            break;
            
        case 0x6: // Bcc, BSR
            executeBranch(opcode);
            break;
            
        case 0x7: // MOVEQ
            executeMoveq(opcode);
            break;
            
        case 0x8: // OR, DIV, SBCD
            if ((opcode & 0x01C0) == 0x01C0) {
                executeDiv(opcode);
            } else {
                executeOr(opcode);
            }
            break;
            
        case 0x9: // SUB, SUBX, SUBA
        case 0xD: // ADD, ADDX, ADDA
            {
                u8 reg = (opcode >> 9) & 0x07;
                u8 opmode = (opcode >> 6) & 0x07;
                
                // Check for ADDA/SUBA (opmode 011 = word, 111 = long to address register)
                if (opmode == 0x03 || opmode == 0x07) {
                    // ADDA or SUBA
                    u8 eaMode = (opcode >> 3) & 0x07;
                    u8 eaReg = opcode & 0x07;
                    bool isLong = (opmode == 0x07);
                    
                    u32 src = readOperand(eaMode, eaReg, isLong ? 4 : 2);
                    if (!isLong) {
                        // Sign-extend word to long
                        src = static_cast<s32>(static_cast<s16>(src));
                    }
                    
                    if (highNibble == 0x9) {
                        m_addrRegs[reg] -= src;  // SUBA
                    } else {
                        m_addrRegs[reg] += src;  // ADDA
                    }
                    // ADDA/SUBA don't affect flags
                } else if (highNibble == 0x9) {
                    executeSub(opcode);
                } else {
                    executeAdd(opcode);
                }
            }
            break;
            
        case 0xA: // Line A - unimplemented/coprocessor
            // Treat as NOP to continue execution
            m_cycles += 4;
            break;
            
        case 0xB: // CMP, EOR, CMPA
            {
                u8 reg = (opcode >> 9) & 0x07;
                u8 opmode = (opcode >> 6) & 0x07;
                
                // Check for CMPA (bit 8 = 1, and opmode indicates word/long to address register)
                if ((opcode & 0x0100) == 0x0100 && (opmode == 0x03 || opmode == 0x07)) {
                    // CMPA - compare <ea> to address register Ar
                    u8 eaMode = (opcode >> 3) & 0x07;
                    u8 eaReg = opcode & 0x07;
                    bool isLong = (opmode == 0x07);
                    
                    u32 src = readOperand(eaMode, eaReg, isLong ? 4 : 2);
                    if (!isLong) {
                        src = static_cast<s32>(static_cast<s16>(src));  // Sign extend
                    }
                    u32 dst = m_addrRegs[reg];
                    u32 result = dst - src;
                    
                    // Set flags for CMPA
                    updateZeroNegative(result, 4);
                    // Carry: set if borrow occurred (dst < src)
                    setFlag(FLAG_C, dst < src);
                    // Overflow: set if signed overflow occurred
                    s32 sDst = static_cast<s32>(dst);
                    s32 sSrc = static_cast<s32>(src);
                    s32 sResult = sDst - sSrc;
                    setFlag(FLAG_V, ((sDst ^ sSrc) & (sDst ^ sResult)) < 0);
                } else if ((opcode & 0x0100) == 0x0100) {
                    // EOR
                    executeEor(opcode);
                } else {
                    // CMP
                    executeCmp(opcode);
                }
            }
            break;
            
        case 0xC: // AND, MUL, ABCD, EXG
            if ((opcode & 0x01C0) == 0x01C0) {
                executeMul(opcode);
            } else {
                executeAnd(opcode);
            }
            break;
            
        case 0xE: // Shift/Rotate
            executeShift(opcode);
            break;
            
        case 0xF: // Line F - Coprocessor or illegal instruction
            // Treat as NOP to keep game running
            m_cycles += 4;
            break;
    }
}

void CPU::executeMove(u16 opcode) {
    u8 size = ((opcode >> 12) & 0x03);
    if (size == 0x01) size = 1;       // Byte
    else if (size == 0x03) size = 2;  // Word
    else if (size == 0x02) size = 4;  // Long
    else size = 2;  // Default to word
    
    u8 dstMode = (opcode >> 6) & 0x07;
    u8 dstReg = (opcode >> 9) & 0x07;
    u8 srcMode = (opcode >> 3) & 0x07;
    u8 srcReg = opcode & 0x07;
    
    u32 value = readOperand(srcMode, srcReg, size);
    writeOperand(dstMode, dstReg, size, value);
    
    updateZeroNegative(value, size);
    setFlag(FLAG_V, false);
    setFlag(FLAG_C, false);
}

void CPU::executeMoveq(u16 opcode) {
    u8 reg = (opcode >> 9) & 0x07;
    s8 immediate = opcode & 0xFF;
    s32 value = immediate;  // Sign extend
    
    m_dataRegs[reg] = value;
    
    updateZeroNegative(value, 4);
    setFlag(FLAG_V, false);
    setFlag(FLAG_C, false);
}

void CPU::executeAdd(u16 opcode) {
    if ((opcode & 0xF100) == 0x5000) {
        // ADDQ
        u8 size = ((opcode >> 6) & 0x03);
        if (size == 0) size = 1;
        else if (size == 1) size = 2;
        else size = 4;
        
        u32 immediate = (opcode >> 9) & 0x07;
        if (immediate == 0) immediate = 8;
        
        u8 mode = (opcode >> 3) & 0x07;
        u8 reg = opcode & 0x07;
        
        u32 dst = readOperand(mode, reg, size);
        u32 result = dst + immediate;
        writeOperand(mode, reg, size, result);
        
        updateZeroNegative(result, size);
    } else {
        // ADD
        u8 reg = (opcode >> 9) & 0x07;
        u8 opmode = (opcode >> 6) & 0x07;
        u8 size = (opmode & 0x03);
        if (size == 0) size = 1;
        else if (size == 1) size = 2;
        else size = 4;
        
        u8 eaMode = (opcode >> 3) & 0x07;
        u8 eaReg = opcode & 0x07;
        
        if (opmode & 0x04) {
            // Dn + <ea> -> <ea>
            u32 src = m_dataRegs[reg];
            u32 dst = readOperand(eaMode, eaReg, size);
            u32 result = src + dst;
            writeOperand(eaMode, eaReg, size, result);
            updateZeroNegative(result, size);
        } else {
            // <ea> + Dn -> Dn
            u32 src = readOperand(eaMode, eaReg, size);
            u32 dst = m_dataRegs[reg];
            u32 result = src + dst;
            writeOperand(0, reg, size, result);
            updateZeroNegative(result, size);
        }
    }
}

void CPU::executeSub(u16 opcode) {
    if ((opcode & 0xF100) == 0x5100) {
        // SUBQ
        u8 size = ((opcode >> 6) & 0x03);
        if (size == 0) size = 1;
        else if (size == 1) size = 2;
        else size = 4;
        
        u32 immediate = (opcode >> 9) & 0x07;
        if (immediate == 0) immediate = 8;
        
        u8 mode = (opcode >> 3) & 0x07;
        u8 reg = opcode & 0x07;
        
        u32 dst = readOperand(mode, reg, size);
        u32 result = dst - immediate;
        writeOperand(mode, reg, size, result);
        
        updateZeroNegative(result, size);
    } else {
        // SUB
        u8 reg = (opcode >> 9) & 0x07;
        u8 opmode = (opcode >> 6) & 0x07;
        u8 size = (opmode & 0x03);
        if (size == 0) size = 1;
        else if (size == 1) size = 2;
        else size = 4;
        
        u8 eaMode = (opcode >> 3) & 0x07;
        u8 eaReg = opcode & 0x07;
        
        if (opmode & 0x04) {
            // <ea> - Dn -> <ea>
            u32 src = readOperand(eaMode, eaReg, size);
            u32 dst = m_dataRegs[reg];
            u32 result = src - dst;
            writeOperand(eaMode, eaReg, size, result);
            updateZeroNegative(result, size);
        } else {
            // Dn - <ea> -> Dn
            u32 src = m_dataRegs[reg];
            u32 dst = readOperand(eaMode, eaReg, size);
            u32 result = src - dst;
            writeOperand(0, reg, size, result);
            updateZeroNegative(result, size);
        }
    }
}

void CPU::executeAnd(u16 opcode) {
    u8 reg = (opcode >> 9) & 0x07;
    u8 opmode = (opcode >> 6) & 0x07;
    u8 size = (opmode & 0x03);
    if (size == 0) size = 1;
    else if (size == 1) size = 2;
    else size = 4;
    
    u8 eaMode = (opcode >> 3) & 0x07;
    u8 eaReg = opcode & 0x07;
    
    u32 src = readOperand(eaMode, eaReg, size);
    u32 dst = m_dataRegs[reg];
    u32 result = src & dst;
    
    if (opmode & 0x04) {
        writeOperand(eaMode, eaReg, size, result);
    } else {
        writeOperand(0, reg, size, result);
    }
    
    updateZeroNegative(result, size);
    setFlag(FLAG_V, false);
    setFlag(FLAG_C, false);
}

void CPU::executeOr(u16 opcode) {
    u8 reg = (opcode >> 9) & 0x07;
    u8 opmode = (opcode >> 6) & 0x07;
    u8 size = (opmode & 0x03);
    if (size == 0) size = 1;
    else if (size == 1) size = 2;
    else size = 4;
    
    u8 eaMode = (opcode >> 3) & 0x07;
    u8 eaReg = opcode & 0x07;
    
    u32 src = readOperand(eaMode, eaReg, size);
    u32 dst = m_dataRegs[reg];
    u32 result = src | dst;
    
    if (opmode & 0x04) {
        writeOperand(eaMode, eaReg, size, result);
    } else {
        writeOperand(0, reg, size, result);
    }
    
    updateZeroNegative(result, size);
    setFlag(FLAG_V, false);
    setFlag(FLAG_C, false);
}

void CPU::executeEor(u16 opcode) {
    u8 reg = (opcode >> 9) & 0x07;
    u8 opmode = (opcode >> 6) & 0x07;
    u8 size = (opmode & 0x03);
    if (size == 0) size = 1;
    else if (size == 1) size = 2;
    else size = 4;
    
    u8 eaMode = (opcode >> 3) & 0x07;
    u8 eaReg = opcode & 0x07;
    
    u32 dst = readOperand(eaMode, eaReg, size);
    u32 src = m_dataRegs[reg];
    u32 result = src ^ dst;
    
    writeOperand(eaMode, eaReg, size, result);
    
    updateZeroNegative(result, size);
    setFlag(FLAG_V, false);
    setFlag(FLAG_C, false);
}

void CPU::executeCmp(u16 opcode) {
    u8 reg = (opcode >> 9) & 0x07;
    u8 opmode = (opcode >> 6) & 0x07;
    u8 size = (opmode & 0x03);
    if (size == 0) size = 1;
    else if (size == 1) size = 2;
    else size = 4;
    
    u8 eaMode = (opcode >> 3) & 0x07;
    u8 eaReg = opcode & 0x07;
    
    u32 src = readOperand(eaMode, eaReg, size);
    u32 dst = m_dataRegs[reg];
    u32 result = dst - src;
    
    // Set flags for CMP
    updateZeroNegative(result, size);
    // Carry: set if borrow occurred (dst < src when treating as unsigned)
    setFlag(FLAG_C, dst < src);
    // Overflow: set if signed overflow occurred
    s32 sDst, sSrc, sResult;
    if (size == 1) {
        sDst = static_cast<s8>(dst & 0xFF);
        sSrc = static_cast<s8>(src & 0xFF);
    } else if (size == 2) {
        sDst = static_cast<s16>(dst & 0xFFFF);
        sSrc = static_cast<s16>(src & 0xFFFF);
    } else {
        sDst = static_cast<s32>(dst);
        sSrc = static_cast<s32>(src);
    }
    sResult = sDst - sSrc;
    setFlag(FLAG_V, ((sDst ^ sSrc) & (sDst ^ sResult)) < 0);
}

void CPU::executeMul(u16 opcode) {
    // MULS/MULU
    u8 reg = (opcode >> 9) & 0x07;
    u8 eaMode = (opcode >> 3) & 0x07;
    u8 eaReg = opcode & 0x07;
    bool isSigned = (opcode & 0x0100) == 0x0100;
    
    u16 src = readOperand(eaMode, eaReg, 2) & 0xFFFF;
    u16 dst = m_dataRegs[reg] & 0xFFFF;
    
    if (isSigned) {
        s32 result = static_cast<s16>(src) * static_cast<s16>(dst);
        m_dataRegs[reg] = result;
        updateZeroNegative(result, 4);
    } else {
        u32 result = src * dst;
        m_dataRegs[reg] = result;
        updateZeroNegative(result, 4);
    }
    
    setFlag(FLAG_V, false);
    setFlag(FLAG_C, false);
    m_cycles += 38;  // MUL takes ~38-70 cycles
}

void CPU::executeDiv(u16 opcode) {
    // DIVS/DIVU
    u8 reg = (opcode >> 9) & 0x07;
    u8 eaMode = (opcode >> 3) & 0x07;
    u8 eaReg = opcode & 0x07;
    bool isSigned = (opcode & 0x0100) == 0x0100;
    
    u16 divisor = readOperand(eaMode, eaReg, 2) & 0xFFFF;
    if (divisor == 0) {
        // Division by zero
        exception(5);  // Trap #5
        return;
    }
    
    u32 dividend = m_dataRegs[reg];
    
    if (isSigned) {
        s32 quotient = static_cast<s32>(dividend) / static_cast<s16>(divisor);
        s16 remainder = static_cast<s32>(dividend) % static_cast<s16>(divisor);
        m_dataRegs[reg] = (remainder << 16) | (quotient & 0xFFFF);
    } else {
        u32 quotient = dividend / divisor;
        u16 remainder = dividend % divisor;
        m_dataRegs[reg] = (remainder << 16) | (quotient & 0xFFFF);
    }
    
    m_cycles += 100;  // DIV takes ~100-140 cycles
}

void CPU::executeShift(u16 opcode) {
    // ASL, ASR, LSL, LSR, ROL, ROR, ROXL, ROXR
    u8 direction = (opcode >> 8) & 0x01;  // 0=right, 1=left
    u8 size = ((opcode >> 6) & 0x03);
    if (size == 0) size = 1;
    else if (size == 1) size = 2;
    else if (size == 2) size = 4;
    else { executeNop(opcode); return; }
    
    u8 type = (opcode >> 3) & 0x03;  // 0=AS, 1=LS, 2=ROX, 3=RO
    u8 mode = (opcode >> 5) & 0x01;  // 0=immediate count, 1=register count
    u8 reg = opcode & 0x07;
    
    // Get shift count
    u8 count;
    if (mode == 0) {
        count = (opcode >> 9) & 0x07;
        if (count == 0) count = 8;
    } else {
        count = m_dataRegs[(opcode >> 9) & 0x07] & 63;
    }
    
    u32 value = m_dataRegs[reg];
    u32 result = value;
    bool carry = false;
    
    // Mask to size
    u32 mask = (size == 1) ? 0xFF : (size == 2) ? 0xFFFF : 0xFFFFFFFF;
    value &= mask;
    
    if (count > 0) {
        switch (type) {
            case 0: // Arithmetic Shift
                if (direction) {
                    // ASL
                    for (u8 i = 0; i < count; i++) {
                        carry = (value & (1 << (size * 8 - 1))) != 0;
                        value = (value << 1) & mask;
                    }
                } else {
                    // ASR
                    u32 signBit = value & (1 << (size * 8 - 1));
                    for (u8 i = 0; i < count; i++) {
                        carry = (value & 1) != 0;
                        value = (value >> 1) | signBit;
                    }
                }
                break;
                
            case 1: // Logical Shift
                if (direction) {
                    // LSL
                    for (u8 i = 0; i < count; i++) {
                        carry = (value & (1 << (size * 8 - 1))) != 0;
                        value = (value << 1) & mask;
                    }
                } else {
                    // LSR
                    for (u8 i = 0; i < count; i++) {
                        carry = (value & 1) != 0;
                        value >>= 1;
                    }
                }
                break;
                
            case 2: // Rotate with Extend
                // ROXL/ROXR - includes extend bit in rotation
                executeNop(opcode);
                return;
                
            case 3: // Rotate
                if (direction) {
                    // ROL
                    for (u8 i = 0; i < count; i++) {
                        carry = (value & (1 << (size * 8 - 1))) != 0;
                        value = ((value << 1) | (carry ? 1 : 0)) & mask;
                    }
                } else {
                    // ROR
                    for (u8 i = 0; i < count; i++) {
                        carry = (value & 1) != 0;
                        value = (value >> 1) | (carry ? (1 << (size * 8 - 1)) : 0);
                    }
                }
                break;
        }
        
        result = value;
    }
    
    // Write back
    if (size == 1) {
        m_dataRegs[reg] = (m_dataRegs[reg] & 0xFFFFFF00) | (result & 0xFF);
    } else if (size == 2) {
        m_dataRegs[reg] = (m_dataRegs[reg] & 0xFFFF0000) | (result & 0xFFFF);
    } else {
        m_dataRegs[reg] = result;
    }
    
    // Update flags
    updateZeroNegative(result, size);
    setFlag(FLAG_C, carry);
    setFlag(FLAG_V, false);
    if (count > 0) {
        setFlag(FLAG_X, carry);
    }
}

void CPU::executeRotate(u16 opcode) {
    executeNop(opcode);
}

void CPU::executeBranch(u16 opcode) {
    u8 condition = (opcode >> 8) & 0x0F;
    s8 displacement8 = opcode & 0xFF;
    
    s32 displacement;
    if (displacement8 == 0) {
        // 16-bit displacement - PC is already at the displacement word location
        // Displacement is relative to the PC of the displacement word
        u32 pcBeforeDisplacement = m_pc;
        displacement = static_cast<s16>(fetchWord());
        // Adjust: displacement is relative to PC before fetching displacement word
        // But we've already incremented PC, so use pcBeforeDisplacement
        if (testCondition(condition)) {
            m_pc = pcBeforeDisplacement + displacement;
            m_cycles += 2;  // Taken branch penalty
        }
    } else {
        // 8-bit displacement - relative to PC after opcode fetch
        displacement = displacement8;
        if (testCondition(condition)) {
            m_pc += displacement;
            m_cycles += 2;  // Taken branch penalty
        }
    }
}

void CPU::executeJump(u16 opcode) {
    u8 mode = (opcode >> 3) & 0x07;
    u8 reg = opcode & 0x07;
    
    u32 address = getEffectiveAddress(mode, reg, 4);
    m_pc = address;
}

void CPU::executeLea(u16 opcode) {
    u8 reg = (opcode >> 9) & 0x07;
    u8 mode = (opcode >> 3) & 0x07;
    u8 eaReg = opcode & 0x07;
    
    u32 address = getEffectiveAddress(mode, eaReg, 4);
    m_addrRegs[reg] = address;
}

void CPU::executePea(u16 opcode) {
    u8 mode = (opcode >> 3) & 0x07;
    u8 reg = opcode & 0x07;
    
    u32 address = getEffectiveAddress(mode, reg, 4);
    push32(address);
}

void CPU::executeJsr(u16 opcode) {
    u8 mode = (opcode >> 3) & 0x07;
    u8 reg = opcode & 0x07;
    
    u32 address = getEffectiveAddress(mode, reg, 4);
    push32(m_pc);
    m_pc = address;
}

void CPU::executeRts(u16 opcode) {
    (void)opcode;
    m_pc = pop32();
}

void CPU::executeRte(u16 opcode) {
    (void)opcode;
    m_sr = pop16();
    m_pc = pop32();
}

void CPU::executeTrap(u16 opcode) {
    u8 vector = opcode & 0x0F;
    exception(32 + vector);
}

void CPU::executeNop(u16 opcode) {
    // Silently treat as NOP to keep game running
    m_cycles += 4;
}

void CPU::exception(u8 vector) {
    // Save SR and PC
    push16(m_sr);
    push32(m_pc);
    
    // Set supervisor mode
    m_sr |= FLAG_S;
    
    // Read exception vector
    u32 vectorAddress = vector * 4;
    m_pc = read32(vectorAddress);
}

void CPU::irq(u8 level) {
    // Check if interrupt level is higher than current mask
    u8 currentMask = (m_sr >> 8) & 0x07;
    if (level > currentMask) {
        exception(24 + level);  // Auto-vectored interrupts
    }
}

void CPU::resetInterrupt() {
    // Clear interrupt bits
    m_sr &= ~(FLAG_I0 | FLAG_I1 | FLAG_I2);
}

void CPU::saveState(std::ofstream& file) {
    file.write(reinterpret_cast<const char*>(m_dataRegs), sizeof(m_dataRegs));
    file.write(reinterpret_cast<const char*>(m_addrRegs), sizeof(m_addrRegs));
    file.write(reinterpret_cast<const char*>(&m_pc), sizeof(m_pc));
    file.write(reinterpret_cast<const char*>(&m_sr), sizeof(m_sr));
    file.write(reinterpret_cast<const char*>(&m_usp), sizeof(m_usp));
    file.write(reinterpret_cast<const char*>(&m_ssp), sizeof(m_ssp));
    file.write(reinterpret_cast<const char*>(&m_cycles), sizeof(m_cycles));
}

void CPU::loadState(std::ifstream& file) {
    file.read(reinterpret_cast<char*>(m_dataRegs), sizeof(m_dataRegs));
    file.read(reinterpret_cast<char*>(m_addrRegs), sizeof(m_addrRegs));
    file.read(reinterpret_cast<char*>(&m_pc), sizeof(m_pc));
    file.read(reinterpret_cast<char*>(&m_sr), sizeof(m_sr));
    file.read(reinterpret_cast<char*>(&m_usp), sizeof(m_usp));
    file.read(reinterpret_cast<char*>(&m_ssp), sizeof(m_ssp));
    file.read(reinterpret_cast<char*>(&m_cycles), sizeof(m_cycles));
}

} // namespace cps

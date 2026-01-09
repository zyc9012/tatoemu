#pragma once

#include "../types.h"
#include <fstream>

namespace cps {

class MemoryBase;
class APUBase;

// Z80 CPU emulator (shared between CPS1 and CPS2)
class SoundCPU {
public:
    SoundCPU();
    ~SoundCPU() = default;

    void reset();
    void step();
    
    u32 getCycles() const { return m_cycles; }
    void setMemory(MemoryBase* memory) { m_memory = memory; }
    void setAPU(APUBase* apu) { m_apu = apu; }
    
    // Interrupt handling
    void irq();
    void nmi();
    
    // Save/Load state
    void saveState(std::ofstream& file);
    void loadState(std::ifstream& file);

private:
    MemoryBase* m_memory;
    APUBase* m_apu;
    u32 m_cycles;
    
    // Z80 registers
    u16 m_af;        // AF register pair
    u16 m_bc;        // BC register pair
    u16 m_de;        // DE register pair
    u16 m_hl;        // HL register pair
    u16 m_af_;       // AF' register pair (alternate)
    u16 m_bc_;       // BC' register pair (alternate)
    u16 m_de_;       // DE' register pair (alternate)
    u16 m_hl_;       // HL' register pair (alternate)
    u16 m_ix;        // IX index register
    u16 m_iy;        // IY index register
    u16 m_sp;        // Stack pointer
    u16 m_pc;        // Program counter
    u8 m_i;          // Interrupt vector register
    u8 m_r;          // Refresh register
    bool m_iff1;     // Interrupt flip-flop 1
    bool m_iff2;     // Interrupt flip-flop 2
    u8 m_im;         // Interrupt mode
    
    // Internal state
    bool m_halted;
};

} // namespace cps

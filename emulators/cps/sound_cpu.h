#pragma once

#include "../types.h"
#include "../components/compact.h"
#include <fstream>

// Forward declarations for Z80 callbacks
extern "C" {
    u8 z80_read_prog(u32 address);
    void z80_write_prog(u32 address, u8 value);
    u8 z80_read_io(u32 port);
    void z80_write_io(u32 port, u8 value);
}

namespace cps {

class Memory;
class APUBase;

// Z80 CPU emulator (shared between CPS1 and CPS2)
class SoundCPU {
public:
    SoundCPU();
    ~SoundCPU();

    void reset();
    void step(u32 cycles);  // Execute specified number of cycles
    
    u32 getCycles() const { return m_cycles; }
    void setMemory(Memory* memory) { m_memory = memory; }
    void setAPU(APUBase* apu) { m_apu = apu; }
    
    // Accessors for Z80 callbacks
    Memory* getMemory() const { return m_memory; }
    APUBase* getAPU() const { return m_apu; }
    
    // Interrupt handling
    void irq(bool state = true);
    void nmi();
    
    // Save/Load state
    void saveState(std::ofstream& file);
    void loadState(std::ifstream& file);

protected:
    // Protected so Z80 callback functions can access them
    Memory* m_memory;
    APUBase* m_apu;
    u32 m_cycles;
};

} // namespace cps

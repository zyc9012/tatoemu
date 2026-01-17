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
    u8 z80_read_op(u32 address);
    u8 z80_read_op_arg(u32 address);
}

namespace cps {

class Memory;
class APU;

// Z80 CPU emulator (shared between CPS1 and CPS2)
class SoundCPU {
public:
    SoundCPU();
    ~SoundCPU();

    void reset();
    u32 step(u32 cycles);  // Execute specified number of cycles, returns actual cycles executed
    
    u32 getCycles() const { return m_cycles; }
    void setMemory(Memory* memory) { m_memory = memory; }
    void setAPU(APU* apu) { m_apu = apu; }
    
    // Accessors for Z80 callbacks
    Memory* getMemory() const { return m_memory; }
    APU* getAPU() const { return m_apu; }
    
    // Interrupt handling
    void irq(bool state = true);
    void nmi();
    
    // Save/Load state
    void saveState(std::ofstream& file);
    void loadState(std::ifstream& file);
    
    // Set CPS version for timer configuration
    void setCPSVersion(u8 version);

protected:
    // Protected so Z80 callback functions can access them
    Memory* m_memory;
    APU* m_apu;
    u32 m_cycles;
    
    // CPS2 timer-based interrupt (252 Hz)
    u8 m_cpsVersion;
    u32 m_timerAccumulator;  // Accumulated cycles since last timer interrupt
    u32 m_timerPeriod;       // Cycles per timer interrupt (for 252 Hz at current frequency)
};

} // namespace cps

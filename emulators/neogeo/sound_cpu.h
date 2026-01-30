#pragma once

#include "../types.h"
#include "../components/compact.h"
#include "../components/buffer.h"

namespace neogeo {

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
    void saveState(Buffer* buf);
    void loadState(Buffer* buf);

protected:
    // Protected so Z80 callback functions can access them
    Memory* m_memory;
    APU* m_apu;
    u32 m_cycles;
};

} // namespace neogeo

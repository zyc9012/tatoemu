#pragma once

#include "../types.h"
#include "../components/compact.h"
#include "../components/buffer.h"

namespace cps {

class Memory;
class APU;
class Cartridge;

// Z80 CPU emulator (shared between CPS1 and CPS2)
class SoundCPU {
public:
    SoundCPU();
    ~SoundCPU();

    void reset();
    u32 step(u32 cycles);  // Execute specified number of cycles, returns actual cycles executed
    
    u32 frameCycles() const { return m_cycles; }
    void endFrame() { m_cycles -= m_cyclesPerFrame; }

    void setMemory(Memory* memory) { m_memory = memory; }
    void setAPU(APU* apu) { m_apu = apu; }
    void setCartridge(Cartridge* cartridge) { m_cartridge = cartridge; }
    
    // Accessors for Z80 callbacks
    Memory* getMemory() const { return m_memory; }
    APU* getAPU() const { return m_apu; }
    
    // Interrupt handling
    void irq(bool state = true);
    void nmi();
    
    // Save/Load state
    void saveState(Buffer* buf);
    void loadState(Buffer* buf);
    
    // Set CPS version for timer configuration
    void setCPSVersion(u8 version);

protected:
    // Protected so Z80 callback functions can access them
    Memory* m_memory;
    APU* m_apu;
    u32 m_cycles;
    u32 m_cyclesPerFrame;
    Cartridge* m_cartridge;
    
    // CPS2 timer-based interrupt (252 Hz)
    u8 m_cpsVersion;
    u32 m_timerAccumulator;  // Accumulated cycles since last timer interrupt
    u32 m_timerPeriod;       // Cycles per timer interrupt (for 252 Hz at current frequency)
};

} // namespace cps

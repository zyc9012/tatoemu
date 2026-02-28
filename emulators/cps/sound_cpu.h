#pragma once

#include "../types.h"
#include "../components/compact.h"
#include "../components/buffer.h"
#include "../components/cpu/z80_new/z80.h"

namespace cps {

class Memory;
class Audio;
class Cartridge;

// Z80 sound CPU wrapper for CPS1/CPS2
class SoundCPU {
public:
    SoundCPU();
    ~SoundCPU();

    void reset();
    u32 step(u32 cycles);  // Execute specified number of cycles, returns actual cycles executed
    
    u32 frameCycles() const { return m_cycles; }
    void endFrame() { m_cycles -= m_cyclesPerFrame; }

    void setMemory(Memory* memory) { m_memory = memory; }
    void setAudio(Audio* audio) { m_audio = audio; }
    void setCartridge(Cartridge* cartridge) { m_cartridge = cartridge; }
    
    // Accessors for Z80 callbacks
    Memory* getMemory() const { return m_memory; }
    Audio* getAudio() const { return m_audio; }
    
    // Interrupt handling
    void irq(bool state = true);
    void nmi();
    
    // Save/Load state
    void saveState(Buffer* buf);
    void loadState(Buffer* buf);
    
    // Set CPS version for timer configuration
    void setCPSVersion(u8 version);

protected:
    Memory* m_memory;
    Audio* m_audio;
    Z80 m_z80;
    u32 m_cycles;
    u32 m_cyclesPerFrame;
    Cartridge* m_cartridge;
    
    // CPS2 timer-based interrupt (252 Hz)
    u8 m_cpsVersion;
    u32 m_timerAccumulator;
    u32 m_timerPeriod;
};

} // namespace cps

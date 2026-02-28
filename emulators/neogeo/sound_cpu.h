#pragma once

#include "consts.h"
#include "../types.h"
#include "../components/compact.h"
#include "../components/buffer.h"
#include "../components/cpu/z80/z80.h"

namespace neogeo {

class Memory;
class Audio;

// Z80 sound CPU wrapper for Neo Geo
class SoundCPU {
public:
    SoundCPU();
    ~SoundCPU();

    void reset();
    u32 step(u32 cycles);  // Execute specified number of cycles, returns actual cycles executed
    
    u32 frameCycles() const { return m_cycles; }
    void endFrame() { m_cycles -= SOUND_CPU_CYCLES_PER_FRAME; }

    void setMemory(Memory* memory) { m_memory = memory; }
    void setAudio(Audio* audio) { m_audio = audio; }
    
    // Accessors for Z80 callbacks
    Memory* getMemory() const { return m_memory; }
    Audio* getAudio() const { return m_audio; }
    
    // Interrupt handling
    void irq(bool state = true);
    void nmi();
    
    // Save/Load state
    void saveState(Buffer* buf);
    void loadState(Buffer* buf);

protected:
    Memory* m_memory;
    Audio* m_audio;
    Z80 m_z80;
    u32 m_cycles;
};

} // namespace neogeo

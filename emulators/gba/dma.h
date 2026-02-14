#pragma once

#include "types.h"
#include "consts.h"
#include "../components/buffer.h"

namespace gba {

class Memory;

struct DMAChannel {
    u32 source;
    u32 dest;
    u16 count;
    u16 control;
    
    u32 internalSource;
    u32 internalDest;
    u32 internalCount;
    bool active;
    bool repeat;
};

class DMA {
public:
    DMA();
    ~DMA();

    void setMemory(Memory* memory) { m_memory = memory; }
    
    void reset();
    void writeRegister(u32 offset, u16 value);
    
    void runImmediate();
    void runHBlank();
    void runVBlank();
    void runFIFO(int fifoIndex);
    
    bool isActive() const;
    
    // Save/Load state
    void saveState(Buffer* buf);
    void loadState(Buffer* buf);

private:
    void trigger(int channel);
    void run(int channel);
    
    Memory* m_memory = nullptr;
    DMAChannel m_channels[4];
};

} // namespace gba

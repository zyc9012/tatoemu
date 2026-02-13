#pragma once

#include "types.h"
#include "../components/buffer.h"

namespace gba {

class Memory;

class CPU {
public:
    CPU();
    ~CPU();

    void setMemory(Memory* memory) { m_memory = memory; }
    
    void reset();
    int step();
    
    void raiseIRQ();
    void checkIRQ();
    
    // Save/Load state
    void saveState(Buffer* buf);
    void loadState(Buffer* buf);

    // Public for ARM7 callbacks
    Memory* m_memory = nullptr;

private:
    int m_cycles = 0;
};

} // namespace gba

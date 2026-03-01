#pragma once

#include "types.h"
#include "../components/buffer.h"
#include "../components/cpu/arm7tdmi/arm7tdmi.h"

namespace gba {

class Memory;

class CPU {
public:
    CPU();
    ~CPU();

    void setMemory(Memory* memory);

    void reset();
    int step(int cycles);

    void checkIRQ();

    // Save/Load state
    void saveState(Buffer* buf);
    void loadState(Buffer* buf);

private:
    ARM7TDMI m_arm7;
    Memory* m_memory = nullptr;
    int m_cycles = 0;
};

} // namespace gba

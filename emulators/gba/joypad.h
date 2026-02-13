#pragma once

#include "types.h"
#include "../components/buffer.h"
#include <SDL3/SDL.h>

namespace gba {

class Memory;

enum class GBAButton {
    A = 0,
    B = 1,
    SELECT = 2,
    START = 3,
    RIGHT = 4,
    LEFT = 5,
    UP = 6,
    DOWN = 7,
    R = 8,
    L = 9
};

class Joypad {
public:
    Joypad();
    ~Joypad();

    void setMemory(Memory* memory) { m_memory = memory; }
    
    bool handleInput(SDL_Event& event);
    void handleButton(GBAButton button, bool pressed);
    
    u16 read() const { return m_state; }
    
    // Save/Load state
    void saveState(Buffer* buf);
    void loadState(Buffer* buf);

private:
    Memory* m_memory = nullptr;
    u16 m_state = 0x3FF; // All buttons released (active low)
};

} // namespace gba

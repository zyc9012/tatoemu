#pragma once

#include "types.h"
#include "../components/buffer.h"
#include <SDL3/SDL.h>

namespace gb {

class CPU;

enum JoypadButton {
    BUTTON_A = 0,
    BUTTON_B = 1,
    BUTTON_SELECT = 2,
    BUTTON_START = 3,
    BUTTON_RIGHT = 4,
    BUTTON_LEFT = 5,
    BUTTON_UP = 6,
    BUTTON_DOWN = 7
};

class Joypad {
public:
    Joypad();
    ~Joypad();

    void setCPU(CPU* cpu);
    
    bool handleInput(SDL_Event& event);
    void handleButton(JoypadButton button, bool pressed);
    
    u8 read() const;
    void write(u8 value);
    
    // Save/Load state
    void saveState(Buffer* buf);
    void loadState(Buffer* buf);

private:
    CPU* m_cpu;
    u8 m_buttonState;    // Current button states (1 = not pressed, 0 = pressed)
    u8 m_selectedButtons; // Which button group is selected
};

} // namespace gb


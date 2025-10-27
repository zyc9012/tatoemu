#pragma once

#include "types.h"

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
    
    void pressButton(JoypadButton button);
    void releaseButton(JoypadButton button);
    
    u8 read() const;
    void write(u8 value);

private:
    CPU* m_cpu;
    u8 m_buttonState;    // Current button states (1 = not pressed, 0 = pressed)
    u8 m_selectedButtons; // Which button group is selected
};


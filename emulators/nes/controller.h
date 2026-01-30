#pragma once

#include "../types.h"
#include "consts.h"
#include "../components/buffer.h"
#include <array>
#include <SDL3/SDL.h>

namespace nes {

class Controller {
public:
    Controller() = default;
    ~Controller() = default;
    
    void reset();
    
    // Button state
    bool handleInput(SDL_Event& event);
    void handleButton(u8 player, ControllerButton button, bool pressed);
    
    // Serial interface
    void write(u8 player, u8 value);
    u8 read(u8 player);
    
    // Direct state access
    u8 getState(u8 player) const { return m_buttons[player]; }
    void setState(u8 player, u8 state) { m_buttons[player] = state; }
    
    // Save/Load state
    void saveState(Buffer* buf);
    void loadState(Buffer* buf);

private:
    std::array<u8, 2> m_buttons;       // Current button state (bit per button)
    std::array<u8, 2> m_shiftRegister; // Shift register for serial output
    std::array<bool, 2> m_strobe;      // Strobe state
};

} // namespace nes

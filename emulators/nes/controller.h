#pragma once

#include "../types.h"
#include "consts.h"
#include <fstream>

namespace nes {

class Controller {
public:
    Controller();
    ~Controller() = default;
    
    void reset();
    
    // Button state
    void pressButton(ControllerButton button);
    void releaseButton(ControllerButton button);
    bool isButtonPressed(ControllerButton button) const;
    
    // Serial interface
    void write(u8 value);
    u8 read();
    
    // Direct state access
    u8 getState() const { return m_buttons; }
    void setState(u8 state) { m_buttons = state; }
    
    // Save/Load state
    void saveState(std::ofstream& file) const;
    void loadState(std::ifstream& file);

private:
    u8 m_buttons;       // Current button state (bit per button)
    u8 m_shiftRegister; // Shift register for serial output
    bool m_strobe;      // Strobe state
};

} // namespace nes

#pragma once

#include "../types.h"
#include "cartridge.h"
#include <fstream>
#include <array>

namespace neogeo {

// NeoGeo controller buttons (4-button layout)
enum ControllerButton : u8 {
    BUTTON_UP = 0,
    BUTTON_DOWN = 1,
    BUTTON_LEFT = 2,
    BUTTON_RIGHT = 3,
    BUTTON_A = 4,
    BUTTON_B = 5,
    BUTTON_C = 6,
    BUTTON_D = 7,
    BUTTON_START = 8,
    BUTTON_SELECT = 9,
    BUTTON_COIN = 10,
    BUTTON_TEST = 11,
    BUTTON_SERVICE = 12,
};

class Controller {
public:
    Controller();
    ~Controller() = default;

    void reset();
    
    // Button input handlers
    void pressButton(u8 player, ControllerButton button);
    void releaseButton(u8 player, ControllerButton button);
    
    // Read input port value (active low, so returns inverted value)
    u8 readInput1(u8 offset) const;
    u8 readInput2(u8 offset) const;
    u8 readInput3(u8 offset) const;
    
    // Save/Load state
    void saveState(std::ofstream& file);
    void loadState(std::ifstream& file);

    // Component connections
    void setCartridge(Cartridge* cartridge) { m_cartridge = cartridge; }

private:
    // Input state: [player][button] -> pressed (true) or not (false)
    // NeoGeo has 2 players, each with 13 possible buttons
    std::array<std::array<bool, 13>, 2> m_buttonState;
    
    // System buttons (not player-specific)
    bool m_testButton;
    bool m_serviceButton;

    // Component connections
    Cartridge* m_cartridge;
};

} // namespace neogeo

#pragma once

#include "../types.h"
#include <fstream>

namespace cps {

class Memory;

// CPS controller buttons (6-button layout - shared between CPS1 and CPS2)
enum ControllerButton : u8 {
    BUTTON_UP = 0,
    BUTTON_DOWN = 1,
    BUTTON_LEFT = 2,
    BUTTON_RIGHT = 3,
    BUTTON_PUNCH1 = 4,
    BUTTON_PUNCH2 = 5,
    BUTTON_PUNCH3 = 6,
    BUTTON_KICK1 = 7,
    BUTTON_KICK2 = 8,
    BUTTON_KICK3 = 9,
    BUTTON_START = 10,
    BUTTON_COIN = 11
};

class Controller {
public:
    Controller();
    ~Controller() = default;

    void reset();
    void pressButton(ControllerButton button);
    void releaseButton(ControllerButton button);
    
    u8 read() const;  // Read direction + punch buttons (ports 0x000/0x001, 0x010/0x011)
    u8 readKicks() const;  // Read kick buttons (port 0x012 for 6-button games)
    u8 readCoinStart() const;  // Read coin/start buttons (port 0x018)
    void setMemory(Memory* memory) { m_memory = memory; }
    
    // Save/Load state
    void saveState(std::ofstream& file);
    void loadState(std::ifstream& file);

private:
    u16 m_buttons;  // Button states (bitmask)
    Memory* m_memory;
};

} // namespace cps

#pragma once

#include "../types.h"
#include "cartridge.h"
#include <fstream>
#include <array>
#include <unordered_map>

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

// Button mapping: maps (player, button) -> (port, bit)
struct ButtonMapping {
    u16 port;
    u8 bit;
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

    // Check system button state (for compatibility)
    bool isTestButtonPressed() const { return (m_portRegisters[0] & 0x01) == 0; }
    bool isServiceButtonPressed() const { return (m_portRegisters[0] & 0x02) == 0; }

    // Save/Load state
    void saveState(std::ofstream& file);
    void loadState(std::ifstream& file);

    // Component connections
    void setCartridge(Cartridge* cartridge) { m_cartridge = cartridge; }

private:
    // Port registers (indexed by offset within each input port)
    // Port 0: Player 1 input (0x300000)
    // Port 1: Player 2 input (0x340000)
    // Port 2: System buttons (0x380000)
    // Port 3: Coin buttons (for system status)
    std::array<u8, 4> m_portRegisters;

    // Button mapping configuration: (player << 8 | button) -> (port, bit)
    std::unordered_map<u16, ButtonMapping> m_buttonMappings;

    // Initialize Neo Geo button mappings
    void initButtonMappings();

    // Helper to set/clear a bit in a port register
    void setPortBit(u8 port, u8 bit, bool pressed);

    // Component connections
    Cartridge* m_cartridge;
};

} // namespace neogeo

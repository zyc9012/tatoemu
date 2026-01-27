#pragma once

#include "../types.h"
#include "cartridge.h"
#include "../components/socd.h"
#include <fstream>
#include <array>
#include <unordered_map>
#include <SDL3/SDL.h>

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

// Button mapping: maps (player, button) -> (bank, bit)
struct ButtonMapping {
    u16 bank;
    u8 bit;
};

class Controller {
public:
    Controller();
    ~Controller() = default;

    void reset();

    // Button input handlers
    bool handleInput(SDL_Event& event);
    void handleButton(u8 player, ControllerButton button, bool pressed);

    // Read input value
    u8 readInput1(u8 offset) const;
    u8 readInput2(u8 offset) const;
    u8 readInput3(u8 offset) const;

    // Get input bank value
    u8 getInputBank(u8 index) const;

    // Save/Load state
    void saveState(std::ofstream& file);
    void loadState(std::ifstream& file);

    // Component connections
    void setCartridge(Cartridge* cartridge) { m_cartridge = cartridge; }

private:
    // Input banks (indexed by offset within each input bank)
    std::array<u8, 6> m_inputBanks;

    // Button mapping configuration: (player << 8 | button) -> (bank, bit)
    std::unordered_map<u16, ButtonMapping> m_buttonMappings;

    // SOCD processor for directional inputs
    ClearOpposite<2> m_socdProcessor;

    // Initialize Neo Geo button mappings
    void initButtonMappings();

    // Helper to set/clear a bit in an input bank
    void setBankBit(u8 bank, u8 bit, bool pressed);

    // Component connections
    Cartridge* m_cartridge;
};

} // namespace neogeo

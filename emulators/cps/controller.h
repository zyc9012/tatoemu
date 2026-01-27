#pragma once

#include "../types.h"
#include "../components/socd.h"
#include <fstream>
#include <array>
#include <unordered_map>
#include <SDL3/SDL.h>

namespace cps {

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
    BUTTON_COIN = 11,
    BUTTON_DIAG = 12,
    BUTTON_SERVICE = 13,
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
    
    // Set CPS version to configure button mappings
    void setCPSVersion(u8 cpsVersion);
    
    // Button input handlers
    bool handleInput(SDL_Event& event);
    void handleButton(u8 player, ControllerButton button, bool pressed);
    
    // Read port register value
    u8 readPort(u16 port) const;
    
    // Save/Load state
    void saveState(std::ofstream& file);
    void loadState(std::ifstream& file);

private:
    // Port registers (indexed by port address)
    std::array<u8, 0x200> m_portRegisters;
    
    // Button mapping configuration: (player << 8 | button) -> (port, bit)
    std::unordered_map<u16, ButtonMapping> m_buttonMappings;
    
    // SOCD processor for directional inputs
    ClearOpposite<2> m_socdProcessor;

    // Initialize button mappings for CPS1
    void initCPS1Mappings();
    
    // Initialize button mappings for CPS2
    void initCPS2Mappings();
    
    // Helper to set/clear a bit in a port register
    void setPortBit(u16 port, u8 bit, bool pressed);
};

} // namespace cps

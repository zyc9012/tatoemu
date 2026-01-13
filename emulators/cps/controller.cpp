#include "controller.h"
#include "memory_base.h"

namespace cps {

Controller::Controller()
    : m_buttons(0)
    , m_memory(nullptr) {
}

void Controller::reset() {
    m_buttons = 0;
}

void Controller::pressButton(ControllerButton button) {
    m_buttons |= (1 << button);
}

void Controller::releaseButton(ControllerButton button) {
    m_buttons &= ~(1 << button);
}

u8 Controller::read() const {
    // Return button states as a byte
    // CPS1 bit mapping for ports 0x000/0x001 and 0x010/0x011 (active low, hardware inverts):
    // Bit 0: Right
    // Bit 1: Left
    // Bit 2: Down
    // Bit 3: Up
    // Bit 4: Fire 1 (Punch 1)
    // Bit 5: Fire 2 (Punch 2)
    // Bit 6: Fire 3 (Punch 3)
    // Bit 7: unused
    
    u8 result = 0;
    
    // Remap from our enum to CPS bit positions
    if (m_buttons & (1 << BUTTON_RIGHT))  result |= (1 << 0);
    if (m_buttons & (1 << BUTTON_LEFT))   result |= (1 << 1);
    if (m_buttons & (1 << BUTTON_DOWN))   result |= (1 << 2);
    if (m_buttons & (1 << BUTTON_UP))     result |= (1 << 3);
    if (m_buttons & (1 << BUTTON_PUNCH1))  result |= (1 << 4);
    if (m_buttons & (1 << BUTTON_PUNCH2))  result |= (1 << 5);
    if (m_buttons & (1 << BUTTON_PUNCH3))  result |= (1 << 6);
    
    return result;
}

u8 Controller::readKicks() const {
    // Return kick button states for port 0x012 (6-button games like Street Fighter II)
    // Bit 0: Kick 1
    // Bit 1: Kick 2
    // Bit 2: Kick 3
    // Bit 4: P2 Kick 1
    // Bit 5: P2 Kick 2
    // Bit 6: P2 Kick 3
    
    u8 result = 0;
    if (m_buttons & (1 << BUTTON_KICK1))  result |= (1 << 0);
    if (m_buttons & (1 << BUTTON_KICK2))  result |= (1 << 1);
    if (m_buttons & (1 << BUTTON_KICK3))  result |= (1 << 2);
    return result;
}

u8 Controller::readCoinStart() const {
    // Return coin/start button states for port 0x018
    // Bit 0: Coin
    // Bit 1: (P2 Coin - handled separately)
    // Bit 2: Service
    // Bit 4: Start
    // Bit 5: (P2 Start - handled separately)
    // Bit 6: Diagnostic
    
    u8 result = 0;
    if (m_buttons & (1 << BUTTON_COIN))  result |= (1 << 0);
    if (m_buttons & (1 << BUTTON_START)) result |= (1 << 4);
    return result;
}

void Controller::saveState(std::ofstream& file) {
    (void)file;
    // Don't save button states
}

void Controller::loadState(std::ifstream& file) {
    (void)file;
}

} // namespace cps

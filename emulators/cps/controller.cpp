#include "controller.h"
#include "../components/socd.h"
#include <cstring>

namespace cps {

Controller::Controller() {
    reset();
}

void Controller::reset() {
    // Initialize all port registers
    m_portRegisters.fill(0x00);
    m_socdProcessor.reset();
}

void Controller::setCPSVersion(u8 cpsVersion) {
    m_buttonMappings.clear();
    
    if (cpsVersion == 2) {
        initCPS2Mappings();
    } else {
        initCPS1Mappings();
    }
}

void Controller::initCPS1Mappings() {
    // CPS1 Input Mapping (based on Street Fighter II)
    // Port 0x000: P2 directions + punches
    // Port 0x001: P1 directions + punches
    // Port 0x177: P1 kicks (bits 0-2) and P2 kicks (bits 4-5)
    // Port 0x018: Coins/Starts
    
    // Player 1 directions + punches -> port 0x001
    m_buttonMappings[(1 << 8) | BUTTON_RIGHT] = {0x001, 0};
    m_buttonMappings[(1 << 8) | BUTTON_LEFT] = {0x001, 1};
    m_buttonMappings[(1 << 8) | BUTTON_DOWN] = {0x001, 2};
    m_buttonMappings[(1 << 8) | BUTTON_UP] = {0x001, 3};
    m_buttonMappings[(1 << 8) | BUTTON_PUNCH1] = {0x001, 4};
    m_buttonMappings[(1 << 8) | BUTTON_PUNCH2] = {0x001, 5};
    m_buttonMappings[(1 << 8) | BUTTON_PUNCH3] = {0x001, 6};
    
    // Player 1 kicks -> port 0x177 (bits 0-2)
    m_buttonMappings[(1 << 8) | BUTTON_KICK1] = {0x177, 0};
    m_buttonMappings[(1 << 8) | BUTTON_KICK2] = {0x177, 1};
    m_buttonMappings[(1 << 8) | BUTTON_KICK3] = {0x177, 2};
    
    // Player 2 directions + punches -> port 0x000
    m_buttonMappings[(2 << 8) | BUTTON_RIGHT] = {0x000, 0};
    m_buttonMappings[(2 << 8) | BUTTON_LEFT] = {0x000, 1};
    m_buttonMappings[(2 << 8) | BUTTON_DOWN] = {0x000, 2};
    m_buttonMappings[(2 << 8) | BUTTON_UP] = {0x000, 3};
    m_buttonMappings[(2 << 8) | BUTTON_PUNCH1] = {0x000, 4};
    m_buttonMappings[(2 << 8) | BUTTON_PUNCH2] = {0x000, 5};
    m_buttonMappings[(2 << 8) | BUTTON_PUNCH3] = {0x000, 6};
    
    // Player 2 kicks -> port 0x177 (bits 4-6)
    m_buttonMappings[(2 << 8) | BUTTON_KICK1] = {0x177, 4};
    m_buttonMappings[(2 << 8) | BUTTON_KICK2] = {0x177, 5};
    m_buttonMappings[(2 << 8) | BUTTON_KICK3] = {0x177, 6};
    
    // Coins/Starts -> port 0x018
    m_buttonMappings[(1 << 8) | BUTTON_COIN] = {0x018, 0};
    m_buttonMappings[(2 << 8) | BUTTON_COIN] = {0x018, 1};
    m_buttonMappings[(1 << 8) | BUTTON_START] = {0x018, 4};
    m_buttonMappings[(2 << 8) | BUTTON_START] = {0x018, 5};

    // Diagnostic/Service -> port 0x018
    m_buttonMappings[(0 << 8) | BUTTON_DIAG] = {0x018, 6};
    m_buttonMappings[(0 << 8) | BUTTON_SERVICE] = {0x018, 2};
}

void Controller::initCPS2Mappings() {
    // CPS2 Input Mapping (based on fighting games)
    // Port 0x000: P2 directions + punches
    // Port 0x001: P1 directions + punches
    // Port 0x011: P1 kicks (bits 0-2) and P2 kicks (bits 4-5)
    // Port 0x020: P1 Start (bit 0), P2 Start (bit 1), P1 Coin (bit 4), P2 Coin (bit 5), P2 Strong Kick (bit 6)
    // Port 0x021: EEPROM (bit 0), Diagnostic (bit 1), Service (bit 2)
    
    // Player 1 directions + punches -> port 0x001
    m_buttonMappings[(1 << 8) | BUTTON_RIGHT] = {0x001, 0};
    m_buttonMappings[(1 << 8) | BUTTON_LEFT] = {0x001, 1};
    m_buttonMappings[(1 << 8) | BUTTON_DOWN] = {0x001, 2};
    m_buttonMappings[(1 << 8) | BUTTON_UP] = {0x001, 3};
    m_buttonMappings[(1 << 8) | BUTTON_PUNCH1] = {0x001, 4};
    m_buttonMappings[(1 << 8) | BUTTON_PUNCH2] = {0x001, 5};
    m_buttonMappings[(1 << 8) | BUTTON_PUNCH3] = {0x001, 6};
    
    // Player 1 kicks -> port 0x011 (bits 0-2)
    m_buttonMappings[(1 << 8) | BUTTON_KICK1] = {0x011, 0};
    m_buttonMappings[(1 << 8) | BUTTON_KICK2] = {0x011, 1};
    m_buttonMappings[(1 << 8) | BUTTON_KICK3] = {0x011, 2};
    
    // Player 2 directions + punches -> port 0x000
    m_buttonMappings[(2 << 8) | BUTTON_RIGHT] = {0x000, 0};
    m_buttonMappings[(2 << 8) | BUTTON_LEFT] = {0x000, 1};
    m_buttonMappings[(2 << 8) | BUTTON_DOWN] = {0x000, 2};
    m_buttonMappings[(2 << 8) | BUTTON_UP] = {0x000, 3};
    m_buttonMappings[(2 << 8) | BUTTON_PUNCH1] = {0x000, 4};
    m_buttonMappings[(2 << 8) | BUTTON_PUNCH2] = {0x000, 5};
    m_buttonMappings[(2 << 8) | BUTTON_PUNCH3] = {0x000, 6};
    
    // Player 2 kicks -> port 0x011 (bits 4-5) and port 0x020 (bit 6 for strong kick)
    m_buttonMappings[(2 << 8) | BUTTON_KICK1] = {0x011, 4};
    m_buttonMappings[(2 << 8) | BUTTON_KICK2] = {0x011, 5};
    m_buttonMappings[(2 << 8) | BUTTON_KICK3] = {0x020, 6};
    
    // Coins/Starts -> port 0x020
    m_buttonMappings[(1 << 8) | BUTTON_START] = {0x020, 0};
    m_buttonMappings[(2 << 8) | BUTTON_START] = {0x020, 1};
    m_buttonMappings[(1 << 8) | BUTTON_COIN] = {0x020, 4};
    m_buttonMappings[(2 << 8) | BUTTON_COIN] = {0x020, 5};

    // Diagnostic/Service -> port 0x021
    m_buttonMappings[(0 << 8) | BUTTON_DIAG] = {0x021, 1};
    m_buttonMappings[(0 << 8) | BUTTON_SERVICE] = {0x021, 2};
}

void Controller::pressButton(u8 player, ControllerButton button) {
    u16 key = (player << 8) | button;
    auto it = m_buttonMappings.find(key);
    if (it != m_buttonMappings.end()) {
        setPortBit(it->second.port, it->second.bit, true);
    }
}

void Controller::releaseButton(u8 player, ControllerButton button) {
    u16 key = (player << 8) | button;
    auto it = m_buttonMappings.find(key);
    if (it != m_buttonMappings.end()) {
        setPortBit(it->second.port, it->second.bit, false);
    }
}

void Controller::setPortBit(u16 port, u8 bit, bool pressed) {
    if (port >= m_portRegisters.size()) {
        return;
    }
    
    if (pressed) {
        m_portRegisters[port] |= (1 << bit);
    } else {
        m_portRegisters[port] &= ~(1 << bit);
    }
}

u8 Controller::readPort(u16 port) const {
    if (port >= m_portRegisters.size()) {
        return 0xFF;
    }

    u8 value = m_portRegisters[port];

    // Apply SOCD processing to directional inputs
    if (port == 0x001) {
        m_socdProcessor.lie(0, value, (1<<3), (1<<2), (1<<1), (1<<0));
    } else if (port == 0x000) {
        m_socdProcessor.lie(1, value, (1<<3), (1<<2), (1<<1), (1<<0));
    }

    return value;
}

void Controller::saveState(std::ofstream& file) {
    file.write(reinterpret_cast<const char*>(m_portRegisters.data()), m_portRegisters.size());
}

void Controller::loadState(std::ifstream& file) {
    file.read(reinterpret_cast<char*>(m_portRegisters.data()), m_portRegisters.size());
}

} // namespace cps

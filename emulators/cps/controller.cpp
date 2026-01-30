#include "controller.h"
#include "config.h"
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

bool Controller::handleInput(SDL_Event& event) {
    switch (event.type) {
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP: {
            bool pressed = event.type == SDL_EVENT_KEY_DOWN;
            switch (event.key.key) {
                case Config::Key::P1_UP:
                    handleButton(1, BUTTON_UP, pressed);
                    return true;
                case Config::Key::P1_DOWN:
                    handleButton(1, BUTTON_DOWN, pressed);
                    return true;
                case Config::Key::P1_LEFT:
                    handleButton(1, BUTTON_LEFT, pressed);
                    return true;
                case Config::Key::P1_RIGHT:
                    handleButton(1, BUTTON_RIGHT, pressed);
                    return true;
                case Config::Key::P1_PUNCH1:
                    handleButton(1, BUTTON_PUNCH1, pressed);
                    return true;
                case Config::Key::P1_PUNCH2:
                    handleButton(1, BUTTON_PUNCH2, pressed);
                    return true;
                case Config::Key::P1_PUNCH3:
                    handleButton(1, BUTTON_PUNCH3, pressed);
                    return true;
                case Config::Key::P1_KICK1:
                    handleButton(1, BUTTON_KICK1, pressed);
                    return true;
                case Config::Key::P1_KICK2:
                    handleButton(1, BUTTON_KICK2, pressed);
                    return true;
                case Config::Key::P1_KICK3:
                    handleButton(1, BUTTON_KICK3, pressed);
                    return true;
                case Config::Key::P2_UP:
                    handleButton(2, BUTTON_UP, pressed);
                    return true;
                case Config::Key::P2_DOWN:
                    handleButton(2, BUTTON_DOWN, pressed);
                    return true;
                case Config::Key::P2_LEFT:
                    handleButton(2, BUTTON_LEFT, pressed);
                    return true;
                case Config::Key::P2_RIGHT:
                    handleButton(2, BUTTON_RIGHT, pressed);
                    return true;
                case Config::Key::P2_PUNCH1:
                    handleButton(2, BUTTON_PUNCH1, pressed);
                    return true;
                case Config::Key::P2_PUNCH2:
                    handleButton(2, BUTTON_PUNCH2, pressed);
                    return true;
                case Config::Key::P2_PUNCH3:
                    handleButton(2, BUTTON_PUNCH3, pressed);
                    return true;
                case Config::Key::P2_KICK1:
                    handleButton(2, BUTTON_KICK1, pressed);
                    return true;
                case Config::Key::P2_KICK2:
                    handleButton(2, BUTTON_KICK2, pressed);
                    return true;
                case Config::Key::P2_KICK3:
                    handleButton(2, BUTTON_KICK3, pressed);
                    return true;
                case Config::Key::P1_COIN:
                    handleButton(1, BUTTON_COIN, pressed);
                    return true;
                case Config::Key::P2_COIN:
                    handleButton(2, BUTTON_COIN, pressed);
                    return true;
                case Config::Key::P1_START:
                    handleButton(1, BUTTON_START, pressed);
                    return true;
                case Config::Key::P2_START:
                    handleButton(2, BUTTON_START, pressed);
                    return true;
                case Config::Key::DIAG:
                    handleButton(0, BUTTON_DIAG, pressed);
                    return true;
                case Config::Key::SERVICE:
                    handleButton(0, BUTTON_SERVICE, pressed);
                    return true;
                default:
                    return false;
            }
        }
    }
    return false;
}

void Controller::handleButton(u8 player, ControllerButton button, bool pressed) {
    u16 key = (player << 8) | button;
    auto it = m_buttonMappings.find(key);
    if (it != m_buttonMappings.end()) {
        setPortBit(it->second.port, it->second.bit, pressed);
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

void Controller::saveState(Buffer* buf) {
    buffer_write(buf, m_portRegisters.data(), m_portRegisters.size());
}

void Controller::loadState(Buffer* buf) {
    buffer_read(buf, m_portRegisters.data(), m_portRegisters.size());
}

} // namespace cps

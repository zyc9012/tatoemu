#include "controller.h"
#include "config.h"
#include "../components/socd.h"
#include <cstring>

namespace neogeo {

Controller::Controller()
    : m_cartridge(nullptr) {
    reset();
    initButtonMappings();
}

void Controller::reset() {
    m_inputBanks.fill(0);
    m_socdProcessor.reset();

    if (Config::System == SystemType::MVS) {
        // 1/2 slot MVS
        m_inputBanks[3] |=  0x20;
        m_inputBanks[5] |=  0x40;
    }
}

void Controller::initButtonMappings() {
    // Neo Geo Button Mappings
    // Bank 0: P1 joystick + buttons A/B/C/D (0x300000)
    // Bank 1: P2 joystick + buttons A/B/C/D (0x340000)
    // Bank 2: P1/P2 start/select buttons (0x380000)
    // Bank 3: P1/P2 coin buttons + system status
    // Bank 4: Not used
    // Bank 5: Diagnostic test button

    // Player 1 directions + buttons -> bank 0
    m_buttonMappings[(1 << 8) | BUTTON_UP] = {0, 0};
    m_buttonMappings[(1 << 8) | BUTTON_DOWN] = {0, 1};
    m_buttonMappings[(1 << 8) | BUTTON_LEFT] = {0, 2};
    m_buttonMappings[(1 << 8) | BUTTON_RIGHT] = {0, 3};
    m_buttonMappings[(1 << 8) | BUTTON_A] = {0, 4};
    m_buttonMappings[(1 << 8) | BUTTON_B] = {0, 5};
    m_buttonMappings[(1 << 8) | BUTTON_C] = {0, 6};
    m_buttonMappings[(1 << 8) | BUTTON_D] = {0, 7};

    // Player 2 directions + buttons -> bank 1
    m_buttonMappings[(2 << 8) | BUTTON_UP] = {1, 0};
    m_buttonMappings[(2 << 8) | BUTTON_DOWN] = {1, 1};
    m_buttonMappings[(2 << 8) | BUTTON_LEFT] = {1, 2};
    m_buttonMappings[(2 << 8) | BUTTON_RIGHT] = {1, 3};
    m_buttonMappings[(2 << 8) | BUTTON_A] = {1, 4};
    m_buttonMappings[(2 << 8) | BUTTON_B] = {1, 5};
    m_buttonMappings[(2 << 8) | BUTTON_C] = {1, 6};
    m_buttonMappings[(2 << 8) | BUTTON_D] = {1, 7};

    // Start/Select buttons -> bank 2
    m_buttonMappings[(1 << 8) | BUTTON_START] = {2, 0};
    m_buttonMappings[(1 << 8) | BUTTON_SELECT] = {2, 1};
    m_buttonMappings[(2 << 8) | BUTTON_START] = {2, 2};
    m_buttonMappings[(2 << 8) | BUTTON_SELECT] = {2, 3};

    // Coin buttons -> bank 3
    m_buttonMappings[(1 << 8) | BUTTON_COIN] = {3, 0};
    m_buttonMappings[(2 << 8) | BUTTON_COIN] = {3, 1};

    // System buttons (test/service) -> bank 0 (for system status)
    m_buttonMappings[(0 << 8) | BUTTON_TEST] = {5, 7};
    m_buttonMappings[(0 << 8) | BUTTON_SERVICE] = {3, 2};
}

bool Controller::handleInput(SDL_Event& event) {
    switch (event.type) {
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP: {
            bool pressed = event.type == SDL_EVENT_KEY_DOWN;
            const SDL_Keycode key = event.key.key;
            if (key == Config::Key::P1_Up) {
                handleButton(1, BUTTON_UP, pressed);
                return true;
            } else if (key == Config::Key::P1_Down) {
                handleButton(1, BUTTON_DOWN, pressed);
                return true;
            } else if (key == Config::Key::P1_Left) {
                handleButton(1, BUTTON_LEFT, pressed);
                return true;
            } else if (key == Config::Key::P1_Right) {
                handleButton(1, BUTTON_RIGHT, pressed);
                return true;
            } else if (key == Config::Key::P1_ButtonA) {
                handleButton(1, BUTTON_A, pressed);
                return true;
            } else if (key == Config::Key::P1_ButtonB) {
                handleButton(1, BUTTON_B, pressed);
                return true;
            } else if (key == Config::Key::P1_ButtonC) {
                handleButton(1, BUTTON_C, pressed);
                return true;
            } else if (key == Config::Key::P1_ButtonD) {
                handleButton(1, BUTTON_D, pressed);
                return true;
            } else if (key == Config::Key::P2_Up) {
                handleButton(2, BUTTON_UP, pressed);
                return true;
            } else if (key == Config::Key::P2_Down) {
                handleButton(2, BUTTON_DOWN, pressed);
                return true;
            } else if (key == Config::Key::P2_Left) {
                handleButton(2, BUTTON_LEFT, pressed);
                return true;
            } else if (key == Config::Key::P2_Right) {
                handleButton(2, BUTTON_RIGHT, pressed);
                return true;
            } else if (key == Config::Key::P2_ButtonA) {
                handleButton(2, BUTTON_A, pressed);
                return true;
            } else if (key == Config::Key::P2_ButtonB) {
                handleButton(2, BUTTON_B, pressed);
                return true;
            } else if (key == Config::Key::P2_ButtonC) {
                handleButton(2, BUTTON_C, pressed);
                return true;
            } else if (key == Config::Key::P2_ButtonD) {
                handleButton(2, BUTTON_D, pressed);
                return true;
            } else if (key == Config::Key::P1_Coin) {
                handleButton(1, BUTTON_COIN, pressed);
                return true;
            } else if (key == Config::Key::P2_Coin) {
                handleButton(2, BUTTON_COIN, pressed);
                return true;
            } else if (key == Config::Key::P1_Start) {
                handleButton(1, BUTTON_START, pressed);
                return true;
            } else if (key == Config::Key::P2_Start) {
                handleButton(2, BUTTON_START, pressed);
                return true;
            } else if (key == Config::Key::P1_Select) {
                handleButton(1, BUTTON_SELECT, pressed);
                return true;
            } else if (key == Config::Key::P2_Select) {
                handleButton(2, BUTTON_SELECT, pressed);
                return true;
            } else if (key == Config::Key::Test) {
                handleButton(0, BUTTON_TEST, pressed);
                return true;
            } else if (key == Config::Key::Service) {
                handleButton(0, BUTTON_SERVICE, pressed);
                return true;
            }
            return false;
        }
    }
    return false;
}

void Controller::handleButton(u8 player, ControllerButton button, bool pressed) {
    u16 key = (player << 8) | button;
    auto it = m_buttonMappings.find(key);
    if (it != m_buttonMappings.end()) {
        setBankBit(it->second.bank, it->second.bit, pressed);
    }
}

void Controller::setBankBit(u8 bank, u8 bit, bool pressed) {
    if (bank >= m_inputBanks.size()) {
        return;
    }

    // Active low: pressed = clear bit, released = set bit
    if (pressed) {
        m_inputBanks[bank] |= (1 << bit);
    } else {
        m_inputBanks[bank] &= ~(1 << bit);
    }
}

u8 Controller::readInput1(u8 offset) const {
    // Input bank 1 (0x300000)

    switch (offset) {
        case 0x00: {
            u8 input = m_inputBanks[0];
            // Apply SOCD processing to directional inputs
            m_socdProcessor.lie(0, input, (1<<0), (1<<1), (1<<2), (1<<3));
            return input;
        }
        case 0x01:
            if (Config::System == SystemType::MVS) {
                return m_inputBanks[4];
            }
            return 0x00;
        case 0x81:
            if (Config::System == SystemType::MVS) {
                return m_inputBanks[5];
            }
            return 0x00;
    }
    return 0x00;  // Default open bus
}

u8 Controller::readInput2(u8 offset) const {
    // Input bank 2 (0x340000)

    if ((offset & 1) == 0) {
        u8 input = m_inputBanks[1];
        // Apply SOCD processing to directional inputs
        m_socdProcessor.lie(1, input, (1<<0), (1<<1), (1<<2), (1<<3));
        return input;
    }

    return 0x00; // Default open bus
}

u8 Controller::readInput3(u8 offset) const {
    // Input bank 3 - (0x380000)

    if ((offset & 1) == 0) {
        return m_inputBanks[2];
    }

    return 0x00; // Default open bus
}

u8 Controller::getInputBank(u8 index) const {
    return m_inputBanks[index];
}

void Controller::saveState(Buffer* buf) {
    (void)buf;
}

void Controller::loadState(Buffer* buf) {
    (void)buf;
}

} // namespace neogeo

#include "controller.h"
#include "config.h"

namespace nes {

void Controller::reset() {
    m_buttons.fill(0);
    m_shiftRegister.fill(0);
    m_strobe.fill(false);
}

bool Controller::handleInput(SDL_Event& event) {
    switch (event.type) {
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP: {
            bool pressed = event.type == SDL_EVENT_KEY_DOWN;
            switch (event.key.key) {
                case Config::Key::BUTTON_A:
                    handleButton(0, BUTTON_A, pressed);
                    return true;
                case Config::Key::BUTTON_B:
                    handleButton(0, BUTTON_B, pressed);
                    return true;
                case Config::Key::START:
                    handleButton(0, BUTTON_START, pressed);
                    return true;
                case Config::Key::SELECT_PRIMARY:
                case Config::Key::SELECT_SECONDARY:
                    handleButton(0, BUTTON_SELECT, pressed);
                    return true;
                case Config::Key::DPAD_UP:
                    handleButton(0, BUTTON_UP, pressed);
                    return true;
                case Config::Key::DPAD_DOWN:
                    handleButton(0, BUTTON_DOWN, pressed);
                    return true;
                case Config::Key::DPAD_LEFT:
                    handleButton(0, BUTTON_LEFT, pressed);
                    return true;
                case Config::Key::DPAD_RIGHT:
                    handleButton(0, BUTTON_RIGHT, pressed);
                    return true;
                // TODO: handle player 2 buttons
                default:
                    return false;
            }
        }
    }
    return false;
}

void Controller::handleButton(u8 player, ControllerButton button, bool pressed) {
    if (pressed) {
        m_buttons[player] |= (1 << button);
    } else {
        m_buttons[player] &= ~(1 << button);
    }
}

void Controller::write(u8 player, u8 value) {
    m_strobe[player] = (value & 0x01) != 0;
    
    if (m_strobe[player]) {
        // While strobe is high, continuously reload shift register
        m_shiftRegister[player] = m_buttons[player];
    }
}

u8 Controller::read(u8 player) {
    u8 result = 0x40;  // Open bus bits 6-7 typically read as 0x40
    
    if (m_strobe[player]) {
        // While strobe is high, always return button A state
        result |= (m_buttons[player] & 0x01);
    } else {
        // Return current bit and shift
        result |= (m_shiftRegister[player] & 0x01);
        m_shiftRegister[player] >>= 1;
        m_shiftRegister[player] |= 0x80;  // Shift in 1s after all buttons read
    }
    
    return result;
}

void Controller::saveState(std::ofstream& file) const {
    file.write(reinterpret_cast<const char*>(m_shiftRegister.data()), m_shiftRegister.size());
    file.write(reinterpret_cast<const char*>(m_strobe.data()), m_strobe.size());
}

void Controller::loadState(std::ifstream& file) {
    file.read(reinterpret_cast<char*>(m_shiftRegister.data()), m_shiftRegister.size());
    file.read(reinterpret_cast<char*>(m_strobe.data()), m_strobe.size());
}

} // namespace nes

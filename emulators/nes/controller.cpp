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
            const SDL_Keycode key = event.key.key;
            if (key == Config::Key::ButtonA) {
                handleButton(0, BUTTON_A, pressed);
                return true;
            } else if (key == Config::Key::ButtonB) {
                handleButton(0, BUTTON_B, pressed);
                return true;
            } else if (key == Config::Key::Start) {
                handleButton(0, BUTTON_START, pressed);
                return true;
            } else if (key == Config::Key::SelectPrimary || key == Config::Key::SelectSecondary) {
                handleButton(0, BUTTON_SELECT, pressed);
                return true;
            } else if (key == Config::Key::DpadUp) {
                handleButton(0, BUTTON_UP, pressed);
                return true;
            } else if (key == Config::Key::DpadDown) {
                handleButton(0, BUTTON_DOWN, pressed);
                return true;
            } else if (key == Config::Key::DpadLeft) {
                handleButton(0, BUTTON_LEFT, pressed);
                return true;
            } else if (key == Config::Key::DpadRight) {
                handleButton(0, BUTTON_RIGHT, pressed);
                return true;
            }
            // TODO: handle player 2 buttons
            return false;
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

void Controller::saveState(Buffer* buf) {
    buffer_write(buf, m_shiftRegister.data(), m_shiftRegister.size());
    buffer_write(buf, m_strobe.data(), m_strobe.size());
}

void Controller::loadState(Buffer* buf) {
    buffer_read(buf, m_shiftRegister.data(), m_shiftRegister.size());
    buffer_read(buf, m_strobe.data(), m_strobe.size());
}

} // namespace nes

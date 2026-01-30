#include "joypad.h"
#include "cpu.h"
#include "config.h"

namespace gb {

Joypad::Joypad()
    : m_cpu(nullptr)
    , m_buttonState(0xFF)
    , m_selectedButtons(0x00) {
}

Joypad::~Joypad() {
}

void Joypad::saveState(Buffer* buf) {
    buffer_write(buf, &m_selectedButtons, sizeof(m_selectedButtons));
}

void Joypad::loadState(Buffer* buf) {
    buffer_read(buf, &m_selectedButtons, sizeof(m_selectedButtons));
}

void Joypad::setCPU(CPU* cpu) {
    m_cpu = cpu;
}

bool Joypad::handleInput(SDL_Event& event) {
    switch (event.type) {
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP: {
            bool pressed = event.type == SDL_EVENT_KEY_DOWN;
            switch (event.key.key) {
                case Config::Key::BUTTON_A: // A button
                    handleButton(BUTTON_A, pressed);
                    return true;
                case Config::Key::BUTTON_B: // B button
                    handleButton(BUTTON_B, pressed);
                    return true;
                case Config::Key::START: // Start
                    handleButton(BUTTON_START, pressed);
                    return true;
                case Config::Key::SELECT_PRIMARY: // Select
                case Config::Key::SELECT_SECONDARY:
                    handleButton(BUTTON_SELECT, pressed);
                    return true;
                case Config::Key::DPAD_UP:
                    handleButton(BUTTON_UP, pressed);
                    return true;
                case Config::Key::DPAD_DOWN:
                    handleButton(BUTTON_DOWN, pressed);
                    return true;
                case Config::Key::DPAD_LEFT:
                    handleButton(BUTTON_LEFT, pressed);
                    return true;
                case Config::Key::DPAD_RIGHT:
                    handleButton(BUTTON_RIGHT, pressed);
                    return true;
                default:
                    return false;
            }
        }
    }
    return false;
}

void Joypad::handleButton(JoypadButton button, bool pressed) {
    if (pressed) {
        bool wasPressed = !(m_buttonState & (1 << button));
        m_buttonState &= ~(1 << button);
        
        // Request joypad interrupt on button press
        if (!wasPressed && m_cpu) {
            m_cpu->requestInterrupt(INT_JOYPAD);
        }
    } else {
        m_buttonState |= (1 << button);
    }
}

u8 Joypad::read() const {
    u8 result = m_selectedButtons | 0xC0; // Upper 2 bits always set
    
    if (!(m_selectedButtons & 0x10)) {
        // Direction buttons selected
        result |= (m_buttonState >> 4) & 0x0F;
    }
    
    if (!(m_selectedButtons & 0x20)) {
        // Action buttons selected
        result |= m_buttonState & 0x0F;
    }
    
    // If neither selected, return all 1s for lower nibble
    if ((m_selectedButtons & 0x30) == 0x30) {
        result |= 0x0F;
    }
    
    return result;
}

void Joypad::write(u8 value) {
    m_selectedButtons = value & 0x30;
}

} // namespace gb


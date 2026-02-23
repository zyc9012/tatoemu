#include "joypad.h"
#include "memory.h"
#include "config.h"

namespace gba {

Joypad::Joypad() {}
Joypad::~Joypad() {}

bool Joypad::handleInput(SDL_Event& event) {
    if (event.type != SDL_EVENT_KEY_DOWN && event.type != SDL_EVENT_KEY_UP) {
        return false;
    }
    
    bool pressed = (event.type == SDL_EVENT_KEY_DOWN);
    SDL_Keycode key = event.key.key;
    
    if (key == Config::Key::ButtonA) {
        handleButton(GBAButton::A, pressed);
    } else if (key == Config::Key::ButtonB) {
        handleButton(GBAButton::B, pressed);
    } else if (key == Config::Key::Select) {
        handleButton(GBAButton::SELECT, pressed);
    } else if (key == Config::Key::Start) {
        handleButton(GBAButton::START, pressed);
    } else if (key == Config::Key::DpadRight) {
        handleButton(GBAButton::RIGHT, pressed);
    } else if (key == Config::Key::DpadLeft) {
        handleButton(GBAButton::LEFT, pressed);
    } else if (key == Config::Key::DpadUp) {
        handleButton(GBAButton::UP, pressed);
    } else if (key == Config::Key::DpadDown) {
        handleButton(GBAButton::DOWN, pressed);
    } else if (key == Config::Key::ButtonR) {
        handleButton(GBAButton::R, pressed);
    } else if (key == Config::Key::ButtonL) {
        handleButton(GBAButton::L, pressed);
    } else {
        return false;
    }
    
    return true;
}

void Joypad::handleButton(GBAButton button, bool pressed) {
    int bit = static_cast<int>(button);
    
    if (pressed) {
        m_state &= ~(1 << bit); // Active low
    } else {
        m_state |= (1 << bit);
    }
    
    // Check for keypad interrupt
    if (m_memory) {
        u16 keycnt = m_memory->readIO16(IO::KEYCNT);
        if (keycnt & (1 << 14)) { // IRQ enable
            bool condition = (keycnt & (1 << 15)) != 0; // AND (1) or OR (0)
            u16 selected = keycnt & 0x3FF;
            u16 pressed_buttons = ~m_state & 0x3FF;
            
            bool trigger = condition ? 
                ((pressed_buttons & selected) == selected) :  // AND: all selected pressed
                ((pressed_buttons & selected) != 0);          // OR: any selected pressed
            
            if (trigger) {
                m_memory->requestIRQ(IRQ::KEYPAD);
            }
        }
    }
}

void Joypad::saveState(Buffer* buf) {
    (void)buf;
}

void Joypad::loadState(Buffer* buf) {
    (void)buf;
}

} // namespace gba

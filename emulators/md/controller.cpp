#include "controller.h"

namespace md {

void Controller::reset() {
    m_pressed.fill(false);
    m_dataLatch = 0;
    m_ctrl = 0;
    m_thPhase = 0;
    m_thIdleLines = 0;
}

void Controller::endLine() {
    // The pad's internal counter resets after roughly 1.5 ms without a strobe.
    if (++m_thIdleLines > 25) {
        m_thPhase = 0;
    }
}

bool Controller::handleInput(SDL_Event& event) {
    if (event.type != SDL_EVENT_KEY_DOWN && event.type != SDL_EVENT_KEY_UP) {
        return false;
    }

    const bool pressed = (event.type == SDL_EVENT_KEY_DOWN);
    const SDL_Keycode key = event.key.key;

    if      (key == Config::Key::DpadUp)    handleButton(Button::Up, pressed);
    else if (key == Config::Key::DpadDown)  handleButton(Button::Down, pressed);
    else if (key == Config::Key::DpadLeft)  handleButton(Button::Left, pressed);
    else if (key == Config::Key::DpadRight) handleButton(Button::Right, pressed);
    else if (key == Config::Key::ButtonA)   handleButton(Button::A, pressed);
    else if (key == Config::Key::ButtonB)   handleButton(Button::B, pressed);
    else if (key == Config::Key::ButtonC)   handleButton(Button::C, pressed);
    else if (key == Config::Key::ButtonX)   handleButton(Button::X, pressed);
    else if (key == Config::Key::ButtonY)   handleButton(Button::Y, pressed);
    else if (key == Config::Key::ButtonZ)   handleButton(Button::Z, pressed);
    else if (key == Config::Key::Start)     handleButton(Button::Start, pressed);
    else if (key == Config::Key::Mode)      handleButton(Button::Mode, pressed);
    else return false;

    return true;
}

void Controller::handleButton(Button button, bool pressed) {
    m_pressed[static_cast<u32>(button)] = pressed;
}

// The four TH phases exposed by a six-button pad.  A three-button pad only
// ever produces the two base phases.
u8 Controller::padValue(bool th) const {
    if (Config::SixButtonPad) {
        if (m_thPhase == 2 && !th) {
            // All directions read low; this is how software detects the pad.
            return static_cast<u8>(bit(Button::Start, 5) | bit(Button::A, 4));
        }
        if (m_thPhase == 3 && th) {
            return static_cast<u8>(
                bit(Button::C, 5) | bit(Button::B, 4) |
                bit(Button::Mode, 3) | bit(Button::X, 2) |
                bit(Button::Y, 1) | bit(Button::Z, 0));
        }
        if (m_thPhase == 3 && !th) {
            return static_cast<u8>(bit(Button::Start, 5) | bit(Button::A, 4) | 0x0F);
        }
    }

    if (th) {
        // ?1CB RLDU
        return static_cast<u8>(
            bit(Button::C, 5) | bit(Button::B, 4) |
            bit(Button::Right, 3) | bit(Button::Left, 2) |
            bit(Button::Down, 1) | bit(Button::Up, 0));
    }

    // ?0SA 00DU
    return static_cast<u8>(
        bit(Button::Start, 5) | bit(Button::A, 4) |
        bit(Button::Down, 1) | bit(Button::Up, 0));
}

u8 Controller::readData(u32 port) {
    if (port != 0) {
        // No device on the other ports.
        return 0x7F;
    }

    const bool th = (m_dataLatch & 0x40) != 0;
    u8 value = padValue(th);

    // Pins driven as outputs read back the latched value.
    value |= static_cast<u8>(m_dataLatch & m_ctrl);

    return static_cast<u8>((m_dataLatch & 0x80) | value);
}

void Controller::writeData(u32 port, u8 value) {
    if (port != 0) return;

    m_thIdleLines = 0;

    // A rising edge on TH advances the six-button shift sequence.
    if (!(m_dataLatch & 0x40) && (value & 0x40)) {
        m_thPhase++;
    }

    m_dataLatch = value;
}

u8 Controller::readCtrl(u32 port) const {
    return (port == 0) ? m_ctrl : 0x00;
}

void Controller::writeCtrl(u32 port, u8 value) {
    if (port != 0) return;
    m_ctrl = value;
}

void Controller::saveState(Buffer* buf) {
    buffer_write(buf, &m_dataLatch, sizeof(m_dataLatch));
    buffer_write(buf, &m_ctrl, sizeof(m_ctrl));
    buffer_write(buf, &m_thPhase, sizeof(m_thPhase));
    buffer_write(buf, &m_thIdleLines, sizeof(m_thIdleLines));
}

void Controller::loadState(Buffer* buf) {
    buffer_read(buf, &m_dataLatch, sizeof(m_dataLatch));
    buffer_read(buf, &m_ctrl, sizeof(m_ctrl));
    buffer_read(buf, &m_thPhase, sizeof(m_thPhase));
    buffer_read(buf, &m_thIdleLines, sizeof(m_thIdleLines));
}

} // namespace md

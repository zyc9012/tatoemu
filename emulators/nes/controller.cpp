#include "controller.h"

namespace nes {

Controller::Controller()
    : m_buttons(0)
    , m_shiftRegister(0)
    , m_strobe(false) {
}

void Controller::reset() {
    m_buttons = 0;
    m_shiftRegister = 0;
    m_strobe = false;
}

void Controller::pressButton(ControllerButton button) {
    m_buttons |= (1 << button);
}

void Controller::releaseButton(ControllerButton button) {
    m_buttons &= ~(1 << button);
}

bool Controller::isButtonPressed(ControllerButton button) const {
    return (m_buttons & (1 << button)) != 0;
}

void Controller::write(u8 value) {
    m_strobe = (value & 0x01) != 0;
    
    if (m_strobe) {
        // While strobe is high, continuously reload shift register
        m_shiftRegister = m_buttons;
    }
}

u8 Controller::read() {
    u8 result = 0x40;  // Open bus bits 6-7 typically read as 0x40
    
    if (m_strobe) {
        // While strobe is high, always return button A state
        result |= (m_buttons & 0x01);
    } else {
        // Return current bit and shift
        result |= (m_shiftRegister & 0x01);
        m_shiftRegister >>= 1;
        m_shiftRegister |= 0x80;  // Shift in 1s after all buttons read
    }
    
    return result;
}

void Controller::saveState(std::ofstream& file) const {
    file.write(reinterpret_cast<const char*>(&m_shiftRegister), sizeof(m_shiftRegister));
    file.write(reinterpret_cast<const char*>(&m_strobe), sizeof(m_strobe));
}

void Controller::loadState(std::ifstream& file) {
    file.read(reinterpret_cast<char*>(&m_shiftRegister), sizeof(m_shiftRegister));
    file.read(reinterpret_cast<char*>(&m_strobe), sizeof(m_strobe));
}

} // namespace nes

#include "joypad.h"
#include "cpu.h"

Joypad::Joypad()
    : m_cpu(nullptr)
    , m_buttonState(0xFF)
    , m_selectedButtons(0x00) {
}

Joypad::~Joypad() {
}

void Joypad::saveState(std::ofstream& file) const {
    file.write(reinterpret_cast<const char*>(&m_selectedButtons), sizeof(m_selectedButtons));
}

void Joypad::loadState(std::ifstream& file) {
    file.read(reinterpret_cast<char*>(&m_selectedButtons), sizeof(m_selectedButtons));
}

void Joypad::setCPU(CPU* cpu) {
    m_cpu = cpu;
}

void Joypad::pressButton(JoypadButton button) {
    bool wasPressed = !(m_buttonState & (1 << button));
    m_buttonState &= ~(1 << button);
    
    // Request joypad interrupt on button press
    if (!wasPressed && m_cpu) {
        m_cpu->requestInterrupt(INT_JOYPAD);
    }
}

void Joypad::releaseButton(JoypadButton button) {
    m_buttonState |= (1 << button);
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


#include "controller.h"
#include <cstring>

namespace neogeo {

Controller::Controller()
    : m_cartridge(nullptr) {
    reset();
}

void Controller::reset() {
    // Initialize all buttons to not pressed
    for (auto& player : m_buttonState) {
        player.fill(false);
    }
    m_testButton = false;
    m_serviceButton = false;
}

void Controller::pressButton(u8 player, ControllerButton button) {
    if (player == 0) {
        // System buttons
        if (button == BUTTON_TEST) {
            m_testButton = true;
        } else if (button == BUTTON_SERVICE) {
            m_serviceButton = true;
        }
    } else if (player >= 1 && player <= 2) {
        // Player buttons (1-indexed)
        u8 playerIndex = player - 1;
        if (button < 13) {
            m_buttonState[playerIndex][button] = true;
        }
    }
}

void Controller::releaseButton(u8 player, ControllerButton button) {
    if (player == 0) {
        // System buttons
        if (button == BUTTON_TEST) {
            m_testButton = false;
        } else if (button == BUTTON_SERVICE) {
            m_serviceButton = false;
        }
    } else if (player >= 1 && player <= 2) {
        // Player buttons (1-indexed)
        u8 playerIndex = player - 1;
        if (button < 13) {
            m_buttonState[playerIndex][button] = false;
        }
    }
}

u8 Controller::readInput1(u8 offset) const {
    // Input port 1 (0x300000)
    // offset 0x00: P1 joystick + buttons
    // offset 0x01: MVS slot status
    // offset 0x81: MVS slot status

    u8 result = 0x00;

    if (offset == 0x00) {
        // P1 joystick + buttons A/B/C/D
        if (m_buttonState[0][BUTTON_RIGHT]) result |= 0x01;
        if (m_buttonState[0][BUTTON_LEFT]) result |= 0x02;
        if (m_buttonState[0][BUTTON_DOWN]) result |= 0x04;
        if (m_buttonState[0][BUTTON_UP]) result |= 0x08;
        if (m_buttonState[0][BUTTON_A]) result |= 0x10;
        if (m_buttonState[0][BUTTON_B]) result |= 0x20;
        if (m_buttonState[0][BUTTON_C]) result |= 0x40;
        if (m_buttonState[0][BUTTON_D]) result |= 0x80;
    } else if (offset == 0x01) {
        // Return 0 for now (no buttons pressed)
        result = 0x00;
    } else if (offset == 0x81) {
        // MVS slot status: Bit 6 = 1 for 1/2 slot MVS
        // For AES, this should be 0 (no slot reporting)
        if (m_cartridge && !m_cartridge->isAES()) {
            result = 0x40;  // 1/2 slot MVS
        } else {
            result = 0x00;  // AES (no slot reporting)
        }
    }

    return ~result;
}

u8 Controller::readInput2(u8 offset) const {
    // Input port 2 (0x340000)
    
    u8 result = 0x00;
    
    if ((offset & 1) == 0) {
        // Even offset: P2 joystick + buttons
        if (m_buttonState[1][BUTTON_RIGHT]) result |= 0x01;
        if (m_buttonState[1][BUTTON_LEFT]) result |= 0x02;
        if (m_buttonState[1][BUTTON_DOWN]) result |= 0x04;
        if (m_buttonState[1][BUTTON_UP]) result |= 0x08;
        if (m_buttonState[1][BUTTON_A]) result |= 0x10;
        if (m_buttonState[1][BUTTON_B]) result |= 0x20;
        if (m_buttonState[1][BUTTON_C]) result |= 0x40;
        if (m_buttonState[1][BUTTON_D]) result |= 0x80;
    }
    // Odd offset returns 0xFF (open bus)
    
    return ~result;
}

u8 Controller::readInput3(u8 offset) const {
    // Input port 3 - used for both 0x320001 and 0x380000

    u8 result = 0x00;

    if (offset == 0x00) {
        // Bit 4-5: Memory card CD1/READY (set if card inserted)
        // Bit 6: Memory card WP (write protect, set if writable)
        // Bit 7: 1 = AES, 0 = MVS
        result = 0x70;  // Memory card inserted and writable

        // Set AES/MVS bit based on cartridge type
        if (m_cartridge && m_cartridge->isAES()) {
            result |= 0x80;  // AES mode (bit 7 = 1)
        } else {
            // MVS mode (bit 7 = 0) - already set by default
        }

        if (m_testButton) result |= 0x01;
        if (m_serviceButton) result |= 0x02;
    } else if (offset == 0x01) {
        // For MVS: Bit 5 indicates slot count (1 for 1/2 slot or 4 slot)
        // For AES: This bit is not used (should be 0)
        if (m_cartridge && !m_cartridge->isAES()) {
            result = 0x20;  // 1/2 slot MVS
        } else {
            result = 0x00;  // AES (no slot reporting)
        }
    }

    return ~result;
}

void Controller::saveState(std::ofstream& file) {
    file.write(reinterpret_cast<const char*>(m_buttonState.data()), sizeof(m_buttonState));
    file.write(reinterpret_cast<const char*>(&m_testButton), sizeof(m_testButton));
    file.write(reinterpret_cast<const char*>(&m_serviceButton), sizeof(m_serviceButton));
}

void Controller::loadState(std::ifstream& file) {
    file.read(reinterpret_cast<char*>(m_buttonState.data()), sizeof(m_buttonState));
    file.read(reinterpret_cast<char*>(&m_testButton), sizeof(m_testButton));
    file.read(reinterpret_cast<char*>(&m_serviceButton), sizeof(m_serviceButton));
}

} // namespace neogeo

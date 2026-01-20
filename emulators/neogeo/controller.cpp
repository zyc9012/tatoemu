#include "controller.h"
#include <cstring>

namespace neogeo {

Controller::Controller()
    : m_cartridge(nullptr) {
    reset();
    initButtonMappings();
}

void Controller::reset() {
    // Initialize all port registers to 0xFF (active low, so 0xFF = no inputs)
    m_portRegisters.fill(0xFF);
}

void Controller::initButtonMappings() {
    // Neo Geo Button Mappings
    // Port 0: P1 joystick + buttons A/B/C/D (0x300000)
    // Port 1: P2 joystick + buttons A/B/C/D (0x340000)
    // Port 2: P1/P2 start/select buttons (0x380000)
    // Port 3: P1/P2 coin buttons (for system status at 0x320001)

    // Player 1 directions + buttons -> port 0
    m_buttonMappings[(1 << 8) | BUTTON_UP] = {0, 0};
    m_buttonMappings[(1 << 8) | BUTTON_DOWN] = {0, 1};
    m_buttonMappings[(1 << 8) | BUTTON_LEFT] = {0, 2};
    m_buttonMappings[(1 << 8) | BUTTON_RIGHT] = {0, 3};
    m_buttonMappings[(1 << 8) | BUTTON_A] = {0, 4};
    m_buttonMappings[(1 << 8) | BUTTON_B] = {0, 5};
    m_buttonMappings[(1 << 8) | BUTTON_C] = {0, 6};
    m_buttonMappings[(1 << 8) | BUTTON_D] = {0, 7};

    // Player 2 directions + buttons -> port 1
    m_buttonMappings[(2 << 8) | BUTTON_UP] = {1, 0};
    m_buttonMappings[(2 << 8) | BUTTON_DOWN] = {1, 1};
    m_buttonMappings[(2 << 8) | BUTTON_LEFT] = {1, 2};
    m_buttonMappings[(2 << 8) | BUTTON_RIGHT] = {1, 3};
    m_buttonMappings[(2 << 8) | BUTTON_A] = {1, 4};
    m_buttonMappings[(2 << 8) | BUTTON_B] = {1, 5};
    m_buttonMappings[(2 << 8) | BUTTON_C] = {1, 6};
    m_buttonMappings[(2 << 8) | BUTTON_D] = {1, 7};

    // Start/Select buttons -> port 2
    m_buttonMappings[(1 << 8) | BUTTON_START] = {2, 0};
    m_buttonMappings[(1 << 8) | BUTTON_SELECT] = {2, 1};
    m_buttonMappings[(2 << 8) | BUTTON_START] = {2, 2};
    m_buttonMappings[(2 << 8) | BUTTON_SELECT] = {2, 3};

    // Coin buttons -> port 3
    m_buttonMappings[(1 << 8) | BUTTON_COIN] = {3, 0};
    m_buttonMappings[(2 << 8) | BUTTON_COIN] = {3, 1};

    // System buttons (test/service) -> port 0 (for system status)
    m_buttonMappings[(0 << 8) | BUTTON_TEST] = {0, 0};    // Test (bit 0)
    m_buttonMappings[(0 << 8) | BUTTON_SERVICE] = {0, 1}; // Service (bit 1)
}

void Controller::pressButton(u8 player, ControllerButton button) {
    u16 key = (player << 8) | button;
    auto it = m_buttonMappings.find(key);
    if (it != m_buttonMappings.end()) {
        setPortBit(it->second.port, it->second.bit, true);
    }
}

void Controller::releaseButton(u8 player, ControllerButton button) {
    u16 key = (player << 8) | button;
    auto it = m_buttonMappings.find(key);
    if (it != m_buttonMappings.end()) {
        setPortBit(it->second.port, it->second.bit, false);
    }
}

void Controller::setPortBit(u8 port, u8 bit, bool pressed) {
    if (port >= m_portRegisters.size()) {
        return;
    }

    // Active low: pressed = clear bit, released = set bit
    if (pressed) {
        m_portRegisters[port] &= ~(1 << bit);
    } else {
        m_portRegisters[port] |= (1 << bit);
    }
}

u8 Controller::readInput1(u8 offset) const {
    // Input port 1 (0x300000)

    if (offset == 0x00) {
        // P1 joystick + buttons A/B/C/D -> port 0
        return m_portRegisters[0];
    } else if (offset == 0x01) {
        // P1/P2 start/select buttons -> port 2 (bits 0-3)
        return (m_portRegisters[2] & 0x0F) | 0xF0;  // Only bits 0-3, others high
    } else if (offset == 0x81) {
        // MVS slot status: Bit 6 = 1 for 1/2 slot MVS
        // For AES, this should be 0 (no slot reporting)
        u8 result = 0x00;
        if (m_cartridge && !m_cartridge->isAES()) {
            result = 0x40;  // 1/2 slot MVS
        }
        return ~result;
    }

    return 0xFF;  // Default open bus
}

u8 Controller::readInput2(u8 offset) const {
    // Input port 2 (0x340000)

    if ((offset & 1) == 0) {
        // Even offset: P2 joystick + buttons A/B/C/D -> port 1
        return m_portRegisters[1];
    }
    // Odd offset returns 0xFF (open bus)

    return 0xFF;
}

u8 Controller::readInput3(u8 offset) const {
    // Input port 3 - used for 0x380000 (start/select buttons) and coin buttons for system status

    if (offset == 0x00) {
        // 0x380000: P1/P2 start/select buttons -> port 2
        return m_portRegisters[2];
    } else if (offset == 0x01) {
        // Coin buttons (used by system status read at 0x320001) -> port 3
        return m_portRegisters[3];
    }

    return 0xFF;  // Default open bus
}

void Controller::saveState(std::ofstream& file) {
    (void)file;
}

void Controller::loadState(std::ifstream& file) {
    (void)file;
}

} // namespace neogeo

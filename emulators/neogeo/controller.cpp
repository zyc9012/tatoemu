#include "controller.h"
#include <cstring>

namespace neogeo {

Controller::Controller()
    : m_cartridge(nullptr) {
    reset();
    initButtonMappings();
}

void Controller::reset() {
    m_inputBanks.fill(0);

    if (m_cartridge && m_cartridge->getSystemType() == SystemType::MVS) {
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

void Controller::pressButton(u8 player, ControllerButton button) {
    u16 key = (player << 8) | button;
    auto it = m_buttonMappings.find(key);
    if (it != m_buttonMappings.end()) {
        setBankBit(it->second.bank, it->second.bit, true);
    }
}

void Controller::releaseButton(u8 player, ControllerButton button) {
    u16 key = (player << 8) | button;
    auto it = m_buttonMappings.find(key);
    if (it != m_buttonMappings.end()) {
        setBankBit(it->second.bank, it->second.bit, false);
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
        case 0x00:
            return m_inputBanks[0];
        case 0x01:
            if (m_cartridge && m_cartridge->getSystemType() == SystemType::MVS) {
                return m_inputBanks[4];
            }
            return 0x00;
        case 0x81:
            if (m_cartridge && m_cartridge->getSystemType() == SystemType::MVS) {
                return m_inputBanks[5];
            }
            return 0x00;
    }
    return 0x00;  // Default open bus
}

u8 Controller::readInput2(u8 offset) const {
    // Input bank 2 (0x340000)

    if ((offset & 1) == 0) {
        return m_inputBanks[1];
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

void Controller::saveState(std::ofstream& file) {
    (void)file;
}

void Controller::loadState(std::ifstream& file) {
    (void)file;
}

} // namespace neogeo

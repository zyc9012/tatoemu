#include "controller.h"
#include "memory_base.h"

namespace cps {

Controller::Controller()
    : m_buttons(0)
    , m_coinInserted(false)
    , m_memory(nullptr) {
}

void Controller::reset() {
    m_buttons = 0;
    m_coinInserted = false;
}

void Controller::pressButton(ControllerButton button) {
    m_buttons |= (1 << button);
}

void Controller::releaseButton(ControllerButton button) {
    m_buttons &= ~(1 << button);
}

void Controller::insertCoin() {
    m_coinInserted = true;
}

u8 Controller::read() const {
    // Return button states as a byte
    // Format depends on CPS I/O mapping
    return static_cast<u8>(m_buttons & 0xFF);
}

void Controller::saveState(std::ofstream& file) {
    file.write(reinterpret_cast<const char*>(&m_buttons), sizeof(m_buttons));
    file.write(reinterpret_cast<const char*>(&m_coinInserted), sizeof(m_coinInserted));
}

void Controller::loadState(std::ifstream& file) {
    file.read(reinterpret_cast<char*>(&m_buttons), sizeof(m_buttons));
    file.read(reinterpret_cast<char*>(&m_coinInserted), sizeof(m_coinInserted));
}

} // namespace cps

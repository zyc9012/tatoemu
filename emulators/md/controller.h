#pragma once

#include "consts.h"
#include "config.h"
#include "../types.h"
#include "../components/buffer.h"
#include <SDL3/SDL.h>
#include <array>

namespace md {

// Buttons in the order they appear in the pad's shift register.
enum class Button {
    Up, Down, Left, Right,
    A, B, C, Start,
    X, Y, Z, Mode,
    Count
};

// ---------------------------------------------------------------------------
// Control pad on port 1, with optional six-button support.
// ---------------------------------------------------------------------------
class Controller {
public:
    Controller() { reset(); }

    void reset();

    bool handleInput(SDL_Event& event);
    void handleButton(Button button, bool pressed);

    // Called once per scanline; the pad's TH phase counter times out when the
    // console stops strobing it.
    void endLine();

    u8 readData(u32 port);
    void writeData(u32 port, u8 value);
    u8 readCtrl(u32 port) const;
    void writeCtrl(u32 port, u8 value);

    void saveState(Buffer* buf);
    void loadState(Buffer* buf);

private:
    // Returns 0 when the button is held (the pad is active low).
    u8 bit(Button button, u32 shift) const {
        return m_pressed[static_cast<u32>(button)] ? 0 : static_cast<u8>(1u << shift);
    }

    u8 padValue(bool th) const;

    std::array<bool, static_cast<u32>(Button::Count)> m_pressed{};

    u8 m_dataLatch = 0;
    u8 m_ctrl = 0;
    u8 m_thPhase = 0;
    u32 m_thIdleLines = 0;
};

} // namespace md

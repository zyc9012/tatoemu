#pragma once

#include <SDL3/SDL.h>
#include "../types.h"

namespace md {

namespace Config {
namespace Key {
    // Player 1 — three-button layout maps to A/S/D, six-button adds Q/W/E.
    inline SDL_Keycode ButtonX = SDLK_A;
    inline SDL_Keycode ButtonY = SDLK_S;
    inline SDL_Keycode ButtonZ = SDLK_D;
    inline SDL_Keycode ButtonA = SDLK_Z;
    inline SDL_Keycode ButtonB = SDLK_X;
    inline SDL_Keycode ButtonC = SDLK_C;
    inline SDL_Keycode Start   = SDLK_RETURN;
    inline SDL_Keycode Mode    = SDLK_RSHIFT;
    inline SDL_Keycode DpadUp    = SDLK_UP;
    inline SDL_Keycode DpadDown  = SDLK_DOWN;
    inline SDL_Keycode DpadLeft  = SDLK_LEFT;
    inline SDL_Keycode DpadRight = SDLK_RIGHT;
}

// Controller type reported to the console for port 1.
// true  = 6-button Fighting Pad, false = 3-button Control Pad.
inline bool SixButtonPad = true;

// Region reported through the version register.
// 0 = Japan/NTSC, 1 = USA/NTSC, 2 = Europe/PAL
inline u32 Region = 1;

} // namespace Config

} // namespace md

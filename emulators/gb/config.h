#pragma once

#include <SDL3/SDL.h>
#include "types.h"

namespace gb {

namespace Config {
namespace Key {
    inline SDL_Keycode ButtonA = SDLK_Z;
    inline SDL_Keycode ButtonB = SDLK_X;
    inline SDL_Keycode Start = SDLK_RETURN;
    inline SDL_Keycode SelectPrimary = SDLK_RSHIFT;
    inline SDL_Keycode SelectSecondary = SDLK_LSHIFT;
    inline SDL_Keycode DpadUp = SDLK_UP;
    inline SDL_Keycode DpadDown = SDLK_DOWN;
    inline SDL_Keycode DpadLeft = SDLK_LEFT;
    inline SDL_Keycode DpadRight = SDLK_RIGHT;
}

} // namespace Config

} // namespace gb

#pragma once

#include <SDL3/SDL.h>
#include "../types.h"

namespace nes {

namespace Config {
namespace Key {
    // NES controller mapping
    constexpr SDL_Keycode BUTTON_A = SDLK_Z;
    constexpr SDL_Keycode BUTTON_B = SDLK_X;
    constexpr SDL_Keycode START = SDLK_RETURN;
    constexpr SDL_Keycode SELECT_PRIMARY = SDLK_RSHIFT;
    constexpr SDL_Keycode SELECT_SECONDARY = SDLK_LSHIFT;
    constexpr SDL_Keycode DPAD_UP = SDLK_UP;
    constexpr SDL_Keycode DPAD_DOWN = SDLK_DOWN;
    constexpr SDL_Keycode DPAD_LEFT = SDLK_LEFT;
    constexpr SDL_Keycode DPAD_RIGHT = SDLK_RIGHT;
}

} // namespace Config

} // namespace nes

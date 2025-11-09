#pragma once

#include <SDL3/SDL.h>
#include "types.h"

namespace Config {
namespace Window {
    constexpr u32 SCALE = 4;
    constexpr SDL_ScaleMode SCALE_MODE = SDL_SCALEMODE_LINEAR;
}

namespace Audio {
    constexpr u32 SAMPLE_RATE = 44100;
    constexpr float VOLUME = 0.2f;
}

namespace Key {
    constexpr SDL_Keycode QUIT = SDLK_ESCAPE;
    constexpr SDL_Keycode BUTTON_A = SDLK_Z;
    constexpr SDL_Keycode BUTTON_B = SDLK_X;
    constexpr SDL_Keycode START = SDLK_RETURN;
    constexpr SDL_Keycode SELECT_PRIMARY = SDLK_RSHIFT;
    constexpr SDL_Keycode SELECT_SECONDARY = SDLK_LSHIFT;
    constexpr SDL_Keycode DPAD_UP = SDLK_UP;
    constexpr SDL_Keycode DPAD_DOWN = SDLK_DOWN;
    constexpr SDL_Keycode DPAD_LEFT = SDLK_LEFT;
    constexpr SDL_Keycode DPAD_RIGHT = SDLK_RIGHT;
    constexpr SDL_Keycode SAVE_STATE = SDLK_F5;
    constexpr SDL_Keycode LOAD_STATE = SDLK_F9;
    constexpr SDL_Keycode PAUSE = SDLK_P;
    constexpr SDL_Keycode GAME_SPEED_UP = SDLK_EQUALS;
    constexpr SDL_Keycode GAME_SPEED_DOWN = SDLK_MINUS;
}

} // namespace Config

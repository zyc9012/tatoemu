#pragma once

#include <SDL3/SDL.h>
#include "types.h"

namespace Config {
namespace Window {
    inline u32 Scale = 2;
    inline SDL_ScaleMode ScaleMode = SDL_SCALEMODE_LINEAR;
}

namespace Audio {
    inline u32 SampleRate = 44100;
    inline float Volume = 0.3f;
}

namespace Key {
    constexpr SDL_Keycode QUIT = SDLK_ESCAPE;
    constexpr SDL_Keycode SAVE_STATE = SDLK_F5;
    constexpr SDL_Keycode LOAD_STATE = SDLK_F9;
    constexpr SDL_Keycode PAUSE = SDLK_P;
    constexpr SDL_Keycode GAME_SPEED_UP = SDLK_EQUALS;
    constexpr SDL_Keycode GAME_SPEED_DOWN = SDLK_MINUS;
}

} // namespace Config

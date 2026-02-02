#pragma once

#include <SDL3/SDL.h>
#include "types.h"

namespace Config {
namespace Window {
    inline u32 Scale = 0; // Auto
    inline SDL_ScaleMode ScaleMode = SDL_SCALEMODE_LINEAR;
}

namespace Audio {
    inline u32 SampleRate = 44100;
    inline float Volume = 0.3f;
}

namespace Key {
    inline SDL_Keycode Quit = SDLK_ESCAPE;
    inline SDL_Keycode SaveState = SDLK_F5;
    inline SDL_Keycode LoadState = SDLK_F9;
    inline SDL_Keycode Pause = SDLK_P;
    inline SDL_Keycode GameSpeedUp = SDLK_EQUALS;
    inline SDL_Keycode GameSpeedDown = SDLK_MINUS;
}

} // namespace Config

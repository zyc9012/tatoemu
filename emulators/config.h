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
    inline u32 Volume = 60;
}

namespace Key {
    inline SDL_Keycode Quit = SDLK_ESCAPE;
    inline SDL_Keycode SaveState = SDLK_F5;
    inline SDL_Keycode LoadState = SDLK_F9;
    inline SDL_Keycode LoadState_Backup1 = SDLK_F10;
    inline SDL_Keycode LoadState_Backup2 = SDLK_F11;
    inline SDL_Keycode LoadState_Backup3 = SDLK_F12;
    inline SDL_Keycode Pause = SDLK_P;
    inline SDL_Keycode GameSpeedUp = SDLK_EQUALS;
    inline SDL_Keycode GameSpeedDown = SDLK_MINUS;
    inline SDL_Keycode CheatConsole = SDLK_GRAVE;
}

} // namespace Config

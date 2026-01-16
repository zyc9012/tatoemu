#pragma once

#include <SDL3/SDL.h>
#include "../types.h"

namespace cps {

namespace Config {
namespace Key {
    // CPS arcade controls (6-button layout - shared between CPS1 and CPS2)
    constexpr SDL_Keycode P1_UP = SDLK_UP;
    constexpr SDL_Keycode P1_DOWN = SDLK_DOWN;
    constexpr SDL_Keycode P1_LEFT = SDLK_LEFT;
    constexpr SDL_Keycode P1_RIGHT = SDLK_RIGHT;
    constexpr SDL_Keycode P1_PUNCH1 = SDLK_A;
    constexpr SDL_Keycode P1_PUNCH2 = SDLK_S;
    constexpr SDL_Keycode P1_PUNCH3 = SDLK_D;
    constexpr SDL_Keycode P1_KICK1 = SDLK_Z;
    constexpr SDL_Keycode P1_KICK2 = SDLK_X;
    constexpr SDL_Keycode P1_KICK3 = SDLK_C;
    
    constexpr SDL_Keycode P2_UP = SDLK_KP_8;
    constexpr SDL_Keycode P2_DOWN = SDLK_KP_5;
    constexpr SDL_Keycode P2_LEFT = SDLK_KP_4;
    constexpr SDL_Keycode P2_RIGHT = SDLK_KP_6;
    constexpr SDL_Keycode P2_PUNCH1 = SDLK_J;
    constexpr SDL_Keycode P2_PUNCH2 = SDLK_K;
    constexpr SDL_Keycode P2_PUNCH3 = SDLK_L;
    constexpr SDL_Keycode P2_KICK1 = SDLK_M;
    constexpr SDL_Keycode P2_KICK2 = SDLK_COMMA;
    constexpr SDL_Keycode P2_KICK3 = SDLK_PERIOD;
    
    constexpr SDL_Keycode P1_COIN = SDLK_5;
    constexpr SDL_Keycode P2_COIN = SDLK_6;
    constexpr SDL_Keycode P1_START = SDLK_1;
    constexpr SDL_Keycode P2_START = SDLK_2;

    constexpr SDL_Keycode DIAG = SDLK_F2;
    constexpr SDL_Keycode SERVICE = SDLK_F3;
}
} // namespace Config

} // namespace cps

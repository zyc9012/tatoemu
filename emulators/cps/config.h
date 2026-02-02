#pragma once

#include <SDL3/SDL.h>
#include "../types.h"

namespace cps {

namespace Config {
namespace Key {
    // CPS arcade controls (6-button layout - shared between CPS1 and CPS2)
    inline SDL_Keycode P1_Up = SDLK_UP;
    inline SDL_Keycode P1_Down = SDLK_DOWN;
    inline SDL_Keycode P1_Left = SDLK_LEFT;
    inline SDL_Keycode P1_Right = SDLK_RIGHT;
    inline SDL_Keycode P1_Punch1 = SDLK_A;
    inline SDL_Keycode P1_Punch2 = SDLK_S;
    inline SDL_Keycode P1_Punch3 = SDLK_D;
    inline SDL_Keycode P1_Kick1 = SDLK_Z;
    inline SDL_Keycode P1_Kick2 = SDLK_X;
    inline SDL_Keycode P1_Kick3 = SDLK_C;
    
    inline SDL_Keycode P2_Up = SDLK_KP_8;
    inline SDL_Keycode P2_Down = SDLK_KP_5;
    inline SDL_Keycode P2_Left = SDLK_KP_4;
    inline SDL_Keycode P2_Right = SDLK_KP_6;
    inline SDL_Keycode P2_Punch1 = SDLK_J;
    inline SDL_Keycode P2_Punch2 = SDLK_K;
    inline SDL_Keycode P2_Punch3 = SDLK_L;
    inline SDL_Keycode P2_Kick1 = SDLK_M;
    inline SDL_Keycode P2_Kick2 = SDLK_COMMA;
    inline SDL_Keycode P2_Kick3 = SDLK_PERIOD;
    
    inline SDL_Keycode P1_Coin = SDLK_5;
    inline SDL_Keycode P2_Coin = SDLK_6;
    inline SDL_Keycode P1_Start = SDLK_1;
    inline SDL_Keycode P2_Start = SDLK_2;

    inline SDL_Keycode Diag = SDLK_F2;
    inline SDL_Keycode Service = SDLK_F3;
}
} // namespace Config

} // namespace cps

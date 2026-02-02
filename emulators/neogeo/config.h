#pragma once

#include <SDL3/SDL.h>
#include "../types.h"
#include "cartridge.h"

namespace neogeo {

namespace Config {
namespace Key {
    // NeoGeo arcade controls (4-button layout)
    inline SDL_Keycode P1_Up = SDLK_UP;
    inline SDL_Keycode P1_Down = SDLK_DOWN;
    inline SDL_Keycode P1_Left = SDLK_LEFT;
    inline SDL_Keycode P1_Right = SDLK_RIGHT;
    inline SDL_Keycode P1_ButtonA = SDLK_A;
    inline SDL_Keycode P1_ButtonB = SDLK_S;
    inline SDL_Keycode P1_ButtonC = SDLK_D;
    inline SDL_Keycode P1_ButtonD = SDLK_F;
    
    inline SDL_Keycode P2_Up = SDLK_KP_8;
    inline SDL_Keycode P2_Down = SDLK_KP_5;
    inline SDL_Keycode P2_Left = SDLK_KP_4;
    inline SDL_Keycode P2_Right = SDLK_KP_6;
    inline SDL_Keycode P2_ButtonA = SDLK_J;
    inline SDL_Keycode P2_ButtonB = SDLK_K;
    inline SDL_Keycode P2_ButtonC = SDLK_L;
    inline SDL_Keycode P2_ButtonD = SDLK_SEMICOLON;
    
    inline SDL_Keycode P1_Coin = SDLK_5;
    inline SDL_Keycode P2_Coin = SDLK_6;
    inline SDL_Keycode P1_Start = SDLK_1;
    inline SDL_Keycode P2_Start = SDLK_2;
    inline SDL_Keycode P1_Select = SDLK_3;
    inline SDL_Keycode P2_Select = SDLK_4;

    inline SDL_Keycode Test = SDLK_F2;
    inline SDL_Keycode Service = SDLK_F3;
}

// Default to MVS
inline SystemType System = SystemType::MVS;

// Default to Universe BIOS 4.0
inline u8 BiosIndex = 19;

} // namespace Config

} // namespace neogeo

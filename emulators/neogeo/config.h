#pragma once

#include <SDL3/SDL.h>
#include "../types.h"

namespace neogeo {

namespace Config {
namespace Key {
    // NeoGeo arcade controls (4-button layout)
    constexpr SDL_Keycode P1_UP = SDLK_UP;
    constexpr SDL_Keycode P1_DOWN = SDLK_DOWN;
    constexpr SDL_Keycode P1_LEFT = SDLK_LEFT;
    constexpr SDL_Keycode P1_RIGHT = SDLK_RIGHT;
    constexpr SDL_Keycode P1_BUTTON_A = SDLK_A;
    constexpr SDL_Keycode P1_BUTTON_B = SDLK_S;
    constexpr SDL_Keycode P1_BUTTON_C = SDLK_D;
    constexpr SDL_Keycode P1_BUTTON_D = SDLK_F;
    
    constexpr SDL_Keycode P2_UP = SDLK_KP_8;
    constexpr SDL_Keycode P2_DOWN = SDLK_KP_5;
    constexpr SDL_Keycode P2_LEFT = SDLK_KP_4;
    constexpr SDL_Keycode P2_RIGHT = SDLK_KP_6;
    constexpr SDL_Keycode P2_BUTTON_A = SDLK_J;
    constexpr SDL_Keycode P2_BUTTON_B = SDLK_K;
    constexpr SDL_Keycode P2_BUTTON_C = SDLK_L;
    constexpr SDL_Keycode P2_BUTTON_D = SDLK_SEMICOLON;
    
    constexpr SDL_Keycode P1_COIN = SDLK_5;
    constexpr SDL_Keycode P2_COIN = SDLK_6;
    constexpr SDL_Keycode P1_START = SDLK_1;
    constexpr SDL_Keycode P2_START = SDLK_2;
    constexpr SDL_Keycode P1_SELECT = SDLK_3;
    constexpr SDL_Keycode P2_SELECT = SDLK_4;

    constexpr SDL_Keycode TEST = SDLK_F2;
    constexpr SDL_Keycode SERVICE = SDLK_F3;
}
} // namespace Config

} // namespace neogeo

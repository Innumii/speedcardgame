#pragma once

#include <SDL2/SDL.h>

class RenderCardOverlay {
public:
    static void overlayShimmer(SDL_Renderer* renderer, const SDL_Rect& panel, Uint32 now); //for spell usage
    static void overlaySSR(SDL_Renderer* renderer, const SDL_Rect& panel, Uint32 now);
};
#ifndef RENDER_BACKDROP_HPP
#define RENDER_BACKDROP_HPP

#include <SDL2/SDL.h>

class RenderBackdrop {
public:
    static void drawBackgroundWithVignette(
        SDL_Renderer* renderer,
        int screenW,
        int screenH,
        SDL_Color background,
        SDL_Color vignetteColor,
        int vignetteLayers,
        float vignetteAlphaFalloff,
        Uint8 vignetteMaxAlpha
    );
};

#endif

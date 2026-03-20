#include "render/RenderBackdrop.hpp"

void RenderBackdrop::drawBackgroundWithVignette(
    SDL_Renderer* renderer,
    int screenW,
    int screenH,
    SDL_Color background,
    SDL_Color vignetteColor,
    int vignetteLayers,
    float vignetteAlphaFalloff,
    Uint8 vignetteMaxAlpha
) {
    if (!renderer || screenW <= 0 || screenH <= 0 || vignetteLayers <= 0) {
        return;
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(renderer, background.r, background.g, background.b, background.a);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    for (int i = 0; i < vignetteLayers; ++i) {
        const int alphaValue = static_cast<int>(vignetteMaxAlpha) - static_cast<int>(i * vignetteAlphaFalloff);
        if (alphaValue <= 0) {
            break;
        }

        SDL_SetRenderDrawColor(
            renderer,
            vignetteColor.r,
            vignetteColor.g,
            vignetteColor.b,
            static_cast<Uint8>(alphaValue)
        );

        SDL_Rect edge{i, i, screenW - 2 * i, screenH - 2 * i};
        if (edge.w <= 0 || edge.h <= 0) {
            break;
        }

        SDL_RenderDrawRect(renderer, &edge);
    }
}

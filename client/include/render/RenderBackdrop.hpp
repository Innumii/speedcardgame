#pragma once
#include <SDL2/SDL.h>
#include <vector>

class RenderBackdrop {
public:
    static float getElapsed();
    static void resetElapsed();

    static void drawBackgroundWithVignette(
        SDL_Renderer* renderer,
        int screenW, int screenH,
        SDL_Color background,
        SDL_Color vignetteColor,
        int vignetteLayers,
        float vignetteAlphaFalloff,
        Uint8 vignetteMaxAlpha
    );

    static void drawAnimatedWaves(
        SDL_Renderer* renderer,
        int screenW, int screenH,
        SDL_Color waveColor,
        float phase,
        float fillRatio    = 0.35f,
        float amplitude    = 14.0f,
        float frequency    = 0.018f,
        int   gradientBands = 14
    );

    static void drawStars(
        SDL_Renderer* renderer,
        int screenW, int screenH,
        float phase,
        float skyFraction,   // portion of screen above the waves, e.g. 1.0f - fillRatio
        SDL_Color starColor,
        int numStars = 18
    );

    static void drawTitleBackdrop(
        SDL_Renderer* renderer,
        int screenW, int screenH
    );

private:
    static float evalWave(float x, float phase, float amplitude, float frequency);
    static bool initialized;
    static Uint32 startTick;
        struct StarParticle {
        float xFrac;        // 0–1 horizontal position
        float yFrac;        // 0–1 within the sky area
        float phaseOffset;  // stagger glint timing
        float rotSpeed;     // rotation speed multiplier
        float size;         // arm length at full glint
    };
};
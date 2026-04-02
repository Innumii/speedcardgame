#include "render/RenderBackdrop.hpp"
#include <SDL2/SDL.h>
#include <cmath>
#include <algorithm>
#include <vector>
#include <render/Theme.hpp>

bool   RenderBackdrop::initialized = false;
Uint32 RenderBackdrop::startTick   = 0;

void RenderBackdrop::drawTitleBackdrop(
    SDL_Renderer* renderer,
    int screenW, int screenH
) {
    const float elapsed = getElapsed();

    drawBackgroundWithVignette(
        renderer, screenW, screenH,
        Theme::BG,
        SDL_Color{0, 0, 0, 255},
        80, 1.5f, 120
    );

    // ── Rise transition ───────────────────────────────────────────────
    // Over the first `riseDuration` seconds, fillRatio eases from 0
    // (waves off the bottom) up to the target value.
    constexpr float targetFill   = 0.28f;
    constexpr float riseDuration = 1.0f;

    const float riseT    = std::min(elapsed / riseDuration, 1.0f);
    // Ease out cubic — fast rise that settles gently at the target
    const float riseEase = 1.0f - (1.0f - riseT) * (1.0f - riseT) * (1.0f - riseT);
    const float fillRatio = targetFill * riseEase;

    drawAnimatedWaves(
        renderer, screenW, screenH,
        SDL_Color{60, 140, 200, 220},
        elapsed * 2.2f,
        fillRatio,   // <-- rises from 0 to targetFill
        20.0f, 0.016f, 16
    );

    drawStars(
        renderer, screenW, screenH,
        elapsed,
        0.62f,
        Theme::BANNER_GLOW,
        18
    );

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

float RenderBackdrop::getElapsed() {

    if (!initialized) {
        startTick   = SDL_GetTicks();
        initialized = true;
    }

    return (SDL_GetTicks() - startTick) / 1000.0f;
}

void RenderBackdrop::resetElapsed() {
    initialized = false;
    startTick = 0;
}

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
// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Sum of three harmonics — gives an organic, non-repetitive wave shape.
float RenderBackdrop::evalWave(
    float x, float phase, float amplitude, float frequency
) {
    // Slow drifting terrain — shifts the baseline per-column dramatically
    const float terrain =
        0.45f * std::sin(x * 0.0021f + phase * 0.13f) +
        0.35f * std::sin(x * 0.0047f + phase * 0.09f + 2.1f) +
        0.20f * std::sin(x * 0.0011f + phase * 0.07f + 4.4f);

    // Surface ripple on top of the terrain
    const float ripple =
        0.60f * std::sin(x * frequency        + phase) +
        0.25f * std::sin(x * frequency * 2.3f + phase * 1.4f + 1.00f) +
        0.15f * std::sin(x * frequency * 3.7f + phase * 0.7f + 2.55f);

    return (amplitude * 4.5f * terrain) + (amplitude * ripple);
}

static SDL_Color blendBrighter(SDL_Color c, int lift) {
    return {
        static_cast<Uint8>(std::min(255, c.r + lift)),
        static_cast<Uint8>(std::min(255, c.g + lift)),
        static_cast<Uint8>(std::min(255, c.b + lift)),
        c.a
    };
}

// ---------------------------------------------------------------------------
// Wave renderer
// ---------------------------------------------------------------------------

void RenderBackdrop::drawAnimatedWaves(
    SDL_Renderer* renderer,
    int screenW, int screenH,
    SDL_Color waveColor,
    float phase,
    float fillRatio,
    float amplitude,
    float frequency,
    int gradientBands
) {
    if (!renderer || screenW <= 0 || screenH <= 0) return;

    const float baseY = screenH * (1.0f - fillRatio);

    struct WaveLayer {
        float phaseScale;
        float ampScale;
        float freqScale;
        float yOffset;
        int   colourLift;
        Uint8 alpha;
    };

    const WaveLayer layers[] = {
        { 0.55f, 0.75f, 1.30f, +10.0f,  0,  60 },
        { 1.00f, 1.00f, 1.00f,   0.0f, 15,  90 },
    };

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    for (const auto& layer : layers) {
        const float lPhase = phase     * layer.phaseScale;
        const float lAmp   = amplitude * layer.ampScale;
        const float lFreq  = frequency * layer.freqScale;
        const float lBaseY = baseY     + layer.yOffset;
        const SDL_Color lCol = blendBrighter(waveColor, layer.colourLift);

        // Pre-compute surface Y per column once
        std::vector<int> surfaceY(screenW);
        int minSurf = screenH;
        for (int x = 0; x < screenW; ++x) {
            const float wave = evalWave(static_cast<float>(x), lPhase, lAmp, lFreq);
            surfaceY[x] = static_cast<int>(lBaseY + wave);
            minSurf = std::min(minSurf, surfaceY[x]);
        }

        // Iterate row by row — each row gets an exact interpolated colour,
        // no band boundaries, no seams
        const float totalDepth = static_cast<float>(screenH - std::max(minSurf, 0));

        for (int y = std::max(minSurf, 0); y < screenH; ++y) {
            // t=0 near the surface (bright), t=1 at the bottom (dark)
            const float t          = static_cast<float>(y - std::max(minSurf, 0)) / totalDepth;
            const float darkFactor = 1.0f - (t * 0.65f);

            const Uint8 r = static_cast<Uint8>(lCol.r * darkFactor);
            const Uint8 g = static_cast<Uint8>(lCol.g * darkFactor);
            const Uint8 b = static_cast<Uint8>(lCol.b * darkFactor);
            const Uint8 a = layer.alpha;

            SDL_SetRenderDrawColor(renderer, r, g, b, a);

            // Draw a horizontal run of pixels for all columns where this row
            // is below that column's wave surface
            int runStart = -1;
            for (int x = 0; x < screenW; ++x) {
                const bool below = (y >= surfaceY[x]);
                if (below && runStart < 0) {
                    runStart = x;
                } else if (!below && runStart >= 0) {
                    SDL_RenderDrawLine(renderer, runStart, y, x - 1, y);
                    runStart = -1;
                }
            }
            if (runStart >= 0) {
                SDL_RenderDrawLine(renderer, runStart, y, screenW - 1, y);
            }
        }
    }
}

void RenderBackdrop::drawStars(
    SDL_Renderer* renderer,
    int screenW, int screenH,
    float phase,
    float skyFraction,
    SDL_Color starColor,
    int numStars
) {
    if (!renderer || screenW <= 0 || screenH <= 0) return;

    static std::vector<StarParticle> stars;
    if (static_cast<int>(stars.size()) != numStars) {
        stars.clear();
        stars.reserve(numStars);

        uint32_t seed = 0xC0FFEE42u;
        auto rng = [&]() -> float {
            seed = seed * 1664525u + 1013904223u;
            return static_cast<float>(seed >> 8) / 16777215.0f;
        };

        for (int i = 0; i < numStars; ++i) {
            StarParticle s;
            s.xFrac       = 0.04f + rng() * 0.92f;
            // Power curve < 1 pulls values toward 0 (top of sky).
            // Raw uniform sample raised to 0.35 heavily clusters near yFrac=0
            // and becomes very sparse near yFrac=1 (the wave surface).
            const float raw = rng();
            s.yFrac = std::pow(raw, 1.5f);  // >1 pulls toward 0 (top of sky)
            s.phaseOffset = rng() * 6.2832f;
            s.rotSpeed    = 0.25f + rng() * 0.55f;
            s.size = 8.0f + rng() * 10.0f;
            stars.push_back(s);
        }
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    const float skyH = screenH * skyFraction;

    for (const auto& star : stars) {
        const float raw   = std::sin(phase * 1.3f + star.phaseOffset);
        const float glint = std::pow(std::max(0.0f, raw), 8.0f);
        if (glint < 0.01f) continue;

        const float cx     = star.xFrac * screenW;
        const float cy     = star.yFrac * skyH;
        const float angle  = phase * star.rotSpeed + star.phaseOffset;
        const float armLen = star.size * glint;

        constexpr int kSteps = 12;
        for (int arm = 0; arm < 4; ++arm) {
            const float a      = angle + arm * (3.14159265f * 0.5f);
            const float dxFull = std::cos(a) * armLen;
            const float dyFull = std::sin(a) * armLen;

            for (int step = 0; step < kSteps; ++step) {
                const float t0    = static_cast<float>(step)     / kSteps;
                const float t1    = static_cast<float>(step + 1) / kSteps;
                const float aFrac = (1.0f - t0) * (1.0f - t0);
                const Uint8 alpha = static_cast<Uint8>(glint * aFrac * starColor.a);
                if (alpha == 0) continue;

                SDL_SetRenderDrawColor(renderer,
                    starColor.r, starColor.g, starColor.b, alpha);
                SDL_RenderDrawLine(renderer,
                    static_cast<int>(cx + dxFull * t0),
                    static_cast<int>(cy + dyFull * t0),
                    static_cast<int>(cx + dxFull * t1),
                    static_cast<int>(cy + dyFull * t1));
            }
        }
    }
}
#ifndef FATIGUE_ANIMATION_HPP
#define FATIGUE_ANIMATION_HPP

#include "AnimationInterface.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

// Plays on the (now-empty) deck rect:
//   - Fire particles spawned along the card-back outline that rise & fade
//   - A "-N" label that floats upward and fades to transparent
//
// Call render(renderer) from your RenderPlaying pass each frame while the
// animation is active.
class FatigueAnimation : public AnimationInterface {
public:
    // deckRect     – the SDL_Rect of the self-deck zone (from computeSelfDeckRect)
    // fatigueDamage– raw positive damage value; displayed as "-N"
    // durationMs   – total lifetime of the whole effect
    // font         – non-owning; used for the floating damage label (may be null)
    FatigueAnimation(const SDL_Rect& deckRect,
                     int            fatigueDamage,
                     Uint32         durationMs,
                     TTF_Font*      font = nullptr);

    void start()              override;
    void update(float dt)     override;
    bool isFinished()   const override;
    bool isBlocking() const override { return false; }

    // Call once per frame inside your render pass while !isFinished().
    void render(SDL_Renderer* renderer) const;

private:
    // ── particle ──────────────────────────────────────────────────────────────
    struct Particle {
        float x, y;         // world position
        float vx, vy;       // velocity (px/s)
        float life;         // 1.0 = just born, 0.0 = dead
        float maxLife;      // seconds this particle lives
        float radius;       // render radius in px
    };

    // ── floating damage label ─────────────────────────────────────────────────
    struct FloatLabel {
        float x, y;         // current position
        float alpha;        // 0–255
    };

    // ── data ──────────────────────────────────────────────────────────────────
    SDL_Rect  deckRect;
    int       fatigueDamage;
    float     durationSeconds;
    float     elapsedSeconds{0.f};
    bool      finished{false};
    TTF_Font* font{nullptr};    // non-owning

    std::string          damageStr;
    std::vector<Particle> particles;
    FloatLabel            label{};
    float                 spawnAccum{0.f};

    mutable std::mt19937 rng;   // mutable so render() can call it via helpers

    // ── tunables ──────────────────────────────────────────────────────────────
    static constexpr float SPAWN_RATE_PER_SEC = 85.f;   // particles / second
    static constexpr float LABEL_RISE_PX_S    = 55.f;   // label rise speed
    static constexpr float LABEL_FADE_START   = 0.45f;  // fraction of duration before fading

    // Corner radius used when tracing the card-back outline.
    // Matches RenderCard::drawCardBack  (cardRect.w / 7, clamped to MIN_RADIUS).
    int cornerRadius() const;

    // Emit one new particle at a random point on the card-back perimeter.
    void spawnParticle();

    // Return a random point on the rounded-rect perimeter defined by r_.
    void samplePerimeterPoint(int r, float& outX, float& outY) const;
};

#endif // FATIGUE_ANIMATION_HPP
#ifndef FATIGUE_ANIMATION_HPP
#define FATIGUE_ANIMATION_HPP

#include "AnimationInterface.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

class FatigueAnimation : public AnimationInterface {
public:
    FatigueAnimation(const SDL_Rect& deckRect,
                     int            fatigueDamage,
                     Uint32         durationMs,
                     TTF_Font*      font = nullptr);

    ~FatigueAnimation();

    void start()             override;
    void update(float dt)    override;
    bool isFinished()  const override;
    bool isBlocking()  const override { return false; }

    void render(SDL_Renderer* renderer) const;

private:
    struct Particle {
        float x, y;
        float vx, vy;
        float life;
        float maxLife;
        float radius;
    };

    struct FloatLabel {
        float x, y;
        float alpha;
    };

    SDL_Rect  deckRect;
    int       fatigueDamage;
    float     durationSeconds;
    float     elapsedSeconds{0.f};
    bool      finished{false};
    TTF_Font* font{nullptr};

    std::string           damageStr;
    std::vector<Particle> particles;
    FloatLabel            label{};
    float                 spawnAccum{0.f};

    // Cached label texture — built once on first render(), reused every frame
    mutable SDL_Texture* labelTexture{nullptr};
    mutable int          labelTexW{0};
    mutable int          labelTexH{0};

    mutable std::mt19937 rng;

    // Cached distributions — constructed once, reused every spawn
    mutable std::uniform_real_distribution<float> perimDist;  // set in constructor
    mutable std::uniform_real_distribution<float> vxDist{-18.f, 18.f};
    mutable std::uniform_real_distribution<float> vyDist{-55.f, -25.f};
    mutable std::uniform_real_distribution<float> lifeDist{0.35f, 0.75f};
    mutable std::uniform_real_distribution<float> sizeDist{2.5f, 4.5f};

    static constexpr float SPAWN_RATE_PER_SEC = 85.f;
    static constexpr float LABEL_RISE_PX_S    = 55.f;
    static constexpr float LABEL_FADE_START   = 0.45f;

    int  cornerRadius() const;
    void spawnParticle();
    void samplePerimeterPoint(int r, float& outX, float& outY) const;
    void destroyLabelTexture() const;
};

#endif 
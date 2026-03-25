#ifndef SUMMON_ANIMATION_HPP
#define SUMMON_ANIMATION_HPP

#include "AnimationInterface.hpp"
#include <SDL2/SDL.h>

class SummonAnimation : public AnimationInterface {
    SDL_Rect rect{0, 0, 0, 0};
    float durationSeconds{0.0F};
    float elapsedSeconds{0.0F};
    bool finished{false};

public:
    SummonAnimation(const SDL_Rect& cardRect, Uint32 durationMs = 600U);

    void start()          override;
    void update(float dt) override;
    bool isFinished()     const override;

    void draw(SDL_Renderer* renderer) const;
    float getProgress() const;
};

#endif
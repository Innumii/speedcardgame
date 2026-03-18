#include "animation/DeathAnimation.hpp"

#include <algorithm>

DeathAnimation::DeathAnimation(int lane,
                               bool isSelfPlayer,
                               const std::vector<SDL_Rect>& selfSlots,
                               const std::vector<SDL_Rect>& opponentSlots,
                               Uint32 durationMs)
    : durationSeconds(static_cast<float>(std::max<Uint32>(durationMs, 1U)) / 1000.0F) {
    if (lane < 0) {
        finished = true;
        return;
    }

    const std::vector<SDL_Rect>& slots = isSelfPlayer ? selfSlots : opponentSlots;
    const std::size_t laneIndex = static_cast<std::size_t>(lane);
    if (laneIndex >= slots.size()) {
        finished = true;
        return;
    }

    cardRect = slots[laneIndex];
    canAnimate = true;
}

void DeathAnimation::start() {
    elapsedSeconds = 0.0F;
    finished = !canAnimate;
}

void DeathAnimation::update(float dt) {
    if (finished) {
        return;
    }

    elapsedSeconds += std::max(dt, 0.0F);
    const float t = std::clamp(elapsedSeconds / durationSeconds, 0.0F, 1.0F);
    finished = (t >= 1.0F);
}

bool DeathAnimation::isFinished() const {
    return finished;
}

SDL_Rect DeathAnimation::getRect() const {
    return cardRect;
}

Uint8 DeathAnimation::getAlpha() const {
    if (finished || durationSeconds <= 0.0F) {
        return static_cast<Uint8>(0);
    }

    const float t = std::clamp(elapsedSeconds / durationSeconds, 0.0F, 1.0F);
    const float alpha = 220.0F * (1.0F - t);
    return static_cast<Uint8>(alpha);
}

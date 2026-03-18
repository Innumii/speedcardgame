#include "animation/AttackAnimation.hpp"

#include <algorithm>
#include <cmath>

namespace {
struct PointF {
    float x{0.0F};
    float y{0.0F};
};

PointF rectCenter(const SDL_Rect& rect) {
    return PointF{
        static_cast<float>(rect.x) + static_cast<float>(rect.w) * 0.5F,
        static_cast<float>(rect.y) + static_cast<float>(rect.h) * 0.5F
    };
}

PointF quadraticBezier(const PointF& p0, const PointF& p1, const PointF& p2, float t) {
    const float u = 1.0F - t;
    const float tt = t * t;
    const float uu = u * u;

    return PointF{
        uu * p0.x + 2.0F * u * t * p1.x + tt * p2.x,
        uu * p0.y + 2.0F * u * t * p1.y + tt * p2.y
    };
}

float smoothstep(float t) {
    return t * t * (3.0F - 2.0F * t);
}

bool tryResolveLaneRect(const std::vector<SDL_Rect>& slots, int lane, SDL_Rect& outRect) {
    if (lane < 0) {
        return false;
    }

    const std::size_t laneIndex = static_cast<std::size_t>(lane);
    if (laneIndex >= slots.size()) {
        return false;
    }

    outRect = slots[laneIndex];
    return true;
}
}

AttackAnimation::AttackAnimation(int lane,
                                 bool isSelfPlayer,
                                 const std::vector<SDL_Rect>& selfSlots,
                                 const std::vector<SDL_Rect>& opponentSlots,
                                 Uint32 durationMs,
                                 const SDL_Rect* explicitTargetRect)
        : lane(lane),
            selfPlayer(isSelfPlayer),
            durationSeconds(static_cast<float>(std::max<Uint32>(durationMs, 1U)) / 1000.0F) {
    const std::vector<SDL_Rect>& sourceSlots = isSelfPlayer ? selfSlots : opponentSlots;
    if (!tryResolveLaneRect(sourceSlots, lane, startRect)) {
        finished = true;
        return;
    }

    if (explicitTargetRect) {
        targetRect = *explicitTargetRect;
        canAnimate = true;
        return;
    }

    const std::vector<SDL_Rect>& targetSlots = isSelfPlayer ? opponentSlots : selfSlots;
    if (!tryResolveLaneRect(targetSlots, lane, targetRect)) {
        finished = true;
        return;
    }

    // For lane-vs-lane combat, both creatures should converge to the same midpoint.
    const PointF startCenter = rectCenter(startRect);
    const PointF targetCenter = rectCenter(targetRect);
    const PointF midpoint{
        (startCenter.x + targetCenter.x) * 0.5F,
        (startCenter.y + targetCenter.y) * 0.5F
    };

    targetRect.w = startRect.w;
    targetRect.h = startRect.h;
    targetRect.x = static_cast<int>(std::lround(midpoint.x - static_cast<float>(targetRect.w) * 0.5F));
    targetRect.y = static_cast<int>(std::lround(midpoint.y - static_cast<float>(targetRect.h) * 0.5F));

    canAnimate = true;
}

void AttackAnimation::start() {
    elapsedSeconds = 0.0F;
    finished = !canAnimate;
    currentRect = startRect;
}

void AttackAnimation::update(float dt) {
    if (finished) {
        return;
    }

    elapsedSeconds += std::max(dt, 0.0F);
    const float t = std::clamp(elapsedSeconds / durationSeconds, 0.0F, 1.0F);

    // Move towards the target during the first half, then return.
    const float phase = (t <= 0.5F) ? (t / 0.5F) : ((1.0F - t) / 0.5F);
    const float curvedPhase = smoothstep(phase);

    const PointF startCenter = rectCenter(startRect);
    const PointF targetCenter = rectCenter(targetRect);

    const float dx = targetCenter.x - startCenter.x;
    const float dy = targetCenter.y - startCenter.y;
    const float distance = std::sqrt(dx * dx + dy * dy);
    const float arcHeight = std::clamp(distance * 0.18F, 40.0F, 120.0F);

    const PointF control{
        (startCenter.x + targetCenter.x) * 0.5F,
        (startCenter.y + targetCenter.y) * 0.5F - arcHeight
    };

    const PointF bezierPos = quadraticBezier(startCenter, control, targetCenter, curvedPhase);

    // Keep rendered attacker dimensions fixed to the source card size.
    currentRect.w = startRect.w;
    currentRect.h = startRect.h;
    currentRect.x = static_cast<int>(std::lround(bezierPos.x - static_cast<float>(currentRect.w) * 0.5F));
    currentRect.y = static_cast<int>(std::lround(bezierPos.y - static_cast<float>(currentRect.h) * 0.5F));

    finished = (t >= 1.0F);
}

bool AttackAnimation::isFinished() const {
    return finished;
}

SDL_Rect AttackAnimation::getCurrentRect() const {
    return currentRect;
}

Uint8 AttackAnimation::getAlpha() const {
    if (finished || durationSeconds <= 0.0F) {
        return static_cast<Uint8>(0);
    }

    const float t = std::clamp(elapsedSeconds / durationSeconds, 0.0F, 1.0F);
    const float pulse = std::sin(t * 3.1415926F);
    const float alpha = 90.0F + 110.0F * std::max(pulse, 0.0F);
    return static_cast<Uint8>(alpha);
}

int AttackAnimation::getLane() const {
    return lane;
}

bool AttackAnimation::isSelfPlayer() const {
    return selfPlayer;
}

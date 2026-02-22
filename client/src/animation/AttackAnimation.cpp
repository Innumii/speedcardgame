#include "animation/AttackAnimation.hpp"

#include <algorithm>

namespace {
    float easeInOutQuad(float t) {
        return t < 0.5f ? 2.0f * t * t : 1.0f - ((-2.0f * t + 2.0f) * (-2.0f * t + 2.0f)) / 2.0f;
    }

    int lerpInt(int from, int to, float t) {
        return static_cast<int>(from + (to - from) * t);
    }
}

void AttackAnimation::start(const SDL_Rect& from, const SDL_Rect& to, Uint32 now, Uint32 duration) {
    fromRect = from;
    toRect = to;
    currentRect = from;
    startTick = now;
    durationMs = duration > 0 ? duration : 1;
    active = true;
}

void AttackAnimation::update(Uint32 now) {
    if (!active) return;

    float t = static_cast<float>(now - startTick) / static_cast<float>(durationMs);
    t = std::max(0.0f, std::min(t, 1.0f));
    const float eased = easeInOutQuad(t);

    currentRect.x = lerpInt(fromRect.x, toRect.x, eased);
    currentRect.y = lerpInt(fromRect.y, toRect.y, eased);
    currentRect.w = lerpInt(fromRect.w, toRect.w, eased);
    currentRect.h = lerpInt(fromRect.h, toRect.h, eased);

    if (t >= 1.0f) {
        currentRect = toRect;
        active = false;
    }
}

bool AttackAnimation::isActive() const {
    return active;
}

const SDL_Rect& AttackAnimation::getCurrentRect() const {
    return currentRect;
}

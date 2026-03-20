#include "animation/DrawCardAnimation.hpp"

#include <algorithm>

namespace {
int lerpInt(int a, int b, float t) {
    return static_cast<int>(a + (b - a) * t);
}
}

DrawCardAnimation::DrawCardAnimation(const SDL_Rect& fromRect, const SDL_Rect& toRect,
                                     std::size_t handIndexValue, Uint32 durationMs)
    : startRect(fromRect),
      endRect(toRect),
      currentRect(fromRect),
      handIndex(handIndexValue),
      durationSeconds(static_cast<float>(std::max<Uint32>(durationMs, 1U)) / 1000.0F) {}

void DrawCardAnimation::start() {
    elapsedSeconds = 0.0F;
    finished = false;
    currentRect = startRect;
}

void DrawCardAnimation::update(float dt) {
    if (finished) {
        return;
    }

    elapsedSeconds += std::max(dt, 0.0F);
    const float t = std::clamp(elapsedSeconds / durationSeconds, 0.0F, 1.0F);

    currentRect.x = lerpInt(startRect.x, endRect.x, t);
    currentRect.y = lerpInt(startRect.y, endRect.y, t);
    currentRect.w = lerpInt(startRect.w, endRect.w, t);
    currentRect.h = lerpInt(startRect.h, endRect.h, t);

    finished = (t >= 1.0F);
}

bool DrawCardAnimation::isFinished() const {
    return finished;
}

SDL_Rect DrawCardAnimation::getCurrentRect() const {
    return currentRect;
}

std::size_t DrawCardAnimation::getHandIndex() const {
    return handIndex;
}

void DrawCardAnimation::updateDestination(SDL_Rect newDest) { 
    endRect = newDest; 
}

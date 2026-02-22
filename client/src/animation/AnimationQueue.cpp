#include "animation/animationQueue.hpp"

void AnimationQueue::clear() {
    drawCardQueue.clear();
    activeDrawCard = DrawCardAnimation{};
    activeDrawCardHandIndex = InvalidIndex;
    activeAttack = AttackAnimation{};
    activeAttackLane = InvalidIndex;
}

void AnimationQueue::enqueueDrawCard(const SDL_Rect& from, const SDL_Rect& to, std::size_t handIndex, Uint32 durationMs) {
    drawCardQueue.push_back(DrawCardRequest{from, to, handIndex, durationMs});
}

void AnimationQueue::update(Uint32 now) {
    if (!activeDrawCard.isActive() && !drawCardQueue.empty()) {
        startNext(now);
    }

    activeDrawCard.update(now);

    if (!activeDrawCard.isActive()) {
        activeDrawCardHandIndex = InvalidIndex;

        if (!drawCardQueue.empty()) {
            startNext(now);
            activeDrawCard.update(now);
        }
    }

    activeAttack.update(now);
    if (!activeAttack.isActive()) {
        activeAttackLane = InvalidIndex;
    }
}

bool AnimationQueue::hasActiveDrawCard() const {
    return activeDrawCard.isActive() && activeDrawCardHandIndex != InvalidIndex;
}

const SDL_Rect& AnimationQueue::getActiveDrawCardRect() const {
    return activeDrawCard.getCurrentRect();
}

std::size_t AnimationQueue::getActiveDrawCardHandIndex() const {
    return activeDrawCardHandIndex;
}

void AnimationQueue::startAttack(const SDL_Rect& from, const SDL_Rect& to, std::size_t attackerLane, Uint32 durationMs) {
    activeAttack.start(from, to, SDL_GetTicks(), durationMs);
    activeAttackLane = attackerLane;
}

bool AnimationQueue::hasActiveAttack() const {
    return activeAttack.isActive() && activeAttackLane != InvalidIndex;
}

const SDL_Rect& AnimationQueue::getActiveAttackRect() const {
    return activeAttack.getCurrentRect();
}

std::size_t AnimationQueue::getActiveAttackLane() const {
    return activeAttackLane;
}

void AnimationQueue::startNext(Uint32 now) {
    if (drawCardQueue.empty()) {
        return;
    }

    const DrawCardRequest request = drawCardQueue.front();
    drawCardQueue.pop_front();

    activeDrawCard.start(request.fromRect, request.toRect, now, request.durationMs);
    activeDrawCardHandIndex = request.handIndex;
}

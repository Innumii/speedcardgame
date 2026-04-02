#include "animation/AnimationQueue.hpp"
#include "animation/DrawCardAnimation.hpp"

#include <algorithm>

void AnimationQueue::startNextAnimation() {
    const Uint32 now = SDL_GetTicks();
    if (delayUntil > 0 && now < delayUntil) return;
    delayUntil = 0;

    while (!animationQueue.empty()) {
        auto next = animationQueue.front();
        animationQueue.pop_front();
        if (next) {
            next->start();
            activeAnimations.push_back(std::move(next));
            if (animationGapMs > 0) {
                delayUntil = SDL_GetTicks() + animationGapMs;
                break;
            }
        }
    }
}

void AnimationQueue::enqueue(std::shared_ptr<AnimationInterface> animation) {
    if (!animation) return;
    animationQueue.push_back(std::move(animation));
}

void AnimationQueue::enqueueGroup(const std::vector<std::shared_ptr<AnimationInterface>>& animations) {
    auto group = std::make_shared<AnimationGroup>();
    for (const auto& animation : animations) {
        group->add(animation);
    }
    if (!group->getAnimations().empty()) {
        enqueue(group);
    }
}

void AnimationQueue::update(Uint32 nowTick) {
    if (previousTick == 0) previousTick = nowTick;

    const Uint32 deltaTick = (nowTick >= previousTick) ? (nowTick - previousTick) : 0;
    previousTick = nowTick;
    const float dtSeconds = static_cast<float>(deltaTick) / 1000.0F;

    // Start all newly queued animations
    startNextAnimation();

    // Update all active animations
    for (auto& anim : activeAnimations) {
        if (anim) anim->update(std::max(dtSeconds, 0.0F));
    }

    // Remove finished ones
    activeAnimations.erase(
        std::remove_if(activeAnimations.begin(), activeAnimations.end(),
            [](const std::shared_ptr<AnimationInterface>& a) {
                return !a || a->isFinished();
            }),
        activeAnimations.end()
    );
}

bool AnimationQueue::hasActiveAnimation() const {
    return std::any_of(activeAnimations.begin(), activeAnimations.end(),
        [](const auto& anim) {
            return anim && !anim->isFinished() && anim->isBlocking();
        });
}

std::shared_ptr<const AnimationInterface> AnimationQueue::getActiveAnimation() const {
    return activeAnimations.empty() ? nullptr : activeAnimations.front();
}

const std::vector<std::shared_ptr<AnimationInterface>>& AnimationQueue::getActiveAnimations() const {
    return activeAnimations;
}

void AnimationQueue::clear() {
    animationQueue.clear();
    activeAnimations.clear();
    previousTick = 0;
    delayUntil = 0;
}

bool AnimationQueue::hasPendingDrawForIndex(std::size_t index) const {
    for (const auto& anim : animationQueue) {
        auto draw = std::dynamic_pointer_cast<const DrawCardAnimation>(anim);
        if (draw && draw->getHandIndex() == index) return true;
    }
    for (const auto& anim : activeAnimations) {
        auto draw = std::dynamic_pointer_cast<const DrawCardAnimation>(anim);
        if (draw && draw->getHandIndex() == index) return true;
    }
    return false;
}

void AnimationQueue::updateDrawDestinations(const std::vector<SDL_Rect>& layout) {
    for (auto& anim : animationQueue) {
        auto draw = std::dynamic_pointer_cast<DrawCardAnimation>(anim);
        if (draw && draw->getHandIndex() < layout.size())
            draw->updateDestination(layout[draw->getHandIndex()]);
    }
    for (auto& anim : activeAnimations) {
        auto draw = std::dynamic_pointer_cast<DrawCardAnimation>(anim);
        if (draw && draw->getHandIndex() < layout.size())
            draw->updateDestination(layout[draw->getHandIndex()]);
    }
}
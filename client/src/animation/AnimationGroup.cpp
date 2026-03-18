#include "animation/AnimationGroup.hpp"

void AnimationGroup::add(std::shared_ptr<AnimationInterface> animation) {
    if (animation) {
        animations.push_back(std::move(animation));
    }
}

const std::vector<std::shared_ptr<AnimationInterface>>& AnimationGroup::getAnimations() const {
    return animations;
}

void AnimationGroup::start() {
    for (const auto& animation : animations) {
        if (animation) {
            animation->start();
        }
    }
}

void AnimationGroup::update(float dt) {
    for (const auto& animation : animations) {
        if (animation && !animation->isFinished()) {
            animation->update(dt);
        }
    }
}

bool AnimationGroup::isFinished() const {
    if (animations.empty()) {
        return true;
    }

    for (const auto& animation : animations) {
        if (animation && !animation->isFinished()) {
            return false;
        }
    }

    return true;
}

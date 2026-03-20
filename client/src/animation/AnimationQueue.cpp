#include "animation/AnimationQueue.hpp"
#include "animation/DrawCardAnimation.hpp"

#include <algorithm>

void AnimationQueue::startNextAnimation() {
	if (activeAnimation || animationQueue.empty()) {
		return;
	}

	activeAnimation = animationQueue.front();
	animationQueue.pop_front();
	if (activeAnimation) {
		activeAnimation->start();
	}
}

void AnimationQueue::enqueue(std::shared_ptr<AnimationInterface> animation) {
	if (!animation) {
		return;
	}

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
void AnimationQueue::clear() {
	while (!animationQueue.empty()) {
		animationQueue.pop_front();
	}

	activeAnimation.reset();
	previousTick = 0;
}

void AnimationQueue::update(Uint32 nowTick) {
	if (previousTick == 0) {
		previousTick = nowTick;
	}

	startNextAnimation();
	if (!activeAnimation) {
		previousTick = nowTick;
		return;
	}

	const Uint32 deltaTick = (nowTick >= previousTick) ? (nowTick - previousTick) : 0;
	previousTick = nowTick;

	const float dtSeconds = static_cast<float>(deltaTick) / 1000.0F;
	activeAnimation->update(std::max(dtSeconds, 0.0F));

	if (activeAnimation->isFinished()) {
		activeAnimation.reset();
		startNextAnimation();
	}
}

bool AnimationQueue::hasActiveAnimation() const {
	return (activeAnimation != nullptr) || !animationQueue.empty();
}

std::shared_ptr<AnimationInterface> AnimationQueue::getActiveAnimation() {
	return activeAnimation;
}

std::shared_ptr<const AnimationInterface> AnimationQueue::getActiveAnimation() const {
	return activeAnimation;
}

bool AnimationQueue::hasPendingDrawForIndex(std::size_t index) const {
    for (const auto& anim : animationQueue) { // iterate all queued, not just active
        auto draw = std::dynamic_pointer_cast<const DrawCardAnimation>(anim);
        if (draw && draw->getHandIndex() == index) return true;
    }
    // also check active
    auto draw = std::dynamic_pointer_cast<const DrawCardAnimation>(activeAnimation);
    return draw && draw->getHandIndex() == index;
}

void AnimationQueue::updateDrawDestinations(const std::vector<SDL_Rect>& layout) {
    for (auto& anim : animationQueue) {
        auto draw = std::dynamic_pointer_cast<DrawCardAnimation>(anim);
        if (draw && draw->getHandIndex() < layout.size())
            draw->updateDestination(layout[draw->getHandIndex()]);
    }
    auto draw = std::dynamic_pointer_cast<DrawCardAnimation>(activeAnimation);
    if (draw && draw->getHandIndex() < layout.size())
        draw->updateDestination(layout[draw->getHandIndex()]);
}
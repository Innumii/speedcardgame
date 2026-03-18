#include "animation/AnimationQueue.hpp"

#include <algorithm>

void AnimationQueue::startNextAnimation() {
	if (activeAnimation || animationQueue.empty()) {
		return;
	}

	activeAnimation = animationQueue.front();
	animationQueue.pop();
	if (activeAnimation) {
		activeAnimation->start();
	}
}

void AnimationQueue::enqueue(std::shared_ptr<AnimationInterface> animation) {
	if (!animation) {
		return;
	}

	animationQueue.push(std::move(animation));
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
		animationQueue.pop();
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

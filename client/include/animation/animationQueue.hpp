#ifndef ANIMATION_QUEUE_HPP
#define ANIMATION_QUEUE_HPP

#include "AnimationInterface.hpp"
#include "AnimationGroup.hpp"
#include <SDL2/SDL_stdinc.h>
#include <memory>
#include <queue>
#include <vector>

class AnimationQueue {
	std::queue<std::shared_ptr<AnimationInterface>> animationQueue;
	std::shared_ptr<AnimationInterface> activeAnimation;
	Uint32 previousTick{0};

	void startNextAnimation();

public:
	void enqueue(std::shared_ptr<AnimationInterface> animation);
	void enqueueGroup(const std::vector<std::shared_ptr<AnimationInterface>>& animations);

	void clear();
	void update(Uint32 nowTick);
	bool hasActiveAnimation() const;
	std::shared_ptr<AnimationInterface> getActiveAnimation();
	std::shared_ptr<const AnimationInterface> getActiveAnimation() const;
};

#endif
#ifndef ANIMATION_QUEUE_HPP
#define ANIMATION_QUEUE_HPP

#include "AnimationInterface.hpp"
#include "AnimationGroup.hpp"
#include <SDL2/SDL_stdinc.h>
#include <memory>
#include <deque>
#include <vector>
#include <SDL2/SDL.h>

class AnimationQueue {
    std::deque<std::shared_ptr<AnimationInterface>> animationQueue;
	Uint32 delayUntil{0};
	Uint32 animationGapMs{100};

    std::vector<std::shared_ptr<AnimationInterface>> activeAnimations;
    Uint32 previousTick{0};

    void startNextAnimation();

public:
    void enqueue(std::shared_ptr<AnimationInterface> animation);
    void enqueueGroup(const std::vector<std::shared_ptr<AnimationInterface>>& animations);

    void clear();
    void update(Uint32 nowTick);
    bool hasActiveAnimation() const;
    std::shared_ptr<const AnimationInterface> getActiveAnimation() const;
	const std::vector<std::shared_ptr<AnimationInterface>>& getActiveAnimations() const;
    bool hasPendingDrawForIndex(std::size_t index) const;
    void updateDrawDestinations(const std::vector<SDL_Rect>& layout);

	void setAnimationGap(Uint32 ms) { animationGapMs = ms; }

};

#endif
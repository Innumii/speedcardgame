#ifndef ANIMATION_QUEUE_HPP
#define ANIMATION_QUEUE_HPP

#include <SDL2/SDL.h>

#include <cstddef>
#include <deque>

#include "animation/AttackAnimation.hpp"
#include "animation/DrawCardAnimation.hpp"

class AnimationQueue {
public:
	static constexpr std::size_t InvalidIndex = static_cast<std::size_t>(-1);

	void clear();

	void enqueueDrawCard(const SDL_Rect& from, const SDL_Rect& to, std::size_t handIndex, Uint32 durationMs);
	void update(Uint32 now);

	bool hasActiveDrawCard() const;
	const SDL_Rect& getActiveDrawCardRect() const;
	std::size_t getActiveDrawCardHandIndex() const;

	void startAttack(const SDL_Rect& from, const SDL_Rect& to, std::size_t attackerLane, Uint32 durationMs);
	bool hasActiveAttack() const;
	const SDL_Rect& getActiveAttackRect() const;
	std::size_t getActiveAttackLane() const;

private:
	struct DrawCardRequest {
		SDL_Rect fromRect;
		SDL_Rect toRect;
		std::size_t handIndex;
		Uint32 durationMs;
	};

	void startNext(Uint32 now);

	std::deque<DrawCardRequest> drawCardQueue;
	DrawCardAnimation activeDrawCard;
	std::size_t activeDrawCardHandIndex{InvalidIndex};

	AttackAnimation activeAttack;
	std::size_t activeAttackLane{InvalidIndex};
};

#endif

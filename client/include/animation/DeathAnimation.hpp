#ifndef DEATH_ANIMATION_HPP
#define DEATH_ANIMATION_HPP

#include "AnimationInterface.hpp"

#include <SDL2/SDL.h>
#include <vector>

class DeathAnimation : public AnimationInterface {
	SDL_Rect cardRect{0, 0, 0, 0};
	bool canAnimate{false};
	float durationSeconds{0.0F};
	float elapsedSeconds{0.0F};
	bool finished{false};

public:
	explicit DeathAnimation(int lane,
					   bool isSelfPlayer,
					   const std::vector<SDL_Rect>& selfSlots,
					   const std::vector<SDL_Rect>& opponentSlots,
					   Uint32 durationMs);

	void start() override;
	void update(float dt) override;
	bool isFinished() const override;

	SDL_Rect getRect() const;
	Uint8 getAlpha() const;
};

#endif

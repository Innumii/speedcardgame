#ifndef ATTACK_ANIMATION_HPP
#define ATTACK_ANIMATION_HPP

#include "AnimationInterface.hpp"

#include <SDL2/SDL.h>
#include <vector>

class AttackAnimation : public AnimationInterface {
	SDL_Rect startRect{0, 0, 0, 0};
	SDL_Rect targetRect{0, 0, 0, 0};
	SDL_Rect currentRect{0, 0, 0, 0};
	int lane{-1};
	bool selfPlayer{false};
	bool canAnimate{false};

	float durationSeconds{0.0F};
	float elapsedSeconds{0.0F};
	bool finished{false};

public:
	AttackAnimation(int lane,
					bool isSelfPlayer,
					const std::vector<SDL_Rect>& selfSlots,
					const std::vector<SDL_Rect>& opponentSlots,
					Uint32 durationMs,
					const SDL_Rect* explicitTargetRect = nullptr);

	void start() override;
	void update(float dt) override;
	bool isFinished() const override;

	SDL_Rect getCurrentRect() const;
	Uint8 getAlpha() const;
	int getLane() const;
	bool isSelfPlayer() const;
};

#endif

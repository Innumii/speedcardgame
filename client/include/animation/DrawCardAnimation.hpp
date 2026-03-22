#ifndef DRAW_CARD_ANIMATION_HPP
#define DRAW_CARD_ANIMATION_HPP

#include "AnimationInterface.hpp"

#include <SDL2/SDL.h>
#include <cstddef>

class DrawCardAnimation : public AnimationInterface {
	SDL_Rect startRect{0, 0, 0, 0};
	SDL_Rect endRect{0, 0, 0, 0};
	SDL_Rect currentRect{0, 0, 0, 0};

	std::size_t handIndex{0};
	float durationSeconds{0.0F};
	float elapsedSeconds{0.0F};
	bool finished{false};

public:
	DrawCardAnimation(const SDL_Rect& fromRect, const SDL_Rect& toRect,
					  std::size_t handIndex, Uint32 durationMs);

	void start() override;
	void update(float dt) override;
	bool isFinished() const override;

	SDL_Rect getCurrentRect() const;
	std::size_t getHandIndex() const;
	float getProgress() const;
	void updateDestination(SDL_Rect newDest);
};

#endif
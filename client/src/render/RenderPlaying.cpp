#include "render/RenderPLaying.hpp"

#include "core/Game.hpp"
#include "objects/Card.h"
#include "render/RenderBoard.hpp"
#include "render/RenderCard.hpp"
#include "render/RenderText.hpp"
#include "states/Playing.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <algorithm>
#include <string>

void RenderPlaying::render(Playing& playing, Game& game) {
	SDL_Renderer* renderer = game.getRenderer();
	if (!renderer) return;

	SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
	SDL_RenderClear(renderer);

	const Uint32 now = SDL_GetTicks();

	int screenW = 0, screenH = 0;
	if (SDL_GetRendererOutputSize(renderer, &screenW, &screenH) != 0) {
		screenW = 800;
		screenH = 600;
	}

	RenderText textRenderer;

	textRenderer.drawText(
		renderer,
		"Health: " + std::to_string(playing.player.health),
		playing.fontLarge.get(),
		SDL_Color{255, 255, 255, 255},
		20,
		20
	);

	const std::string manaText = "Mana: " + std::to_string(playing.player.mana);
	int manaW = 0, manaH = 0;
	if (playing.fontLarge) {
		TTF_SizeText(playing.fontLarge.get(), manaText.c_str(), &manaW, &manaH);
	}
	textRenderer.drawText(
		renderer,
		manaText,
		playing.fontLarge.get(),
		SDL_Color{255, 255, 255, 255},
		screenW - manaW - 20,
		20
	);

	playing.cardRects = playing.computeCardLayout(playing.player.hand.size(), screenW, screenH);
	playing.computeZones(screenW, screenH);

	int mouseX = 0, mouseY = 0;
	SDL_GetMouseState(&mouseX, &mouseY);
	const bool hoveringDiscard = playing.pointInRect(playing.discardZone, mouseX, mouseY);

	const bool draggingCard = playing.drag.active && playing.drag.index < playing.player.hand.size();

	std::size_t newHoverIndex = static_cast<std::size_t>(-1);
	if (!draggingCard) {
		for (std::size_t i = 0; i < playing.cardRects.size(); ++i) {
			if (playing.pointInRect(playing.cardRects[i], mouseX, mouseY)) {
				newHoverIndex = i;
				break;
			}
		}
	}

	if (newHoverIndex != playing.hoverIndex) {
		playing.hoverIndex = newHoverIndex;
		playing.hoverStartTick = now;
	}

	constexpr Uint32 hoverDelayMs = 1000;
	const bool showPreview =
		playing.hoverIndex != static_cast<std::size_t>(-1) &&
		playing.hoverIndex < playing.player.hand.size() &&
		now - playing.hoverStartTick >= hoverDelayMs;

	RenderBoard::drawPlayZones(renderer, textRenderer, playing.playSlots, playing.fontSmall.get());
	RenderBoard::drawDiscardZone(renderer, textRenderer, playing.discardZone, hoveringDiscard, playing.fontSmall.get());

	for (std::size_t i = 0; i < playing.player.hand.size(); ++i) {
		if (draggingCard && i == playing.drag.index) continue;
		if (i < playing.cardRects.size() && playing.player.hand[i]) {
			RenderCard::drawHandCard(renderer, textRenderer, *playing.player.hand[i], playing.cardRects[i], playing.fontSmall.get());
		}
	}

	RenderBoard::drawBoardState(renderer, textRenderer, playing.board, playing.playSlots, playing.fontSmall.get());

	if (draggingCard && playing.drag.index < playing.cardRects.size()) {
		SDL_Rect floating = playing.cardRects[playing.drag.index];
		floating.x = playing.drag.x;
		floating.y = playing.drag.y;
		RenderCard::drawHandCard(renderer, textRenderer, *playing.player.hand[playing.drag.index], floating, playing.fontSmall.get());
	}

	if (showPreview) {
		if (const auto& cardPtr = playing.player.hand[playing.hoverIndex]) {
			const int previewWidth = 260;
			const int previewHeight = 240;
			const int padding = 12;

			int handY = screenH - 165 - 30;
			if (!playing.cardRects.empty()) {
				handY = playing.cardRects.front().y;
			}

			int previewY = handY - previewHeight - 10;
			if (previewY < 20) previewY = 20;
			const int previewX = 20;

			SDL_Rect panel{previewX, previewY, previewWidth, previewHeight};
			RenderCard::drawPreview(renderer, textRenderer, *cardPtr, panel, playing.fontSmall.get(), playing.fontLarge.get());
		}
	}

	SDL_RenderPresent(renderer);
}
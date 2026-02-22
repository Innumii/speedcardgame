#include "render/RenderBoard.hpp"

#include "core/Board.hpp"
#include "objects/Card.h"
#include "render/RenderCard.hpp"
#include "render/RenderText.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <algorithm>
#include <vector>

void RenderBoard::drawOpponentPlayZones(SDL_Renderer* renderer, RenderText& textRenderer, const std::vector<SDL_Rect>& playSlots, TTF_Font* fontSmall) {
	if (!renderer || !fontSmall) return;
	if (playSlots.empty()) return;

	const int areaX = playSlots.front().x;
	const int opponentOffset = 200;
	const int areaY = playSlots.front().y - opponentOffset;

	const int labelY = std::max(10, areaY - 18);
	textRenderer.drawText(
		renderer,
		"Opponent Play Zone",
		fontSmall,
		SDL_Color{210, 230, 210, 255},
		areaX,
		labelY
	);

	for (const auto& slot : playSlots) {
		SDL_Rect adjustedSlot = slot;
		adjustedSlot.y -= opponentOffset;

		SDL_SetRenderDrawColor(renderer, 80, 60, 60, 190);
		SDL_RenderFillRect(renderer, &adjustedSlot);
		SDL_SetRenderDrawColor(renderer, 190, 140, 140, 255);
		SDL_RenderDrawRect(renderer, &adjustedSlot);
	}
}

void RenderBoard::drawPlayZones(SDL_Renderer* renderer, RenderText& textRenderer, const std::vector<SDL_Rect>& playSlots, TTF_Font* fontSmall) {
	if (!renderer || !fontSmall) return;
	if (playSlots.empty()) return;

	const int areaX = playSlots.front().x;
	const int areaY = playSlots.front().y;

	const int labelY = std::max(10, areaY - 18);
	textRenderer.drawText(
		renderer,
		"Play Zone",
		fontSmall,
		SDL_Color{210, 230, 210, 255},
		areaX,
		labelY
	);

	for (const auto& slot : playSlots) {
		SDL_SetRenderDrawColor(renderer, 60, 80, 60, 190);
		SDL_RenderFillRect(renderer, &slot);
		SDL_SetRenderDrawColor(renderer, 140, 190, 140, 255);
		SDL_RenderDrawRect(renderer, &slot);
	}
}

void RenderBoard::drawDiscardZone(SDL_Renderer* renderer, RenderText& textRenderer, const SDL_Rect& discardZone, bool hovering, TTF_Font* fontSmall) {
	if (!renderer || !fontSmall) return;

	SDL_SetRenderDrawColor(renderer, hovering ? 80 : 60, 80, 110, 255);
	SDL_RenderFillRect(renderer, &discardZone);
	SDL_SetRenderDrawColor(renderer, 190, 190, 220, 255);
	SDL_RenderDrawRect(renderer, &discardZone);

	textRenderer.drawText(
		renderer,
		"Discard Zone",
		fontSmall,
		SDL_Color{255, 255, 255, 255},
		discardZone.x + 10,
		discardZone.y + 10
	);

	textRenderer.drawWrappedText(
		renderer,
		"Drop cards here to gain mana",
		fontSmall,
		SDL_Color{220, 220, 220, 255},
		discardZone.x + 10,
		discardZone.y + 32,
		20
	);
}

void RenderBoard::drawBoardState(SDL_Renderer* renderer, RenderText& textRenderer, const Board& board, const std::vector<SDL_Rect>& playSlots, int localPlayerId, TTF_Font* fontSmall) {
	if (!renderer || !fontSmall) return;
	if (playSlots.empty()) return;

	for (int pid = 0; pid <= 1; ++pid) {
		for (std::size_t lane = 0; lane < board.getLaneCount(); ++lane) {
			const auto& optCard = board.getZone(static_cast<int>(lane), pid);
			if (optCard && *optCard) {
				const Card* card = optCard->get();
				if (!card) continue;

				SDL_Rect rect = playSlots[lane];
				if (pid != localPlayerId) {
					rect.y -= 200;
				}

				RenderCard::drawBoardCard(renderer, textRenderer, *card, rect, fontSmall);
			}
		}
	}
}
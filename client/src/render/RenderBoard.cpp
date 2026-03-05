#include "render/RenderBoard.hpp"

#include "core/Board.hpp"
#include "objects/Card.h"
#include "render/RenderCard.hpp"
#include "render/RenderText.hpp"
#include "utils/RenderUtil.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <algorithm>
#include <vector>

void RenderBoard::drawOpponentPlayZones(SDL_Renderer* renderer, RenderText& textRenderer, 
                                        const std::vector<SDL_Rect>& playSlots, TTF_Font* fontSmall) {
	if (!renderer || !fontSmall) return;
	if (playSlots.empty()) return;

	const int areaX = playSlots.front().x;
	const int opponentOffset = 210;
	const int areaY = playSlots.front().y - opponentOffset;

	const int labelY = std::max(10, areaY - 22);
	textRenderer.drawText(
		renderer,
		"Opponent Play Zone",
		fontSmall,
		SDL_Color{180, 160, 200, 255},
		areaX,
		labelY
	);

	for (const auto& slot : playSlots) {
		SDL_Rect adjustedSlot = slot;
		adjustedSlot.y -= opponentOffset;

		RenderUtil::fillRoundedRect(renderer, adjustedSlot, 8, SDL_Color{50, 40, 55, 180});
		RenderUtil::drawRoundedBorder(renderer, adjustedSlot, 8, SDL_Color{120, 90, 130, 255}, 2);
	}
}

void RenderBoard::drawPlayZones(SDL_Renderer* renderer, RenderText& textRenderer, 
                                const std::vector<SDL_Rect>& playSlots, TTF_Font* fontSmall) {
	if (!renderer || !fontSmall) return;
	if (playSlots.empty()) return;

	const int areaX = playSlots.front().x;
	const int areaY = playSlots.front().y;

	const int labelY = std::max(10, areaY - 22);
	textRenderer.drawText(
		renderer,
		"Play Zone",
		fontSmall,
		SDL_Color{180, 220, 180, 255},
		areaX,
		labelY
	);

	for (const auto& slot : playSlots) {
		RenderUtil::fillRoundedRect(renderer, slot, 8, SDL_Color{40, 60, 50, 180});
		RenderUtil::drawRoundedBorder(renderer, slot, 8, SDL_Color{100, 160, 120, 255}, 2);
	}
}

void RenderBoard::drawDiscardZone(SDL_Renderer* renderer, RenderText& textRenderer, 
                                  const SDL_Rect& discardZone, bool hovering, TTF_Font* fontSmall) {
	if (!renderer || !fontSmall) return;

	SDL_Color fill = hovering ? SDL_Color{60, 75, 100, 200} : SDL_Color{45, 60, 80, 180};
	SDL_Color border = hovering ? SDL_Color{120, 150, 200, 255} : SDL_Color{90, 120, 160, 255};

	RenderUtil::fillRoundedRect(renderer, discardZone, 10, fill);
	RenderUtil::drawRoundedBorder(renderer, discardZone, 10, border, hovering ? 3 : 2);

	const int textPadding = 6;
	const int maxTextWidth = discardZone.w - (textPadding * 2);

	// Title
	textRenderer.drawWrappedText(
		renderer,
		"Discard",
		fontSmall,
		SDL_Color{220, 230, 255, 255},
		discardZone.x + textPadding,
		discardZone.y + textPadding,
		maxTextWidth
	);

	// Description
	textRenderer.drawWrappedText(
		renderer,
		"Drop cards to gain mana",
		fontSmall,
		SDL_Color{180, 200, 230, 255},
		discardZone.x + textPadding,
		discardZone.y + 26,
		maxTextWidth
	);
}

void RenderBoard::drawBoardState(SDL_Renderer* renderer, RenderText& textRenderer, 
                                 const Board& board, const std::vector<SDL_Rect>& playSlots, 
                                 TTF_Font* fontTitle, TTF_Font* fontBody) {
	if (!renderer || !fontTitle || !fontBody) return;
	if (playSlots.empty()) return;

	const int opponentOffset = 230;

    // Loop over board indices: 0 = local, 1 = remote
    for (int boardIndex = 0; boardIndex <= 1; ++boardIndex) {
        for (std::size_t lane = 0; lane < board.getLaneCount(); ++lane) {
            const auto& optCard = board.getZone(static_cast<int>(lane), boardIndex);
            if (!optCard || !*optCard) continue;

            const Card* card = optCard->get();
            if (!card) continue;

				// Apply offset for remote player
				SDL_Rect rect = playSlots[lane];
				if (boardIndex == 1) {
					rect.y -= opponentOffset;
				}

				RenderCard::drawBoardCard(renderer, textRenderer, *card, rect, fontTitle, fontBody);
		}
	}
}
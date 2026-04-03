#include "render/RenderBoard.hpp"

#include "core/Board.hpp"
#include "objects/Card.h"
#include "render/RenderCard.hpp"
#include "render/RenderText.hpp"
#include "render/Theme.hpp"
#include "utils/RenderUtil.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <algorithm>
#include <vector>

void RenderBoard::drawOpponentPlayZones(SDL_Renderer* renderer, RenderText& textRenderer, 
                                        const std::vector<SDL_Rect>& opponentSlots, TTF_Font* fontSmall) {
	if (!renderer || !fontSmall) return;
	if (opponentSlots.empty()) return;

	const int areaX = opponentSlots.front().x;
	const int areaY = opponentSlots.front().y;

	const int labelY = std::max(Theme::Board::LABEL_MIN_Y, areaY - Theme::Board::LABEL_OFFSET_Y);
	textRenderer.drawText(
		renderer,
		"Opponent Play Zone",
		fontSmall,
		Theme::Board::OPPONENT_LABEL,
		areaX,
		labelY
	);

	for (const auto& slot : opponentSlots) {
		RenderUtil::fillRoundedRect(renderer, slot, Theme::Board::ZONE_CORNER_RADIUS, Theme::Board::OPPONENT_ZONE_FILL);
		RenderUtil::drawRoundedBorder(renderer, slot, Theme::Board::ZONE_CORNER_RADIUS, Theme::Board::OPPONENT_ZONE_BORDER, Theme::Board::ZONE_BORDER_THICKNESS);
	}
}

void RenderBoard::drawPlayZones(SDL_Renderer* renderer, RenderText& textRenderer, 
                                const std::vector<SDL_Rect>& playSlots, TTF_Font* fontSmall) {
	if (!renderer || !fontSmall) return;
	if (playSlots.empty()) return;

	const int areaX = playSlots.front().x;
	const int areaY = playSlots.front().y;

	const int labelY = std::max(Theme::Board::LABEL_MIN_Y, areaY - Theme::Board::LABEL_OFFSET_Y);
	textRenderer.drawText(
		renderer,
		"Play Zone",
		fontSmall,
		Theme::Board::PLAYER_LABEL,
		areaX,
		labelY
	);

	for (const auto& slot : playSlots) {
		RenderUtil::fillRoundedRect(renderer, slot, Theme::Board::ZONE_CORNER_RADIUS, Theme::Board::PLAYER_ZONE_FILL);
		RenderUtil::drawRoundedBorder(renderer, slot, Theme::Board::ZONE_CORNER_RADIUS, Theme::Board::PLAYER_ZONE_BORDER, Theme::Board::ZONE_BORDER_THICKNESS);
	}
}

void RenderBoard::drawDiscardZone(SDL_Renderer* renderer, RenderText& textRenderer, 
                                  const SDL_Rect& discardZone, bool hovering, TTF_Font* fontSmall) {
	if (!renderer || !fontSmall) return;

	SDL_Color fill = hovering ? Theme::Board::DISCARD_FILL_HOVER : Theme::Board::DISCARD_FILL;
	SDL_Color border = hovering ? Theme::Board::DISCARD_BORDER_HOVER : Theme::Board::DISCARD_BORDER;

	RenderUtil::fillRoundedRect(renderer, discardZone, Theme::Board::DISCARD_CORNER_RADIUS, fill);
	RenderUtil::drawRoundedBorder(renderer, discardZone, Theme::Board::DISCARD_CORNER_RADIUS, border,
	                             hovering ? Theme::Board::DISCARD_HOVER_BORDER_THICKNESS : Theme::Board::DISCARD_BORDER_THICKNESS);

	const int textPadding = Theme::Board::DISCARD_TEXT_PADDING;
	const int maxTextWidth = discardZone.w - (textPadding * 2);

	int yOffset = discardZone.y + textPadding;

	// Title
	textRenderer.drawWrappedText(
		renderer,
		"Discard",
		fontSmall,
		Theme::Board::DISCARD_DESCRIPTION_TEXT,
		discardZone.x + textPadding,
		discardZone.y + textPadding,
		maxTextWidth
	);

	// Description
	textRenderer.drawWrappedText(
		renderer,
		"Drop cards to gain mana",
		fontSmall,
		Theme::Board::DISCARD_DESCRIPTION_TEXT,
		discardZone.x + textPadding,
		discardZone.y + 26,
		maxTextWidth
	);
}

void RenderBoard::drawBoardState(SDL_Renderer* renderer, RenderText& textRenderer, 
                                 const Board& board, const std::vector<SDL_Rect>& playSlots,
                                 const std::vector<SDL_Rect>& opponentSlots,
								 TTF_Font* fontTitle, TTF_Font* fontBody,
								 const std::set<std::pair<int, int>>* skippedSlots) {
	if (!renderer || !fontTitle || !fontBody) return;
	if (playSlots.empty()) return;
	const std::size_t laneCount = std::min(playSlots.size(), static_cast<std::size_t>(board.getLaneCount()));

    // Loop over board indices: 0 = local, 1 = remote
    for (int boardIndex = 0; boardIndex <= 1; ++boardIndex) {
				for (std::size_t lane = 0; lane < laneCount; ++lane) {
			if (skippedSlots && skippedSlots->count({static_cast<int>(lane), boardIndex}) > 0) {
				continue;
			}

            const auto& optCard = board.getZone(static_cast<int>(lane), boardIndex);
            if (!optCard || !*optCard) continue;

            const Card* card = optCard->get();
            if (!card) continue;

				// Apply offset for remote player
				SDL_Rect rect = boardIndex == 1 ? opponentSlots[lane] : playSlots[lane];

				RenderCard::drawBoardCard(renderer, textRenderer, *card, rect, fontTitle, fontBody);
		}
	}
}
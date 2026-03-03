#include "render/RenderBoard.hpp"

#include "core/Board.hpp"
#include "objects/Card.h"
#include "render/RenderCard.hpp"
#include "render/RenderText.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <algorithm>
#include <vector>
#include <cmath>

namespace {
	// Helper to draw rounded rectangle
	void fillRoundedRect(SDL_Renderer* r, const SDL_Rect& rect, int radius, SDL_Color c) {
		radius = std::min(radius, std::min(rect.w, rect.h) / 2);
		SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
		SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
		
		SDL_Rect body  = {rect.x + radius, rect.y, rect.w - 2*radius, rect.h};
		SDL_Rect left  = {rect.x, rect.y + radius, radius, rect.h - 2*radius};
		SDL_Rect right = {rect.x + rect.w - radius, rect.y + radius, radius, rect.h - 2*radius};
		
		SDL_RenderFillRect(r, &body);
		SDL_RenderFillRect(r, &left);
		SDL_RenderFillRect(r, &right);
		
		for (int dy = -radius; dy <= radius; ++dy) {
			int dx = (int)std::sqrt((double)(radius*radius - dy*dy));
			SDL_RenderDrawLine(r, rect.x + radius - dx, rect.y + radius + dy,
			                      rect.x + radius, rect.y + radius + dy);
			SDL_RenderDrawLine(r, rect.x + rect.w - radius, rect.y + radius + dy,
			                      rect.x + rect.w - radius + dx, rect.y + radius + dy);
			SDL_RenderDrawLine(r, rect.x + radius - dx, rect.y + rect.h - radius + dy,
			                      rect.x + radius, rect.y + rect.h - radius + dy);
			SDL_RenderDrawLine(r, rect.x + rect.w - radius, rect.y + rect.h - radius + dy,
			                      rect.x + rect.w - radius + dx, rect.y + rect.h - radius + dy);
		}
	}

	void drawRoundedBorder(SDL_Renderer* r, const SDL_Rect& rect, int radius, SDL_Color c, int thickness) {
		radius = std::min(radius, std::min(rect.w, rect.h) / 2);
		thickness = std::min(thickness, radius);
		SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
		SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);

		const int outerR = radius;
		const int innerR = outerR - thickness;

		SDL_Rect top   = {rect.x + outerR, rect.y, rect.w - 2*outerR, thickness};
		SDL_Rect bot   = {rect.x + outerR, rect.y + rect.h - thickness, rect.w - 2*outerR, thickness};
		SDL_Rect left  = {rect.x, rect.y + outerR, thickness, rect.h - 2*outerR};
		SDL_Rect right = {rect.x + rect.w - thickness, rect.y + outerR, thickness, rect.h - 2*outerR};
		SDL_RenderFillRect(r, &top);
		SDL_RenderFillRect(r, &bot);
		SDL_RenderFillRect(r, &left);
		SDL_RenderFillRect(r, &right);

		for (int dy = 0; dy <= outerR; ++dy) {
			int outerDx = (int)std::sqrt(std::max(0.0, (double)(outerR*outerR - dy*dy)));
			int innerDx = (innerR > 0 && dy < innerR) 
				? (int)std::sqrt(std::max(0.0, (double)(innerR*innerR - dy*dy))) 
				: 0;

			int row_top = rect.y + outerR - dy;
			SDL_RenderDrawLine(r, rect.x + outerR - outerDx, row_top,
			                      rect.x + outerR - innerDx, row_top);
			SDL_RenderDrawLine(r, rect.x + rect.w - outerR + innerDx, row_top,
			                      rect.x + rect.w - outerR + outerDx, row_top);

			int row_bot = rect.y + rect.h - outerR + dy;
			SDL_RenderDrawLine(r, rect.x + outerR - outerDx, row_bot,
			                      rect.x + outerR - innerDx, row_bot);
			SDL_RenderDrawLine(r, rect.x + rect.w - outerR + innerDx, row_bot,
			                      rect.x + rect.w - outerR + outerDx, row_bot);
		}
	}
}

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

		fillRoundedRect(renderer, adjustedSlot, 8, SDL_Color{50, 40, 55, 180});
		drawRoundedBorder(renderer, adjustedSlot, 8, SDL_Color{120, 90, 130, 255}, 2);
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
		fillRoundedRect(renderer, slot, 8, SDL_Color{40, 60, 50, 180});
		drawRoundedBorder(renderer, slot, 8, SDL_Color{100, 160, 120, 255}, 2);
	}
}

void RenderBoard::drawDiscardZone(SDL_Renderer* renderer, RenderText& textRenderer, 
                                  const SDL_Rect& discardZone, bool hovering, TTF_Font* fontSmall) {
	if (!renderer || !fontSmall) return;

	SDL_Color fill = hovering ? SDL_Color{60, 75, 100, 200} : SDL_Color{45, 60, 80, 180};
	SDL_Color border = hovering ? SDL_Color{120, 150, 200, 255} : SDL_Color{90, 120, 160, 255};

	fillRoundedRect(renderer, discardZone, 10, fill);
	drawRoundedBorder(renderer, discardZone, 10, border, hovering ? 3 : 2);

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

	for (int pid = 0; pid <= 1; ++pid) {
		for (std::size_t lane = 0; lane < board.getLaneCount(); ++lane) {
			const auto& optCard = board.getZone(static_cast<int>(lane), pid);
			if (optCard && *optCard) {
				const Card* card = optCard->get();
				if (!card) continue;

				SDL_Rect rect = playSlots[lane];
				if (pid == 1) {
					rect.y -= opponentOffset;
				}

				RenderCard::drawBoardCard(renderer, textRenderer, *card, rect, fontTitle, fontBody);
			}
		}
	}
}
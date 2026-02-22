#include "render/RenderPLaying.hpp"

#include "core/Game.hpp"
#include "objects/Card.h"
#include "render/RenderBoard.hpp"
#include "render/RenderCard.hpp"
#include "render/RenderText.hpp"
#include "render/Theme.hpp"
#include "states/Playing.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <algorithm>
#include <string>
#include <cmath>

namespace {
	void drawOpponentDeckAndDiscard(SDL_Renderer* renderer, RenderText& textRenderer,
			const std::vector<SDL_Rect>& playSlots, const SDL_Rect& playerDiscardZone,
			std::size_t deckSize, int screenW, TTF_Font* fontSmall) {
		if (!renderer || !fontSmall) return;
		if (playSlots.empty()) return;

		const int opponentOffset = 200;
		const int gap = 18;
		const int margin = 10;

		const int slotHeight = playSlots.front().h;
		const int discardW = playerDiscardZone.w > 0 ? playerDiscardZone.w : 110;
		const int discardH = playerDiscardZone.h > 0 ? playerDiscardZone.h : 130;
		const int opponentZoneY = playSlots.front().y - opponentOffset;
		const int discardY = opponentZoneY + (slotHeight - discardH) / 2;

		const int leftEdge = playSlots.front().x - gap;
		const int rightEdge = playSlots.back().x + playSlots.back().w + gap;

		int discardX = leftEdge - discardW;
		if (discardX < margin) discardX = margin;

		int deckX = rightEdge;
		if (deckX + discardW > screenW - margin) {
			deckX = std::max(margin, screenW - margin - discardW);
		}

		SDL_Rect opponentDiscard{discardX, discardY, discardW, discardH};
		SDL_SetRenderDrawColor(renderer, 70, 60, 80, 255);
		SDL_RenderFillRect(renderer, &opponentDiscard);
		SDL_SetRenderDrawColor(renderer, 170, 150, 190, 255);
		SDL_RenderDrawRect(renderer, &opponentDiscard);
		textRenderer.drawText(
			renderer,
			"Opponent Discard",
			fontSmall,
			Theme::TEXT_PRIMARY,
			opponentDiscard.x + 6,
			opponentDiscard.y + 6
		);

		SDL_Rect deckBase{deckX, discardY, discardW, discardH};
		const int stackCount = static_cast<int>(std::min<std::size_t>(deckSize, 5));
		for (int i = 0; i < stackCount; ++i) {
			SDL_Rect card{deckBase.x + i * 4, deckBase.y - i * 2, deckBase.w, deckBase.h};
			RenderCard::drawCardBack(renderer, card);
		}
		textRenderer.drawText(
			renderer,
			"Deck: " + std::to_string(deckSize),
			fontSmall,
			Theme::TEXT_PRIMARY,
			deckBase.x + 6,
			deckBase.y + 6
		);
	}

	void drawSelfDeck(SDL_Renderer* renderer, RenderText& textRenderer,
			const std::vector<SDL_Rect>& playSlots, const SDL_Rect& playerDiscardZone,
			int deckSize, TTF_Font* fontSmall) {
		if (!renderer || !fontSmall) return;
		if (playSlots.empty()) return;

		const int gap = 18;
		const int margin = 10;
		const int deckW = playerDiscardZone.w > 0 ? playerDiscardZone.w : 110;
		const int deckH = playerDiscardZone.h > 0 ? playerDiscardZone.h : 130;
		const int deckY = playSlots.front().y + (playSlots.front().h - deckH) / 2;

		int deckX = playSlots.front().x - gap - deckW;
		if (deckX < margin) deckX = margin;

		SDL_Rect deckBase{deckX, deckY, deckW, deckH};
		const int stackCount = static_cast<int>(std::min<int>(deckSize, 5));
		for (int i = 0; i < stackCount; ++i) {
			SDL_Rect card{deckBase.x + i * 4, deckBase.y - i * 2, deckBase.w, deckBase.h};
			RenderCard::drawCardBack(renderer, card);
		}
		textRenderer.drawText(
			renderer,
			"Deck: " + std::to_string(deckSize),
			fontSmall,
			Theme::TEXT_PRIMARY,
			deckBase.x + 6,
			deckBase.y + 6
		);
	}

	void drawStatBox(SDL_Renderer* renderer, const SDL_Rect& box, SDL_Color fill, SDL_Color border, int radius) {
		// rounded rect helper
		auto fillCircle = [&](int cx, int cy, int r) {
			for (int dy = -r; dy <= r; dy++) {
				int dx = (int)sqrt((double)(r*r - dy*dy));
				SDL_RenderDrawLine(renderer, cx-dx, cy+dy, cx+dx, cy+dy);
			}
		};

		SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
		SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);
		
		SDL_Rect body  = {box.x + radius,        box.y,          box.w - 2*radius, box.h};
		SDL_Rect left  = {box.x,                 box.y + radius, radius,           box.h - 2*radius};
		SDL_Rect right = {box.x + box.w - radius, box.y + radius, radius,           box.h - 2*radius};
		
		SDL_RenderFillRect(renderer, &body);
		SDL_RenderFillRect(renderer, &left);
		SDL_RenderFillRect(renderer, &right);
		
		fillCircle(box.x + radius,          box.y + radius,          radius);
		fillCircle(box.x + box.w - radius,  box.y + radius,          radius);
		fillCircle(box.x + radius,          box.y + box.h - radius,  radius);
		fillCircle(box.x + box.w - radius,  box.y + box.h - radius,  radius);
		
		SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
		SDL_RenderDrawLine(renderer, box.x + radius,    box.y,          box.x + box.w - radius, box.y);
		SDL_RenderDrawLine(renderer, box.x + radius,    box.y + box.h,  box.x + box.w - radius, box.y + box.h);
		SDL_RenderDrawLine(renderer, box.x,             box.y + radius, box.x,                  box.y + box.h - radius);
		SDL_RenderDrawLine(renderer, box.x + box.w,     box.y + radius, box.x + box.w,          box.y + box.h - radius);
	}
}

void RenderPlaying::render(Playing& playing, Game& game) {
	SDL_Renderer* renderer = game.getRenderer();
	if (!renderer) return;

	// ── get fonts from Game ──────────────────────────────────────────
	const RenderText::FontSet& uiFonts    = game.getUIFonts();
	const RenderText::FontSet& titleFonts = game.getTitleFonts();

	// ── background ───────────────────────────────────────────────────
	SDL_SetRenderDrawColor(renderer, 28, 22, 45, 255);
	SDL_RenderClear(renderer);

	const Uint32 now = SDL_GetTicks();

	int screenW = 0, screenH = 0;
	if (SDL_GetRendererOutputSize(renderer, &screenW, &screenH) != 0) {
		screenW = 800;
		screenH = 600;
	}

	RenderText textRenderer;

	// ── player stats with boxes ──────────────────────────────────────
	const std::string healthText = "HEALTH: " + std::to_string(playing.player.health);
	const std::string manaText   = "MANA: " + std::to_string(playing.player.mana);
	
	int healthW = 0, healthH = 0, manaW = 0, manaH = 0;
	if (titleFonts.medium) {
		TTF_SizeText(titleFonts.medium, healthText.c_str(), &healthW, &healthH);
		TTF_SizeText(titleFonts.medium, manaText.c_str(),   &manaW,   &manaH);
	}

	// Health box (left side)
	SDL_Rect healthBox = {12, 12, healthW + 20, healthH + 16};
	drawStatBox(renderer, healthBox, 
	            SDL_Color{40, 20, 20, 200},   // dark red fill
	            SDL_Color{180, 60, 60, 255},  // red border
	            8);
	textRenderer.drawText(renderer, healthText, titleFonts.medium,
	                      SDL_Color{255, 220, 220, 255},  // bright red-white
	                      healthBox.x + 10, healthBox.y + 8);

	// Mana box (right side)
	SDL_Rect manaBox = {screenW - manaW - 32, 12, manaW + 20, manaH + 16};
	drawStatBox(renderer, manaBox,
	            SDL_Color{20, 30, 50, 200},   // dark blue fill
	            SDL_Color{60, 120, 200, 255}, // blue border
	            8);
	textRenderer.drawText(renderer, manaText, titleFonts.medium,
	                      SDL_Color{180, 220, 255, 255},  // bright cyan
	                      manaBox.x + 10, manaBox.y + 8);

	// ── opponent stats with boxes ────────────────────────────────────
	const int opponentStatsY = healthBox.y + healthBox.h + 12;
	const std::string opponentHealthText = "Opponent Health: " + std::to_string(game.getHealth(game.getPlayer(true)));
	const std::string opponentManaText   = "Opponent Mana: " + std::to_string(game.getMana(game.getPlayer(true)));
	
	int opponentHealthW = 0, opponentHealthH = 0, opponentManaW = 0, opponentManaH = 0;
	if (uiFonts.small) {
		TTF_SizeText(uiFonts.small, opponentHealthText.c_str(), &opponentHealthW, &opponentHealthH);
		TTF_SizeText(uiFonts.small, opponentManaText.c_str(),   &opponentManaW,   &opponentManaH);
	}

	// Opponent health box (left side)
	SDL_Rect opponentHealthBox = {12, opponentStatsY, opponentHealthW + 16, opponentHealthH + 12};
	drawStatBox(renderer, opponentHealthBox,
	            SDL_Color{30, 20, 20, 150},   // darker muted red
	            SDL_Color{120, 50, 50, 200},  // muted red border
	            6);
	textRenderer.drawText(renderer, opponentHealthText, uiFonts.small,
	                      SDL_Color{200, 140, 140, 255},  // muted red-white
	                      opponentHealthBox.x + 8, opponentHealthBox.y + 6);

	// Opponent mana box (right side)
	SDL_Rect opponentManaBox = {screenW - opponentManaW - 28, opponentStatsY, opponentManaW + 16, opponentManaH + 12};
	drawStatBox(renderer, opponentManaBox,
	            SDL_Color{20, 25, 35, 150},   // darker muted blue
	            SDL_Color{50, 80, 120, 200},  // muted blue border
	            6);
	textRenderer.drawText(renderer, opponentManaText, uiFonts.small,
	                      SDL_Color{140, 170, 200, 255},  // muted cyan
	                      opponentManaBox.x + 8, opponentManaBox.y + 6);

	// ── compute layouts ──────────────────────────────────────────────
	playing.cardRects = playing.computeCardLayout(playing.player.hand.size(), screenW, screenH);
	playing.computeZones(screenW, screenH);

	const std::size_t opponentHandSize = game.getHandSize(game.getPlayer(true));
	std::vector<SDL_Rect> opponentHandRects = playing.computeCardLayout(opponentHandSize, screenW, screenH);
	if (!opponentHandRects.empty()) {
		const int cardHeight = opponentHandRects.front().h;
		int topHandY = 20;
		if (!playing.playSlots.empty()) {
			const int opponentOffset = 200;
			topHandY = std::max(20, playing.playSlots.front().y - opponentOffset - cardHeight - 20);
		}
		for (auto& rect : opponentHandRects) {
			rect.y = topHandY;
		}
	}

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
		(playing.hoverIndex != static_cast<std::size_t>(-1) &&
		playing.hoverIndex < playing.player.hand.size() &&
		now - playing.hoverStartTick >= hoverDelayMs) || playing.previewLocked;

	// ── draw zones ───────────────────────────────────────────────────
	RenderBoard::drawOpponentPlayZones(renderer, textRenderer, playing.playSlots, uiFonts.small);
	drawOpponentDeckAndDiscard(
		renderer,
		textRenderer,
		playing.playSlots,
		playing.discardZone,
		game.getDeck(game.getPlayer(true)).size(),
		screenW,
		uiFonts.small
	);
	RenderBoard::drawPlayZones(renderer, textRenderer, playing.playSlots, uiFonts.small);
	RenderBoard::drawDiscardZone(renderer, textRenderer, playing.discardZone, hoveringDiscard, uiFonts.small);
	drawSelfDeck(
		renderer,
		textRenderer,
		playing.playSlots,
		playing.discardZone,
		playing.deck.size(),
		uiFonts.small
	);

	// ── draw cards ───────────────────────────────────────────────────
	for (const auto& rect : opponentHandRects) {
		RenderCard::drawCardBack(renderer, rect);
	}

	for (std::size_t i = 0; i < playing.player.hand.size(); ++i) {
		if (draggingCard && i == playing.drag.index) continue;
		if (i < playing.cardRects.size() && playing.player.hand[i]) {
			RenderCard::drawHandCard(renderer, textRenderer, *playing.player.hand[i], playing.cardRects[i], titleFonts.small, uiFonts.small);
		}
	}

	RenderBoard::drawBoardState(renderer, textRenderer, playing.board, playing.playSlots, titleFonts.small, uiFonts.small);

	if (draggingCard && playing.drag.index < playing.cardRects.size()) {
		SDL_Rect floating = playing.cardRects[playing.drag.index];
		floating.x = playing.drag.x;
		floating.y = playing.drag.y;
		RenderCard::drawHandCard(renderer, textRenderer, *playing.player.hand[playing.drag.index], floating, titleFonts.small, uiFonts.small);
	}

// ── card preview (centered magnified) ────────────────────────────
if (showPreview && playing.hoverIndex < playing.player.hand.size()) {
    if (const auto& cardPtr = playing.player.hand[playing.hoverIndex]) {
        // ── full-screen darkening overlay ────────────────────────
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 160);
        SDL_RenderFillRect(renderer, nullptr);

        // ── larger centered card preview ─────────────────────────
        const int previewW = std::min(360, screenW - 80);  // changed from 280 to 360
        const int previewH = static_cast<int>(previewW * 1.5f);
        
        const int previewX = (screenW - previewW) / 2;
        const int previewY = (screenH - previewH) / 2;

        SDL_Rect panel{previewX, previewY, previewW, previewH};
		RenderCard::drawPreview(renderer, textRenderer, *cardPtr, panel, 
								uiFonts.large, titleFonts.medium, playing.previewScrollOffset);
    }
}

	SDL_RenderPresent(renderer);
}
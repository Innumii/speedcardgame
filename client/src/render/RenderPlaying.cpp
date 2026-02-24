#include "render/RenderPLaying.hpp"

#include "core/Game.hpp"
#include "objects/Card.h"
#include "render/RenderBoard.hpp"
#include "render/RenderButton.hpp"
#include "render/RenderCard.hpp"
#include "render/RenderText.hpp"
#include "render/Theme.hpp"
#include "states/Playing.hpp"
#include "states/DeckBuilding.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <algorithm>
#include <string>

namespace {
	void drawOpponentDeckAndDiscard(SDL_Renderer* renderer, RenderText& textRenderer, const std::vector<SDL_Rect>& playSlots,
			const SDL_Rect& playerDiscardZone, std::size_t deckSize, int screenW, TTF_Font* fontSmall) {
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

		int deckX = leftEdge - discardW;
		if (deckX < margin) deckX = margin;

		int discardX = rightEdge;
		if (discardX + discardW > screenW - margin) {
			discardX = std::max(margin, screenW - margin - discardW);
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
			SDL_Color{230, 230, 230, 255},
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
			SDL_Color{230, 230, 230, 255},
			deckBase.x + 6,
			deckBase.y + 6
		);
	}

	void drawSelfDeck(SDL_Renderer* renderer, RenderText& textRenderer, const std::vector<SDL_Rect>& playSlots,
			const SDL_Rect& playerDiscardZone, int deckSize, int screenW, TTF_Font* fontSmall) {
		if (!renderer || !fontSmall) return;
		if (playSlots.empty()) return;

		const int gap = 18;
		const int margin = 10;
		const int deckW = playerDiscardZone.w > 0 ? playerDiscardZone.w : 110;
		const int deckH = playerDiscardZone.h > 0 ? playerDiscardZone.h : 130;
		const int deckY = playSlots.front().y + (playSlots.front().h - deckH) / 2;

		int deckX = playSlots.back().x + playSlots.back().w + gap;
		if (deckX + deckW > screenW - margin) deckX = std::max(margin, screenW - margin - deckW);

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
			SDL_Color{230, 230, 230, 255},
			deckBase.x + 6,
			deckBase.y + 6
		);
	}
}

void RenderPlaying::render(Playing& playing, const Game& game) {
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
	playing.computeUiRects(screenW, screenH);

	RenderText textRenderer;
	const RenderText::FontSet& uiFonts = game.getUIFonts();

	textRenderer.drawText(
		renderer,
		"Health: " + std::to_string(playing.player.health),
		playing.fonts.large,
		SDL_Color{255, 255, 255, 255},
		20,
		20
	);

	const std::string manaText = "Mana: " + std::to_string(playing.player.mana);
	textRenderer.drawText(
		renderer,
		manaText,
		playing.fonts.large,
		SDL_Color{255, 255, 255, 255},
		20,
		50
	);

	textRenderer.drawText(
		renderer,
		"Opponent Health: " + std::to_string(game.getHealth(game.getPlayer(true))),
		playing.fonts.small,
		SDL_Color{210, 210, 210, 255},
		20,
		80
	);

	const std::string opponentManaText = "Opponent Mana: " + std::to_string(game.getMana(game.getPlayer(true)));
	textRenderer.drawText(
		renderer,
		opponentManaText,
		playing.fonts.small,
		SDL_Color{210, 210, 210, 255},
		20,
		105
	);

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
	const bool hoveringMenu = playing.pointInRect(playing.menuButton, mouseX, mouseY);
	const bool hoveringExit = playing.menuOpen && playing.pointInRect(playing.exitGameButton, mouseX, mouseY);
	const bool hoveringReturn = playing.surrendered && playing.pointInRect(playing.returnToTitleButton, mouseX, mouseY);

	const bool draggingCard = playing.drag.active && playing.drag.index < playing.player.hand.size();
	const bool hasActiveDrawCard = playing.animationQueue.hasActiveDrawCard();
	const std::size_t activeDrawCardIndex = playing.animationQueue.getActiveDrawCardHandIndex();

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

	RenderBoard::drawOpponentPlayZones(renderer, textRenderer, playing.playSlots, playing.fonts.small);
	drawOpponentDeckAndDiscard(
		renderer,
		textRenderer,
		playing.playSlots,
		playing.discardZone,
		game.getDeck(game.getPlayer(true)).size(),
		screenW,
		playing.fonts.small
	);
	RenderBoard::drawPlayZones(renderer, textRenderer, playing.playSlots, playing.fonts.small);
	RenderBoard::drawDiscardZone(renderer, textRenderer, playing.discardZone, hoveringDiscard, playing.fonts.small);
	drawSelfDeck(
		renderer,
		textRenderer,
		playing.playSlots,
		playing.discardZone,
		playing.deck.size(),
		screenW,
		playing.fonts.small
	);

	if (playing.pendingSpellTarget.active) {
		SDL_SetRenderDrawColor(renderer, 250, 220, 90, 255);
		for (const auto& localSlot : playing.playSlots) {
			SDL_RenderDrawRect(renderer, &localSlot);
			SDL_Rect opponentSlot = localSlot;
			opponentSlot.y -= 200;
			SDL_RenderDrawRect(renderer, &opponentSlot);
		}

		std::string targetPrompt = "Choose target";
		if (playing.pendingSpellTarget.spell) {
			targetPrompt = "Choose target for " + playing.pendingSpellTarget.spell->getName();
		}
		textRenderer.drawText(
			renderer,
			targetPrompt,
			playing.fonts.small,
			SDL_Color{250, 240, 180, 255},
			20,
			130
		);
	}

	for (const auto& rect : opponentHandRects) {
		RenderCard::drawCardBack(renderer, rect);
	}

	for (std::size_t i = 0; i < playing.player.hand.size(); ++i) {
		if (draggingCard && i == playing.drag.index) continue;
		if (hasActiveDrawCard && i == activeDrawCardIndex) continue;
		if (i < playing.cardRects.size() && playing.player.hand[i]) {
			RenderCard::drawHandCard(renderer, textRenderer, *playing.player.hand[i], playing.cardRects[i], playing.fonts.small);
		}
	}

	if (hasActiveDrawCard) {
		RenderCard::drawCardBack(renderer, playing.animationQueue.getActiveDrawCardRect());
	}

	RenderBoard::drawBoardState(renderer, textRenderer, playing.board, playing.playSlots, playing.player.id, playing.fonts.small);

	if (draggingCard && playing.drag.index < playing.cardRects.size()) {
		SDL_Rect floating = playing.cardRects[playing.drag.index];
		floating.x = playing.drag.x;
		floating.y = playing.drag.y;
		RenderCard::drawHandCard(renderer, textRenderer, *playing.player.hand[playing.drag.index], floating, playing.fonts.small);
	}

	if (showPreview) {
		if (const auto& cardPtr = playing.player.hand[playing.hoverIndex]) {
			const int previewWidth = 180;
			const int previewHeight = 240;

			int handY = screenH - 165 - 30;
			if (!playing.cardRects.empty()) {
				handY = playing.cardRects.front().y;
			}

			int previewY = handY - previewHeight - 10;
			if (previewY < 20) previewY = 20;
			const int previewX = 20;

			SDL_Rect panel{previewX, previewY, previewWidth, previewHeight};
			RenderCard::drawPreview(renderer, textRenderer, *cardPtr, panel, playing.fonts.small, playing.fonts.large);
		}
	}

	if (!playing.surrendered) {
		RenderButton::drawButton(
			renderer,
			playing.menuButton,
			"Menu",
			uiFonts.medium,
			Theme::BTN_SECONDARY,
			Theme::BTN_BORDER,
			Theme::BTN_TEXT,
			hoveringMenu
		);

		if (playing.menuOpen) {
			RenderButton::drawButton(
				renderer,
				playing.exitGameButton,
				"Exit Game",
				uiFonts.medium,
				Theme::BTN_QUIT,
				Theme::BTN_BORDER,
				Theme::BTN_TEXT,
				hoveringExit
			);
		}
	}

	if (playing.surrendered) {
		SDL_Rect overlay{0, 0, screenW, screenH};
		SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 190);
		SDL_RenderFillRect(renderer, &overlay);

		int loseTextW = 0;
		int loseTextH = 0;
		if (playing.fonts.large) {
			TTF_SizeText(playing.fonts.large, "You Lose", &loseTextW, &loseTextH);
		}

		textRenderer.drawText(
			renderer,
			"You Lose",
			playing.fonts.large,
			Theme::TEXT_PRIMARY,
			(screenW - loseTextW) / 2,
			(screenH / 2) - loseTextH - 26
		);

		RenderButton::drawButton(
			renderer,
			playing.returnToTitleButton,
			"Return to Title",
			uiFonts.large,
			Theme::BTN_PRIMARY,
			Theme::BTN_BORDER,
			Theme::BTN_TEXT,
			hoveringReturn
		);
	}

	// SDL_RenderPresent(renderer);
}
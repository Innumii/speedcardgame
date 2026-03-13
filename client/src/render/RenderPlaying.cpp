#include "render/RenderPLaying.hpp"

#include "core/Game.hpp"
#include "objects/Card.h"
#include "render/RenderBoard.hpp"
#include "render/RenderButton.hpp"
#include "render/RenderCard.hpp"
#include "render/RenderText.hpp"
#include "render/Theme.hpp"
#include "utils/RenderUtil.hpp"
#include "states/Playing.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <algorithm>
#include <string>

namespace {
	void drawOpponentDeckAndDiscard(SDL_Renderer* renderer, RenderText& textRenderer,
			const std::vector<SDL_Rect>& playSlots, const SDL_Rect& playerDiscardZone,
			std::size_t deckSize, int screenW, TTF_Font* fontSmall) {
		if (!renderer || !fontSmall) return;
		if (playSlots.empty()) return;

		const int opponentOffset = 210;
		const int gap = 28;
		const int margin = 10;

		const int slotHeight = playSlots.front().h;
		const int discardSize = 120;
		const int deckSize_ = 120;
		const int opponentZoneY = playSlots.front().y - opponentOffset;
		const int discardY = opponentZoneY + (slotHeight - discardSize) / 2;

		const int leftEdge = playSlots.front().x - gap;
		const int rightEdge = playSlots.back().x + playSlots.back().w + gap;

		int discardX = leftEdge - discardSize;
		if (discardX < margin) discardX = margin;

		int deckX = rightEdge;
		if (deckX + deckSize_ > screenW - margin) {
			deckX = std::max(margin, screenW - margin - deckSize_);
		}

		SDL_Rect opponentDiscard{discardX, discardY, discardSize, discardSize};
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

		SDL_Rect deckBase{deckX, discardY, deckSize_, deckSize_};
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
			int deckSize, int screenW, TTF_Font* fontSmall) {
		if (!renderer || !fontSmall) return;
		if (playSlots.empty()) return;

		const int gap = 20;
		const int margin = 10;
		const int deckSize_ = 120;
		const int deckY = playSlots.front().y + (playSlots.front().h - deckSize_) / 2;

		// Position deck on RIGHT side of play zones
		int deckX = playSlots.back().x + playSlots.back().w + gap;
		if (deckX + deckSize_ > screenW - margin) {
			deckX = std::max(margin, screenW - margin - deckSize_);
		}

		SDL_Rect deckBase{deckX, deckY, deckSize_, deckSize_};
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

}

void RenderPlaying::render(Playing& playing, const Game& game) {
	SDL_Renderer* renderer = game.getRenderer();
	if (!renderer) return;

	const RenderText::FontSet& uiFonts = game.getUIFonts();
	const RenderText::FontSet& titleFonts = game.getTitleFonts();

	SDL_SetRenderDrawColor(renderer, 28, 22, 45, 255);
	SDL_RenderClear(renderer);

	const Uint32 now = SDL_GetTicks();

	int screenW = 0, screenH = 0;
	if (SDL_GetRendererOutputSize(renderer, &screenW, &screenH) != 0) {
		screenW = 800;
		screenH = 600;
	}

	RenderText textRenderer;

	// ── compute layouts ──────────────────────────────────────────────
	playing.cardRects = playing.computeCardLayout(playing.localPlayer.hand.size(), screenW, screenH);
	playing.computeZones(screenW, screenH);
	playing.computeUiRects(screenW, screenH);

	const std::size_t opponentHandSize = playing.remotePlayer.hand.size();
	std::vector<SDL_Rect> opponentHandRects = playing.computeCardLayout(opponentHandSize, screenW, screenH);
	if (!opponentHandRects.empty()) {
		const int cardHeight = opponentHandRects.front().h;
		int topHandY = 20;
		if (!playing.playSlots.empty()) {
			const int opponentOffset = 210;
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

	const bool draggingCard = playing.drag.active && playing.drag.index < playing.localPlayer.hand.size();
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
		(playing.hoverIndex != static_cast<std::size_t>(-1) &&
		 playing.hoverIndex < playing.localPlayer.hand.size() &&
		 now - playing.hoverStartTick >= hoverDelayMs) || playing.previewLocked;

	// ── draw zones ───────────────────────────────────────────────────
	RenderBoard::drawOpponentPlayZones(renderer, textRenderer, playing.playSlots, uiFonts.small);
	const int opponentDeckCount = playing.remotePlayer.getDeck().size();
	drawOpponentDeckAndDiscard(
		renderer,
		textRenderer,
		playing.playSlots,
		playing.discardZone,
		opponentDeckCount,
		screenW,
		uiFonts.small
	);
	RenderBoard::drawPlayZones(renderer, textRenderer, playing.playSlots, uiFonts.small);
	RenderBoard::drawDiscardZone(renderer, textRenderer, playing.discardZone, hoveringDiscard, uiFonts.small);
	const int selfDeckCount = playing.localPlayer.getDeck().size();
	drawSelfDeck(
		renderer,
		textRenderer,
		playing.playSlots,
		playing.discardZone,
		selfDeckCount,
		screenW,
		uiFonts.small
	);

	// ── opponent stats - top center horizontal bar ───────────────────
	const std::string opponentText = "Opponent";
	const std::string opponentHealthText = "Health: " + std::to_string(playing.remotePlayer.health);
	const std::string opponentManaText = "Mana: " + std::to_string(playing.remotePlayer.mana);

	int oppLabelW = 0, oppLabelH = 0;
	int oppHealthW = 0, oppHealthH = 0;
	int oppManaW = 0, oppManaH = 0;

	if (uiFonts.small) {
		TTF_SizeText(uiFonts.small, opponentText.c_str(), &oppLabelW, &oppLabelH);
		TTF_SizeText(uiFonts.small, opponentHealthText.c_str(), &oppHealthW, &oppHealthH);
		TTF_SizeText(uiFonts.small, opponentManaText.c_str(), &oppManaW, &oppManaH);
	}

	const int oppBarW = 480;
	const int oppBarH = 40;
	const int oppBarX = (screenW - oppBarW) / 2;
	const int oppBarY = 12;

	SDL_Rect opponentBar = {oppBarX, oppBarY, oppBarW, oppBarH};
	RenderUtil::drawRoundedRect(renderer, opponentBar,
	            8,
	            Theme::PANEL_FILL,
	            SDL_Color{100, 80, 120, 255});

	textRenderer.drawText(renderer, opponentText, uiFonts.small,
	                      SDL_Color{180, 170, 200, 255},
	                      oppBarX + 12, oppBarY + (oppBarH - oppLabelH) / 2);

	const int divider1X = oppBarX + oppLabelW + 24;
	SDL_SetRenderDrawColor(renderer, 100, 80, 120, 200);
	SDL_RenderDrawLine(renderer, divider1X, oppBarY + 8, divider1X, oppBarY + oppBarH - 8);

	textRenderer.drawText(renderer, opponentHealthText, uiFonts.small,
	                      SDL_Color{200, 140, 140, 255},
	                      divider1X + 12, oppBarY + (oppBarH - oppHealthH) / 2);

	const int divider2X = divider1X + oppHealthW + 24;
	SDL_SetRenderDrawColor(renderer, 100, 80, 120, 200);
	SDL_RenderDrawLine(renderer, divider2X, oppBarY + 8, divider2X, oppBarY + oppBarH - 8);

	textRenderer.drawText(renderer, opponentManaText, uiFonts.small,
	                      SDL_Color{140, 170, 200, 255},
	                      divider2X + 12, oppBarY + (oppBarH - oppManaH) / 2);

	// ── player stats - bottom center horizontal bar with GLOW ────────
	const std::string healthText = "Health: " + std::to_string(playing.localPlayer.health);
	const std::string manaText = "Mana: " + std::to_string(playing.localPlayer.mana);

	int healthW = 0, healthH = 0;
	int manaW = 0, manaH = 0;

	if (titleFonts.medium) {
		TTF_SizeText(titleFonts.medium, healthText.c_str(), &healthW, &healthH);
		TTF_SizeText(titleFonts.medium, manaText.c_str(), &manaW, &manaH);
	}

	const int playerBarW = 400;
	const int playerBarH = 50;
	const int playerBarX = (screenW - playerBarW) / 2;
	const int playerBarY = screenH - playerBarH - 20;

	SDL_Rect playerBar = {playerBarX, playerBarY, playerBarW, playerBarH};

	// Outer glow for entire bar
	RenderUtil::drawRoundedRect(renderer, {playerBar.x - 3, playerBar.y - 3, playerBar.w + 6, playerBar.h + 6},
	            12,
	            SDL_Color{80, 60, 100, 100},
	            SDL_Color{140, 120, 180, 180});

	RenderUtil::drawRoundedRect(renderer, playerBar,
	            10,
	            SDL_Color{30, 25, 45, 240},
	            SDL_Color{140, 120, 180, 255});

	// Player HP section (left half with red tint + glow)
	SDL_Rect hpSection = {playerBarX, playerBarY, playerBarW / 2, playerBarH};

	// HP glow
	RenderUtil::drawRoundedRect(renderer, {hpSection.x - 2, hpSection.y - 2, hpSection.w + 2, hpSection.h + 4},
	            12,
	            SDL_Color{60, 20, 20, 0},
	            SDL_Color{220, 80, 80, 150});

	RenderUtil::drawRoundedRect(renderer, hpSection,
	            10,
	            SDL_Color{40, 20, 20, 200},
	            SDL_Color{180, 60, 60, 255});

	textRenderer.drawText(renderer, healthText, titleFonts.medium,
	                      SDL_Color{255, 220, 220, 255},
	                      hpSection.x + (hpSection.w - healthW) / 2,
	                      hpSection.y + (hpSection.h - healthH) / 2);

	// Player MP section (right half with blue tint + glow)
	SDL_Rect mpSection = {playerBarX + playerBarW / 2, playerBarY, playerBarW / 2, playerBarH};

	// MP glow
	RenderUtil::drawRoundedRect(renderer, {mpSection.x - 2, mpSection.y - 2, mpSection.w + 4, mpSection.h + 4},
	            12,
	            SDL_Color{20, 40, 80, 0},
	            SDL_Color{100, 160, 255, 150});

	RenderUtil::drawRoundedRect(renderer, mpSection,
	            10,
	            SDL_Color{20, 30, 50, 200},
	            SDL_Color{60, 120, 200, 255});

	textRenderer.drawText(renderer, manaText, titleFonts.medium,
	                      SDL_Color{180, 220, 255, 255},
	                      mpSection.x + (mpSection.w - manaW) / 2,
	                      mpSection.y + (mpSection.h - manaH) / 2);

	// Center divider
	SDL_SetRenderDrawColor(renderer, 140, 120, 180, 255);
	SDL_RenderDrawLine(renderer, playerBarX + playerBarW / 2, playerBarY + 10,
	                   playerBarX + playerBarW / 2, playerBarY + playerBarH - 10);
	if (playing.pendingAction.active) {
		SDL_SetRenderDrawColor(renderer, 250, 220, 90, 255);
		for (const auto& localSlot : playing.playSlots) {
			SDL_RenderDrawRect(renderer, &localSlot);
			SDL_Rect opponentSlot = localSlot;
			opponentSlot.y -= 210;
			SDL_RenderDrawRect(renderer, &opponentSlot);
		}
		std::string targetPrompt = "Choose target";
		if (playing.pendingAction.cardId != -1) {
			auto& hand = playing.localPlayer.hand;

			auto it = std::find_if(hand.begin(), hand.end(),
				[&](const std::unique_ptr<Card>& c) {
					return c && c->getId() == playing.pendingAction.cardId;
				});

			if (it != hand.end() && *it) {
				targetPrompt = "Choose target for " + (*it)->getName();
			}
		}
		textRenderer.drawText(
			renderer,
			targetPrompt,
			uiFonts.small,
			SDL_Color{250, 240, 180, 255},
			20,
			130
		);
	}

	for (const auto& rect : opponentHandRects) {
		RenderCard::drawCardBack(renderer, rect);
	}

	for (std::size_t i = 0; i < playing.localPlayer.hand.size(); ++i) {
		if (draggingCard && i == playing.drag.index) continue;
		if (hasActiveDrawCard && i == activeDrawCardIndex) continue;
		if (i < playing.cardRects.size() && playing.localPlayer.hand[i]) {
			RenderCard::drawHandCard(renderer, textRenderer, *playing.localPlayer.hand[i], 
			                         playing.cardRects[i], uiFonts.tiny, uiFonts.small);
		}
	}

	if (hasActiveDrawCard) {
		RenderCard::drawCardBack(renderer, playing.animationQueue.getActiveDrawCardRect());
	}

	RenderBoard::drawBoardState(renderer, textRenderer, playing.board, playing.playSlots, uiFonts.small, uiFonts.small);

	if (draggingCard && playing.drag.index < playing.cardRects.size()) {
		SDL_Rect floating = playing.cardRects[playing.drag.index];
		floating.x = playing.drag.x;
		floating.y = playing.drag.y;
		RenderCard::drawHandCard(renderer, textRenderer, *playing.localPlayer.hand[playing.drag.index], 
		                         floating, uiFonts.tiny, uiFonts.small);
	}

	// ── card preview (left docked, non-blocking) ─────────────────────
	if (showPreview && playing.hoverIndex < playing.localPlayer.hand.size() && !playing.pauseModalOpen && !playing.exitModalOpen) {
		if (const auto& cardPtr = playing.localPlayer.hand[playing.hoverIndex]) {
			const int previewW = std::min(320, screenW - 40);
			const int previewH = static_cast<int>(previewW * 1.5f);
			const int previewX = 20;
			const int previewY = std::max(20, (screenH - previewH) / 2);

			SDL_Rect panel{previewX, previewY, previewW, previewH};
			
			RenderCard::drawPreview(renderer, textRenderer, *cardPtr, panel, 
			                        uiFonts.large, titleFonts.medium, playing.previewScrollOffset);
		}
	}

	// ── menu button - top right ──────────────────────────────────────
	if (!playing.surrendered && !playing.pauseModalOpen && !playing.exitModalOpen) {
		RenderUtil::drawRoundedRect(renderer, playing.menuButton,
		            10,
		            SDL_Color{50, 50, 60, 200},
		            hoveringMenu ? SDL_Color{120, 120, 140, 255} : SDL_Color{90, 90, 110, 255});
		
		int menuTextW = 0, menuTextH = 0;
		if (uiFonts.medium) {
			TTF_SizeText(uiFonts.medium, "Menu", &menuTextW, &menuTextH);
		}
		textRenderer.drawText(renderer, "Menu", uiFonts.medium,
		                      SDL_Color{220, 220, 230, 255},
		                      playing.menuButton.x + (playing.menuButton.w - menuTextW) / 2,
		                      playing.menuButton.y + (playing.menuButton.h - menuTextH) / 2);
	}

	// ── pause modal ──────────────────────────────────────────────────
	if (playing.pauseModalOpen && !playing.surrendered) {
		SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);
		SDL_RenderFillRect(renderer, nullptr);

		RenderUtil::drawRoundedRect(renderer, playing.pauseModal,
		            12,
		            Theme::PANEL_FILL,
		            SDL_Color{160, 120, 200, 255});

		int pauseTitleW = 0, pauseTitleH = 0;
		if (titleFonts.large) {
			TTF_SizeText(titleFonts.large, "Game Paused", &pauseTitleW, &pauseTitleH);
		}
		textRenderer.drawText(renderer, "Game Paused", titleFonts.large,
		                      Theme::TEXT_IVORY,
		                      playing.pauseModal.x + (playing.pauseModal.w - pauseTitleW) / 2,
		                      playing.pauseModal.y + 40);

		const bool hoveringResume = playing.pointInRect(playing.resumeButton, mouseX, mouseY);
		RenderUtil::drawRoundedRect(renderer, playing.resumeButton,
		            8,
		            hoveringResume ? SDL_Color{50, 180, 120, 255} : SDL_Color{35, 160, 130, 240},
		            hoveringResume ? SDL_Color{100, 220, 160, 255} : SDL_Color{80, 200, 140, 255});
		
		int resumeTextW = 0, resumeTextH = 0;
		if (uiFonts.large) {
			TTF_SizeText(uiFonts.large, "Resume", &resumeTextW, &resumeTextH);
		}
		textRenderer.drawText(renderer, "Resume", uiFonts.large,
		                      Theme::BTN_TEXT,
		                      playing.resumeButton.x + (playing.resumeButton.w - resumeTextW) / 2,
		                      playing.resumeButton.y + (playing.resumeButton.h - resumeTextH) / 2);

		const bool hoveringPauseExit = playing.pointInRect(playing.pauseExitButton, mouseX, mouseY);
		RenderUtil::drawRoundedRect(renderer, playing.pauseExitButton,
		            8,
		            hoveringPauseExit ? SDL_Color{220, 70, 90, 255} : SDL_Color{185, 50, 70, 240},
		            hoveringPauseExit ? SDL_Color{255, 120, 140, 255} : SDL_Color{220, 90, 110, 255});
		
		int exitTextW = 0, exitTextH = 0;
		if (uiFonts.large) {
			TTF_SizeText(uiFonts.large, "Exit Game", &exitTextW, &exitTextH);
		}
		textRenderer.drawText(renderer, "Exit Game", uiFonts.large,
		                      Theme::BTN_TEXT,
		                      playing.pauseExitButton.x + (playing.pauseExitButton.w - exitTextW) / 2,
		                      playing.pauseExitButton.y + (playing.pauseExitButton.h - exitTextH) / 2);
	}

	// ── exit confirmation modal ──────────────────────────────────────
	if (playing.exitModalOpen && !playing.surrendered) {
		SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 200);
		SDL_RenderFillRect(renderer, nullptr);

		RenderUtil::drawRoundedRect(renderer, playing.exitModal,
		            12,
		            Theme::PANEL_FILL,
		            SDL_Color{240, 192, 64, 150});

		int exitTitleW = 0, exitTitleH = 0;
		if (titleFonts.large) {
			TTF_SizeText(titleFonts.large, "Surrender Match?", &exitTitleW, &exitTitleH);
		}
		textRenderer.drawText(renderer, "Surrender Match?", titleFonts.large,
		                      Theme::TEXT_IVORY,
		                      playing.exitModal.x + (playing.exitModal.w - exitTitleW) / 2,
		                      playing.exitModal.y + 40);

		int questionW = 0, questionH = 0;
		const char* question = "Are you sure you want to surrender?";
		if (uiFonts.medium) {
			TTF_SizeText(uiFonts.medium, question, &questionW, &questionH);
		}
		textRenderer.drawText(renderer, question, uiFonts.medium,
		                      Theme::TEXT_MUTED,
		                      playing.exitModal.x + (playing.exitModal.w - questionW) / 2,
		                      playing.exitModal.y + 100);

		const bool hoveringSave = playing.pointInRect(playing.saveExitButton, mouseX, mouseY);
		RenderUtil::drawRoundedRect(renderer, playing.saveExitButton,
		            8,
		            hoveringSave ? SDL_Color{90, 140, 220, 255} : SDL_Color{70, 120, 200, 240},
		            hoveringSave ? SDL_Color{140, 180, 255, 255} : SDL_Color{110, 160, 240, 255});
		
		int saveTextW = 0, saveTextH = 0;
		if (uiFonts.medium) {
			TTF_SizeText(uiFonts.medium, "Yes, Surrender", &saveTextW, &saveTextH);
		}
		textRenderer.drawText(renderer, "Yes, Surrender", uiFonts.medium,
		                      Theme::BTN_TEXT,
		                      playing.saveExitButton.x + (playing.saveExitButton.w - saveTextW) / 2,
		                      playing.saveExitButton.y + (playing.saveExitButton.h - saveTextH) / 2);

		const bool hoveringNoSave = playing.pointInRect(playing.noSaveExitButton, mouseX, mouseY);
		RenderUtil::drawRoundedRect(renderer, playing.noSaveExitButton,
		            8,
		            hoveringNoSave ? SDL_Color{220, 70, 90, 255} : SDL_Color{185, 50, 70, 240},
		            hoveringNoSave ? SDL_Color{255, 120, 140, 255} : SDL_Color{220, 90, 110, 255});
		
		int noSaveTextW = 0, noSaveTextH = 0;
		if (uiFonts.medium) {
			TTF_SizeText(uiFonts.medium, "Cancel", &noSaveTextW, &noSaveTextH);
		}
		textRenderer.drawText(renderer, "Cancel", uiFonts.medium,
		                      Theme::BTN_TEXT,
		                      playing.noSaveExitButton.x + (playing.noSaveExitButton.w - noSaveTextW) / 2,
		                      playing.noSaveExitButton.y + (playing.noSaveExitButton.h - noSaveTextH) / 2);
	}

	// ── surrender screen ─────────────────────────────────────────────
	if (playing.surrendered) {
		SDL_Rect overlay{0, 0, screenW, screenH};
		SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 190);
		SDL_RenderFillRect(renderer, &overlay);

		int loseTextW = 0;
		int loseTextH = 0;
		if (titleFonts.large) {
			TTF_SizeText(titleFonts.large, "You Lose", &loseTextW, &loseTextH);
		}

		textRenderer.drawText(
			renderer,
			"You Lose",
			titleFonts.large,
			Theme::TEXT_PRIMARY,
			(screenW - loseTextW) / 2,
			(screenH / 2) - loseTextH - 26
		);

		const bool hoveringReturn = playing.pointInRect(playing.returnToTitleButton, mouseX, mouseY);
		RenderUtil::drawRoundedRect(renderer, playing.returnToTitleButton,
		            10,
		            SDL_Color{60, 100, 140, 220},
		            hoveringReturn ? SDL_Color{100, 160, 220, 255} : SDL_Color{80, 130, 180, 255});
		
		int returnTextW = 0, returnTextH = 0;
		if (uiFonts.large) {
			TTF_SizeText(uiFonts.large, "Return to Title", &returnTextW, &returnTextH);
		}
		textRenderer.drawText(renderer, "Return to Title", uiFonts.large,
		                      SDL_Color{255, 255, 255, 255},
		                      playing.returnToTitleButton.x + (playing.returnToTitleButton.w - returnTextW) / 2,
		                      playing.returnToTitleButton.y + (playing.returnToTitleButton.h - returnTextH) / 2);
	}

	SDL_RenderPresent(renderer);
}
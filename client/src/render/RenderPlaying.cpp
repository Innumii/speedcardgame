#include "render/RenderPlaying.hpp"

#include "animation/DrawCardAnimation.hpp"
#include "core/Game.hpp"
#include "render/RenderBoard.hpp"
#include "render/RenderButton.hpp"
#include "render/RenderCard.hpp"
#include "render/RenderCombatPhaseWidget.hpp"
#include "render/RenderTargeting.hpp"
#include "render/RenderText.hpp"
#include "render/Theme.hpp"
#include "utils/PlayingLayoutUtil.hpp"
#include "utils/PlayingRenderUtil.hpp"
#include "utils/RenderUtil.hpp"
#include "states/Playing.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <algorithm>
#include <set>
#include <string>
#include <utility>
#include <vector>

void RenderPlaying::render(Playing& playing, const Game& game) {
	SDL_Renderer* renderer = game.getRenderer();
	if (!renderer) return;

	const RenderText::FontSet& uiFonts = game.getUIFonts();
	const RenderText::FontSet& titleFonts = game.getTitleFonts();

	SDL_SetRenderDrawColor(renderer, Theme::Playing::BACKGROUND.r, Theme::Playing::BACKGROUND.g, Theme::Playing::BACKGROUND.b, Theme::Playing::BACKGROUND.a);
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
		const int topHandY = PlayingLayoutUtil::computeOpponentHandY(
			playing.opponentSlots,
			cardHeight,
			Theme::Playing::PREVIEW_MARGIN
		);
		for (auto& rect : opponentHandRects) {
			rect.y = topHandY;
		}
	}

	int mouseX = 0, mouseY = 0;
	SDL_GetMouseState(&mouseX, &mouseY);
	const bool hoveringDiscard = RenderUtil::pointInRect(playing.discardZone, mouseX, mouseY);
	const bool hoveringMenu = RenderUtil::pointInRect(playing.menuButton, mouseX, mouseY);

	const auto& activeAnimations = playing.animationQueue.getActiveAnimations();

	std::vector<std::shared_ptr<const DrawCardAnimation>> activeDrawAnimations;
	for (const auto& anim : activeAnimations) {
		auto found = PlayingRenderUtil::findDrawAnimation(anim);
		if (found) activeDrawAnimations.push_back(found);
	}

	const bool hasActiveDrawCard = !activeDrawAnimations.empty();

	const bool draggingCard = playing.drag.active && playing.drag.index < playing.localPlayer.hand.size();

	std::vector<PlayingRenderUtil::AttackRenderFrame> activeAttackFrames;
	std::vector<PlayingRenderUtil::DeathRenderFrame> activeDeathFrames;
	for (const auto& anim : activeAnimations) {
		PlayingRenderUtil::collectAttackFrames(anim, activeAttackFrames);
		PlayingRenderUtil::collectDeathFrames(anim, activeDeathFrames);
	}

	std::set<std::pair<int, int>> attackingSlots;
	for (const auto& frame : activeAttackFrames) {
		if (frame.lane < 0) {
			continue;
		}

		attackingSlots.emplace(frame.lane, frame.selfPlayer ? 0 : 1);
	}

	std::size_t newHoverIndex = static_cast<std::size_t>(-1);
	if (!draggingCard) {
		for (std::size_t i = 0; i < playing.cardRects.size(); ++i) {
			if (RenderUtil::pointInRect(playing.cardRects[i], mouseX, mouseY)) {
				newHoverIndex = i;
				break;
			}
		}
	}

	if (newHoverIndex != playing.hoverIndex) {
		playing.hoverIndex = newHoverIndex;
		playing.hoverStartTick = now;
	}

	constexpr Uint32 hoverDelayMs = Theme::Playing::HOVER_PREVIEW_DELAY_MS;
	const bool showHandPreview =
		(playing.hoverIndex != static_cast<std::size_t>(-1) &&
		 playing.hoverIndex < playing.localPlayer.hand.size() &&
		 now - playing.hoverStartTick >= hoverDelayMs) || playing.previewLocked;

	int hoveredBoardLane = -1;
	int hoveredBoardIndex = -1;
	if (!draggingCard && newHoverIndex == static_cast<std::size_t>(-1)) {
		for (std::size_t slot = 0; slot < playing.playSlots.size(); ++slot) {
			if (!RenderUtil::pointInRect(playing.playSlots[slot], mouseX, mouseY)) {
				continue;
			}

			const auto& localZone = playing.board.getZone(static_cast<int>(slot), 0);
			if (localZone.has_value() && localZone.value()) {
				hoveredBoardLane = static_cast<int>(slot);
				hoveredBoardIndex = 0;
			}
			break;
		}

		if (hoveredBoardLane < 0) {
			for (std::size_t slot = 0; slot < playing.opponentSlots.size(); ++slot) {
				if (!RenderUtil::pointInRect(playing.opponentSlots[slot], mouseX, mouseY)) {
					continue;
				}

				const auto& oppZone = playing.board.getZone(static_cast<int>(slot), 1);
				if (oppZone.has_value() && oppZone.value()) {
					hoveredBoardLane = static_cast<int>(slot);
					hoveredBoardIndex = 1;
				}
				break;
			}
		}
	}

	if (hoveredBoardLane != playing.boardHoverLane || hoveredBoardIndex != playing.boardHoverIndex) {
		playing.boardHoverLane = hoveredBoardLane;
		playing.boardHoverIndex = hoveredBoardIndex;
		playing.boardHoverStartTick = now;
	}

	const bool showBoardPreview =
		playing.boardHoverLane >= 0 &&
		playing.boardHoverIndex >= 0 &&
		now - playing.boardHoverStartTick >= hoverDelayMs;

	// ── draw zones ───────────────────────────────────────────────────
	RenderBoard::drawOpponentPlayZones(renderer, textRenderer, playing.opponentSlots, uiFonts.small);
	const int opponentDeckCount = playing.remotePlayer.getDeck().size();
	PlayingRenderUtil::drawOpponentDeckAndDiscard(
		renderer,
		textRenderer,
		playing.opponentSlots,
		opponentDeckCount,
		screenW,
		uiFonts.small
	);
	RenderBoard::drawPlayZones(renderer, textRenderer, playing.playSlots, uiFonts.small);
	RenderBoard::drawDiscardZone(renderer, textRenderer, playing.discardZone, hoveringDiscard, uiFonts.small);
	const int selfDeckCount = playing.localPlayer.getDeck().size();
	PlayingRenderUtil::drawSelfDeck(
		renderer,
		textRenderer,
		playing.playSlots,
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

	const int oppBarW = Theme::Playing::OPPONENT_BAR_WIDTH;
	const int oppBarH = Theme::Playing::OPPONENT_BAR_HEIGHT;
	const int oppBarX = (screenW - oppBarW) / 2;
	const int oppBarY = Theme::Playing::OPPONENT_BAR_TOP;

	SDL_Rect opponentBar = {oppBarX, oppBarY, oppBarW, oppBarH};
	RenderUtil::drawRoundedRect(renderer, opponentBar,
	            Theme::Board::ZONE_CORNER_RADIUS,
	            Theme::PANEL_FILL,
	            Theme::Playing::OPPONENT_BAR_BORDER);

	textRenderer.drawText(renderer, opponentText, uiFonts.small,
	                      Theme::Playing::OPPONENT_LABEL,
	                      oppBarX + Theme::Playing::OPPONENT_BAR_TEXT_PADDING, oppBarY + (oppBarH - oppLabelH) / 2);

	const int divider1X = oppBarX + oppLabelW + Theme::Playing::OPPONENT_BAR_DIVIDER_GAP;
	SDL_SetRenderDrawColor(renderer, Theme::Playing::OPPONENT_BAR_DIVIDER.r, Theme::Playing::OPPONENT_BAR_DIVIDER.g, Theme::Playing::OPPONENT_BAR_DIVIDER.b, Theme::Playing::OPPONENT_BAR_DIVIDER.a);
	SDL_RenderDrawLine(renderer, divider1X, oppBarY + Theme::Playing::OPPONENT_BAR_DIVIDER_INSET, divider1X, oppBarY + oppBarH - Theme::Playing::OPPONENT_BAR_DIVIDER_INSET);

	textRenderer.drawText(renderer, opponentHealthText, uiFonts.small,
	                      Theme::Playing::OPPONENT_HEALTH_TEXT,
	                      divider1X + Theme::Playing::OPPONENT_BAR_TEXT_PADDING, oppBarY + (oppBarH - oppHealthH) / 2);

	const int divider2X = divider1X + oppHealthW + Theme::Playing::OPPONENT_BAR_DIVIDER_GAP;
	SDL_SetRenderDrawColor(renderer, Theme::Playing::OPPONENT_BAR_DIVIDER.r, Theme::Playing::OPPONENT_BAR_DIVIDER.g, Theme::Playing::OPPONENT_BAR_DIVIDER.b, Theme::Playing::OPPONENT_BAR_DIVIDER.a);
	SDL_RenderDrawLine(renderer, divider2X, oppBarY + Theme::Playing::OPPONENT_BAR_DIVIDER_INSET, divider2X, oppBarY + oppBarH - Theme::Playing::OPPONENT_BAR_DIVIDER_INSET);

	textRenderer.drawText(renderer, opponentManaText, uiFonts.small,
	                      Theme::Playing::OPPONENT_MANA_TEXT,
	                      divider2X + Theme::Playing::OPPONENT_BAR_TEXT_PADDING, oppBarY + (oppBarH - oppManaH) / 2);

	// ── combat phase widget - compact center overlay ──────────────────
	const Uint32 cycleElapsed = now - playing.combatCycleStartTick;
	const Uint32 cyclePosition = cycleElapsed % Playing::COMBAT_CYCLE_DURATION_MS;
	const Uint32 preCombatDurationMs = Playing::COMBAT_PREPHASE_DURATION_MS;
	const bool combatPhaseActive = cyclePosition >= preCombatDurationMs;

	float barProgress = 1.0F;
	if (combatPhaseActive) {
		const Uint32 combatElapsed = cyclePosition - preCombatDurationMs;
		barProgress = std::min(1.0F, static_cast<float>(combatElapsed) / static_cast<float>(Playing::COMBAT_PHASE_DURATION_MS));
	} else {
		barProgress = 1.0F - std::min(1.0F, static_cast<float>(cyclePosition) / static_cast<float>(preCombatDurationMs));
	}

	const std::string combatLabel = combatPhaseActive ? "Combat Phase" : "Combat Soon";
	RenderCombatPhaseWidget::draw(
		renderer,
		textRenderer,
		uiFonts.small,
		screenW,
		screenH,
		playing.opponentSlots,
		playing.playSlots,
		combatPhaseActive,
		barProgress,
		combatLabel
	);

	// ── player stats - bottom center horizontal bar with GLOW ────────
	const std::string healthText = "Health: " + std::to_string(playing.localPlayer.health);
	const std::string manaText = "Mana: " + std::to_string(playing.localPlayer.mana);

	int healthW = 0, healthH = 0;
	int manaW = 0, manaH = 0;

	if (titleFonts.medium) {
		TTF_SizeText(titleFonts.medium, healthText.c_str(), &healthW, &healthH);
		TTF_SizeText(titleFonts.medium, manaText.c_str(), &manaW, &manaH);
	}

	const int playerBarW = Theme::Playing::PLAYER_BAR_WIDTH;
	const int playerBarH = Theme::Playing::PLAYER_BAR_HEIGHT;
	const int playerBarX = (screenW - playerBarW) / 2;
	const int playerBarY = screenH - playerBarH - Theme::Playing::PLAYER_BAR_BOTTOM_MARGIN;

	SDL_Rect playerBar = {playerBarX, playerBarY, playerBarW, playerBarH};

	// Outer glow for entire bar
	RenderUtil::drawRoundedRect(renderer, {playerBar.x - Theme::Playing::PLAYER_BAR_GLOW_INSET, playerBar.y - Theme::Playing::PLAYER_BAR_GLOW_INSET, playerBar.w + Theme::Playing::PLAYER_BAR_GLOW_INSET * 2, playerBar.h + Theme::Playing::PLAYER_BAR_GLOW_INSET * 2},
	            12,
	            Theme::Playing::PLAYER_BAR_GLOW_FILL,
	            Theme::Playing::PLAYER_BAR_GLOW_BORDER);

	RenderUtil::drawRoundedRect(renderer, playerBar,
	            10,
	            Theme::Playing::PLAYER_BAR_FILL,
	            Theme::Playing::PLAYER_BAR_BORDER);

	// Player HP section (left half with red tint + glow)
	SDL_Rect hpSection = {playerBarX, playerBarY, playerBarW / 2, playerBarH};

	// HP glow
	RenderUtil::drawRoundedRect(renderer, {hpSection.x - 2, hpSection.y - 2, hpSection.w + 2, hpSection.h + 4},
	            12,
	            Theme::Playing::PLAYER_HEALTH_GLOW_FILL,
	            Theme::Playing::PLAYER_HEALTH_GLOW_BORDER);

	RenderUtil::drawRoundedRect(renderer, hpSection,
	            10,
	            Theme::Playing::PLAYER_HEALTH_FILL,
	            Theme::Playing::PLAYER_HEALTH_BORDER);

	textRenderer.drawText(renderer, healthText, titleFonts.medium,
	                      Theme::Playing::PLAYER_HEALTH_TEXT,
	                      hpSection.x + (hpSection.w - healthW) / 2,
	                      hpSection.y + (hpSection.h - healthH) / 2);

	// Player MP section (right half with blue tint + glow)
	SDL_Rect mpSection = {playerBarX + playerBarW / 2, playerBarY, playerBarW / 2, playerBarH};

	// MP glow
	RenderUtil::drawRoundedRect(renderer, {mpSection.x - 2, mpSection.y - 2, mpSection.w + 4, mpSection.h + 4},
	            12,
	            Theme::Playing::PLAYER_MANA_GLOW_FILL,
	            Theme::Playing::PLAYER_MANA_GLOW_BORDER);

	RenderUtil::drawRoundedRect(renderer, mpSection,
	            10,
	            Theme::Playing::PLAYER_MANA_FILL,
	            Theme::Playing::PLAYER_MANA_BORDER);

	textRenderer.drawText(renderer, manaText, titleFonts.medium,
	                      Theme::Playing::PLAYER_MANA_TEXT,
	                      mpSection.x + (mpSection.w - manaW) / 2,
	                      mpSection.y + (mpSection.h - manaH) / 2);

	// Center divider
	SDL_SetRenderDrawColor(renderer, Theme::Playing::PLAYER_BAR_BORDER.r, Theme::Playing::PLAYER_BAR_BORDER.g, Theme::Playing::PLAYER_BAR_BORDER.b, Theme::Playing::PLAYER_BAR_BORDER.a);
	SDL_RenderDrawLine(renderer, playerBarX + playerBarW / 2, playerBarY + Theme::Playing::PLAYER_BAR_DIVIDER_INSET,
	                   playerBarX + playerBarW / 2, playerBarY + playerBarH - Theme::Playing::PLAYER_BAR_DIVIDER_INSET);
	RenderTargeting::drawPendingTargeting(renderer, textRenderer, playing, uiFonts.small);

	for (const auto& rect : opponentHandRects) {
		RenderCard::drawCardBack(renderer, rect);
	}

	for (std::size_t i = 0; i < playing.localPlayer.hand.size(); ++i) {
		if (draggingCard && i == playing.drag.index) continue;
		if (playing.animationQueue.hasPendingDrawForIndex(i)) continue;
		if (i < playing.cardRects.size() && playing.localPlayer.hand[i]) {
			RenderCard::drawHandCard(renderer, textRenderer, *playing.localPlayer.hand[i], 
			                         playing.cardRects[i], uiFonts.tiny, uiFonts.small);
		}
	}

	for (const auto& drawAnim : activeDrawAnimations) {
		RenderCard::drawCardBack(renderer, drawAnim->getCurrentRect());
	}

	if (!activeDeathFrames.empty()) {
		SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
		for (const auto& frame : activeDeathFrames) {
			SDL_SetRenderDrawColor(renderer, 25, 25, 25, static_cast<Uint8>(frame.alpha / 2));
			SDL_RenderFillRect(renderer, &frame.rect);

			SDL_SetRenderDrawColor(renderer, 255, 70, 70, frame.alpha);
			const int x1 = frame.rect.x + 8;
			const int y1 = frame.rect.y + 8;
			const int x2 = frame.rect.x + frame.rect.w - 8;
			const int y2 = frame.rect.y + frame.rect.h - 8;
			SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
			SDL_RenderDrawLine(renderer, x1, y2, x2, y1);
		}
	}

	RenderBoard::drawBoardState(
		renderer,
		textRenderer,
		playing.board,
		playing.playSlots,
		playing.opponentSlots,
		uiFonts.tiny,
		uiFonts.small,
		attackingSlots.empty() ? nullptr : &attackingSlots
	);

	if (!activeAttackFrames.empty()) {
		SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
		for (const auto& frame : activeAttackFrames) {
			if (frame.lane < 0) {
				continue;
			}

			const int boardIndex = frame.selfPlayer ? 0 : 1;
			const auto& zone = playing.board.getZone(frame.lane, boardIndex);
			if (!zone.has_value() || !zone.value()) {
				continue;
			}

			RenderCard::drawBoardCard(renderer, textRenderer, *zone.value(), frame.rect, uiFonts.tiny, uiFonts.small);
		}
	}

	if (draggingCard && playing.drag.index < playing.cardRects.size()) {
		SDL_Rect floating = playing.cardRects[playing.drag.index];
		floating.x = playing.drag.x;
		floating.y = playing.drag.y;
		RenderCard::drawHandCard(renderer, textRenderer, *playing.localPlayer.hand[playing.drag.index], 
		                         floating, uiFonts.tiny, uiFonts.small);
	}

	// ── side preview (left docked, non-blocking) ─────────────────────
	if (!playing.pauseModalOpen && !playing.exitModalOpen) {
		const Card* previewCard = nullptr;

		if (const Card* pendingSpell = playing.findPendingActionCard()) {
			previewCard = pendingSpell;
		} else if (playing.recentSpellPreview && now < playing.recentSpellPreviewUntil) {
			previewCard = playing.recentSpellPreview.get();
		} else if (showHandPreview && playing.hoverIndex < playing.localPlayer.hand.size()) {
			if (const auto& cardPtr = playing.localPlayer.hand[playing.hoverIndex]) {
				previewCard = cardPtr.get();
			}
		} else if (showBoardPreview) {
			const auto& boardZone = playing.board.getZone(playing.boardHoverLane, playing.boardHoverIndex);
			if (boardZone.has_value() && boardZone.value()) {
				previewCard = boardZone.value().get();
			}
		}

		if (previewCard) {
			int maxPreviewW = Theme::Playing::SIDE_PREVIEW_MAX_WIDTH;
			if (!playing.playSlots.empty()) {
				maxPreviewW = std::min(maxPreviewW, playing.playSlots.front().x - Theme::Playing::PREVIEW_MARGIN * 2);
			}
			if (!playing.opponentSlots.empty()) {
				maxPreviewW = std::min(maxPreviewW, playing.opponentSlots.front().x - Theme::Playing::PREVIEW_MARGIN * 2);
			}
			maxPreviewW = std::max(Theme::Playing::PREVIEW_MIN_WIDTH, maxPreviewW);

			const int previewW = std::min(maxPreviewW, screenW - Theme::Playing::PREVIEW_MARGIN * 2);
			const int previewH = static_cast<int>(previewW * Theme::Playing::PREVIEW_ASPECT_RATIO);
			const int previewX = Theme::Playing::PREVIEW_MARGIN;
			int previewY = Theme::Playing::PREVIEW_MARGIN + Theme::Playing::OPPONENT_BAR_HEIGHT + Theme::Playing::SIDE_PREVIEW_TOP_OFFSET;
			if (previewY + previewH > screenH - Theme::Playing::PREVIEW_MARGIN) {
				previewY = screenH - previewH - Theme::Playing::PREVIEW_MARGIN;
			}

			SDL_Rect panel{previewX, previewY, previewW, previewH};
			RenderCard::drawPreview(
				renderer,
				textRenderer,
				*previewCard,
				panel,
				uiFonts.small,
				titleFonts.medium,
				playing.previewScrollOffset
			);
		}
	}

	// ── menu button - top right ──────────────────────────────────────
	if (playing.getState() == PlayingGameState::Playing && !playing.pauseModalOpen && !playing.exitModalOpen) {
		RenderButton::Style menuStyle{};
		menuStyle.fill = Theme::BTN_SECONDARY;
		menuStyle.border = Theme::BTN_BORDER;
		menuStyle.text = Theme::BTN_TEXT;
		menuStyle.radius = Theme::Board::DISCARD_CORNER_RADIUS;
		RenderButton::drawButton(renderer, playing.menuButton, "Menu", uiFonts.medium, menuStyle, hoveringMenu, false);
	}

	// ── pause modal ──────────────────────────────────────────────────
	if (playing.pauseModalOpen && playing.getState() == PlayingGameState::Playing) {
		SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
		SDL_SetRenderDrawColor(renderer, Theme::Playing::PAUSE_OVERLAY.r, Theme::Playing::PAUSE_OVERLAY.g, Theme::Playing::PAUSE_OVERLAY.b, Theme::Playing::PAUSE_OVERLAY.a);
		SDL_RenderFillRect(renderer, nullptr);

		RenderUtil::drawRoundedRect(renderer, playing.pauseModal,
		            12,
		            Theme::PANEL_FILL,
		            Theme::Playing::PAUSE_MODAL_BORDER);

		int pauseTitleW = 0, pauseTitleH = 0;
		if (titleFonts.large) {
			TTF_SizeText(titleFonts.large, "Game Paused", &pauseTitleW, &pauseTitleH);
		}
		textRenderer.drawText(renderer, "Game Paused", titleFonts.large,
		                      Theme::TEXT_IVORY,
		                      playing.pauseModal.x + (playing.pauseModal.w - pauseTitleW) / 2,
		                      playing.pauseModal.y + Theme::Playing::PAUSE_MODAL_TITLE_TOP);

		const bool hoveringResume = RenderUtil::pointInRect(playing.resumeButton, mouseX, mouseY);
		RenderButton::drawButton(renderer, playing.resumeButton, "Resume", uiFonts.large,
		                       Theme::BTN_START, Theme::BTN_BORDER, Theme::BTN_TEXT,
		                       hoveringResume, false);

		const bool hoveringPauseExit = RenderUtil::pointInRect(playing.pauseExitButton, mouseX, mouseY);
		RenderButton::drawButton(renderer, playing.pauseExitButton, "Exit Game", uiFonts.large,
		                       Theme::BTN_QUIT, Theme::BTN_BORDER, Theme::BTN_TEXT,
		                       hoveringPauseExit, false);
	}

	// ── exit confirmation modal ──────────────────────────────────────
	if (playing.exitModalOpen && playing.getState() == PlayingGameState::Playing) {
		SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
		SDL_SetRenderDrawColor(renderer, Theme::Playing::EXIT_OVERLAY.r, Theme::Playing::EXIT_OVERLAY.g, Theme::Playing::EXIT_OVERLAY.b, Theme::Playing::EXIT_OVERLAY.a);
		SDL_RenderFillRect(renderer, nullptr);

		RenderUtil::drawRoundedRect(renderer, playing.exitModal,
		            12,
		            Theme::PANEL_FILL,
		            Theme::Playing::EXIT_MODAL_BORDER);

		int exitTitleW = 0, exitTitleH = 0;
		if (titleFonts.large) {
			TTF_SizeText(titleFonts.large, "Surrender?", &exitTitleW, &exitTitleH);
		}
		textRenderer.drawText(renderer, "Surrender?", titleFonts.large,
		                      Theme::TEXT_IVORY,
		                      playing.exitModal.x + (playing.exitModal.w - exitTitleW) / 2,
		                      playing.exitModal.y + Theme::Playing::EXIT_MODAL_TITLE_TOP);

		int questionW = 0, questionH = 0;
		const char* question = "Are you sure you want to surrender?";
		if (uiFonts.medium) {
			TTF_SizeText(uiFonts.medium, question, &questionW, &questionH);
		}
		textRenderer.drawText(renderer, question, uiFonts.medium,
		                      Theme::TEXT_MUTED,
		                      playing.exitModal.x + (playing.exitModal.w - questionW) / 2,
		                      playing.exitModal.y + Theme::Playing::EXIT_MODAL_QUESTION_TOP);

		const bool hoveringSave = RenderUtil::pointInRect(playing.saveExitButton, mouseX, mouseY);
		RenderButton::drawButton(renderer, playing.saveExitButton, "Yes, Surrender", uiFonts.medium,
		                       Theme::BTN_PRIMARY, Theme::BTN_BORDER, Theme::BTN_TEXT,
		                       hoveringSave, false);

		const bool hoveringNoSave = RenderUtil::pointInRect(playing.noSaveExitButton, mouseX, mouseY);
		RenderButton::drawButton(renderer, playing.noSaveExitButton, "Cancel", uiFonts.medium,
		                       Theme::BTN_QUIT, Theme::BTN_BORDER, Theme::BTN_TEXT,
		                       hoveringNoSave, false);
	}

	// ── Game End Screen (need a button for requeue)─────────────────────────────────────────────
	//Currently, game end does NOT mean disconnect from server. They are still in the server.
	if (playing.getState() != PlayingGameState::Playing) {
		std::string msg = "Defeat";
		int coinReward = playing.getCoinReward();
		if (playing.getState() == PlayingGameState::Won) {
			msg = "Victory";
		}
		SDL_Rect overlay{0, 0, screenW, screenH};
		SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
		SDL_SetRenderDrawColor(renderer, Theme::Playing::SURRENDER_OVERLAY.r, Theme::Playing::SURRENDER_OVERLAY.g, Theme::Playing::SURRENDER_OVERLAY.b, Theme::Playing::SURRENDER_OVERLAY.a);
		SDL_RenderFillRect(renderer, &overlay);

		int loseTextW = 0;
		int loseTextH = 0;
		if (titleFonts.large) {
			TTF_SizeText(titleFonts.large, msg.data(), &loseTextW, &loseTextH);
		}

		textRenderer.drawText(
			renderer,
			msg,
			titleFonts.large,
			Theme::TEXT_PRIMARY,
			(screenW - loseTextW) / 2,
			(screenH / 2) - loseTextH - 26
		);

		if (coinReward > 0) {
			std::string coinsMsg = std::to_string(coinReward) + " Coins Earned!";
			int coinsTextW = 0;
			int coinsTextH = 0;
			if (uiFonts.large) {
				TTF_SizeText(uiFonts.large, coinsMsg.c_str(), &coinsTextW, &coinsTextH);
			}

			textRenderer.drawText(
				renderer,
				coinsMsg,
				uiFonts.large,
				Theme::TEXT_PRIMARY,
				(screenW - coinsTextW) / 2,
				(screenH / 2) - 16   //
			);
		}


		RenderButton::Style returnStyle{};
		returnStyle.fill = Theme::BTN_START;
		returnStyle.border = Theme::BTN_BORDER;
		returnStyle.text = Theme::BTN_TEXT;
		returnStyle.radius = 10;

		const bool hoveringReturn = RenderUtil::pointInRect(playing.returnToTitleButton, mouseX, mouseY);
		RenderButton::drawButton(renderer, playing.returnToTitleButton, "Return to Title", uiFonts.large, returnStyle, hoveringReturn, false);

		// ── Requeue button ───────────────────────────────────────────────
		const bool hoveringRequeue = RenderUtil::pointInRect(playing.requeueButton, mouseX, mouseY);
		RenderButton::drawButton(renderer, playing.requeueButton, "Requeue", uiFonts.large, returnStyle, hoveringRequeue, false);
	}

	SDL_RenderPresent(renderer);
}
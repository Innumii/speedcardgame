#include "render/RenderPlaying.hpp"

#include "animation/DrawCardAnimation.hpp"
#include "animation/DiscardAnimation.hpp"
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
#include <render/RenderCardOverlay.hpp>
#include <iostream>
#include <animation/SummonAnimation.hpp>
#include <animation/AttackAnimation.hpp>

// Reference resolution the Theme pixel constants were authored for.
static constexpr float kRefW = 1200.0F;
static constexpr float kRefH = 850.0F;

static void drawStatsBar(
	SDL_Renderer* renderer,
	RenderText& textRenderer,
	TTF_Font* font,
	int health,
	int mana,
	int screenW,
	int screenH,
	bool anchorTop)  // true = opponent bar (top), false = local player bar (bottom)
{
	// Scale every pixel value uniformly so the bars shrink with the window.
	const float scale = std::min(
		static_cast<float>(screenW) / kRefW,
		static_cast<float>(screenH) / kRefH);

	const int barW      = static_cast<int>(Theme::Playing::PLAYER_BAR_WIDTH          * scale);
	const int barH      = static_cast<int>(Theme::Playing::PLAYER_BAR_HEIGHT         * scale);
	const int glowInset = std::max(1, static_cast<int>(Theme::Playing::PLAYER_BAR_GLOW_INSET    * scale));
	const int divInset  = std::max(1, static_cast<int>(Theme::Playing::PLAYER_BAR_DIVIDER_INSET * scale));
	// Small per-section glow offsets (originally 2 px) scaled to match.
	const int g         = std::max(1, static_cast<int>(2.0F * scale));
	const int cornerOuter = std::max(2, static_cast<int>(12.0F * scale));
	const int cornerInner = std::max(2, static_cast<int>(10.0F * scale));

	const int barX = (screenW - barW) / 2;
	const int verticalOffset = (screenH - static_cast<int>(850.0F * scale)) / 2;
	const int barY = anchorTop
		? static_cast<int>(Theme::Playing::OPPONENT_BAR_TOP * scale) + verticalOffset
		: screenH - barH - static_cast<int>(Theme::Playing::PLAYER_BAR_BOTTOM_MARGIN * scale) - verticalOffset;

	const std::string healthText = "Health: " + std::to_string(health);
	const std::string manaText   = "Mana: "   + std::to_string(mana);

	int healthW = 0, healthH = 0;
	int manaW   = 0, manaH   = 0;
	if (font) {
		TTF_SizeText(font, healthText.c_str(), &healthW, &healthH);
		TTF_SizeText(font, manaText.c_str(),   &manaW,   &manaH);
	}

	const SDL_Rect bar = {barX, barY, barW, barH};

	// Outer glow
	RenderUtil::drawRoundedRect(renderer,
		{bar.x - glowInset, bar.y - glowInset,
		 bar.w + glowInset * 2, bar.h + glowInset * 2},
		cornerOuter,
		Theme::Playing::PLAYER_BAR_GLOW_FILL,
		Theme::Playing::PLAYER_BAR_GLOW_BORDER);

	RenderUtil::drawRoundedRect(renderer, bar,
		cornerInner,
		Theme::Playing::PLAYER_BAR_FILL,
		Theme::Playing::PLAYER_BAR_BORDER);

	// HP section (left half — red tint + glow)
	const SDL_Rect hpSection = {barX, barY, barW / 2, barH};

	RenderUtil::drawRoundedRect(renderer,
		{hpSection.x - g, hpSection.y - g, hpSection.w + g, hpSection.h + g * 2},
		cornerOuter,
		Theme::Playing::PLAYER_HEALTH_GLOW_FILL,
		Theme::Playing::PLAYER_HEALTH_GLOW_BORDER);

	RenderUtil::drawRoundedRect(renderer, hpSection,
		cornerInner,
		Theme::Playing::PLAYER_HEALTH_FILL,
		Theme::Playing::PLAYER_HEALTH_BORDER);

	textRenderer.drawText(renderer, healthText, font,
		Theme::Playing::PLAYER_HEALTH_TEXT,
		hpSection.x + (hpSection.w - healthW) / 2,
		hpSection.y + (hpSection.h - healthH) / 2);

	// MP section (right half — blue tint + glow)
	const SDL_Rect mpSection = {barX + barW / 2, barY, barW / 2, barH};

	RenderUtil::drawRoundedRect(renderer,
		{mpSection.x - g, mpSection.y - g, mpSection.w + g * 2, mpSection.h + g * 2},
		cornerOuter,
		Theme::Playing::PLAYER_MANA_GLOW_FILL,
		Theme::Playing::PLAYER_MANA_GLOW_BORDER);

	RenderUtil::drawRoundedRect(renderer, mpSection,
		cornerInner,
		Theme::Playing::PLAYER_MANA_FILL,
		Theme::Playing::PLAYER_MANA_BORDER);

	textRenderer.drawText(renderer, manaText, font,
		Theme::Playing::PLAYER_MANA_TEXT,
		mpSection.x + (mpSection.w - manaW) / 2,
		mpSection.y + (mpSection.h - manaH) / 2);

	// Center divider
	SDL_SetRenderDrawColor(renderer,
		Theme::Playing::PLAYER_BAR_BORDER.r,
		Theme::Playing::PLAYER_BAR_BORDER.g,
		Theme::Playing::PLAYER_BAR_BORDER.b,
		Theme::Playing::PLAYER_BAR_BORDER.a);
	SDL_RenderDrawLine(renderer,
		barX + barW / 2, barY + divInset,
		barX + barW / 2, barY + barH - divInset);
}

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
		screenH,
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
		screenH,
		uiFonts.small
	);

	// ── opponent stats - top center horizontal bar with GLOW ─────────
	drawStatsBar(
		renderer, textRenderer, titleFonts.medium,
		playing.remotePlayer.health, playing.remotePlayer.mana,
		screenW, screenH, true);

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
	drawStatsBar(
		renderer, textRenderer, titleFonts.medium,
		playing.localPlayer.health, playing.localPlayer.mana,
		screenW, screenH, false);
	RenderTargeting::drawPendingTargeting(renderer, textRenderer, playing, uiFonts.small);

	for (const auto& rect : opponentHandRects) {
		RenderCard::drawCardBack(renderer, rect);
	}

	for (std::size_t i = 0; i < playing.localPlayer.hand.size(); ++i) {
		if (draggingCard && i == playing.drag.index) continue;
		if (playing.animationQueue.hasPendingDrawForIndex(i)) continue;
		if (i < playing.cardRects.size() && playing.localPlayer.hand[i]) {
			if (DiscardAnimation::hasPending(playing.localPlayer.hand[i]->getId())) continue;
			RenderCard::drawHandCard(renderer, textRenderer, *playing.localPlayer.hand[i],
									playing.cardRects[i], uiFonts.tiny, uiFonts.small);
		}
	}

	for (const auto& drawAnim : activeDrawAnimations) {
		const SDL_Rect rect     = drawAnim->getCurrentRect();
		const float    progress = drawAnim->getProgress();

		constexpr float flipStartProgress = 0.55F;

		if (progress < flipStartProgress) {
			RenderCard::drawCardBack(renderer, rect);
		} else {
			const float t          = (progress - flipStartProgress) / (1.0F - flipStartProgress);
			const float widthScale = std::abs(1.0F - 2.0F * t);
			const int   scaledW    = std::max(1, static_cast<int>(rect.w * widthScale));
			const SDL_Rect flipRect{
				rect.x + (rect.w - scaledW) / 2,
				rect.y,
				scaledW,
				rect.h
			};

			if (t < 0.5F) {
				RenderCard::drawCardBack(renderer, flipRect);
			} else {
				const std::size_t idx = drawAnim->getHandIndex();
				if (idx < playing.localPlayer.hand.size() && playing.localPlayer.hand[idx]) {
					RenderCard::drawHandCard(renderer, textRenderer,
											*playing.localPlayer.hand[idx],
											flipRect, uiFonts.tiny, uiFonts.small);
				}
			}
		}
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

	for (const auto& anim : activeAnimations) {
		if (auto summon = std::dynamic_pointer_cast<SummonAnimation>(anim)) {
			summon->draw(renderer);
		}
	}

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

	// collect active discard animations (same pattern as draw animations)
	std::vector<std::shared_ptr< DiscardAnimation>> activeDiscardAnimations;
	for (const auto& anim : activeAnimations) {
		if (auto found = std::dynamic_pointer_cast< DiscardAnimation>(anim))
			activeDiscardAnimations.push_back(found);
	}

	// draw them
	for (const auto& discardAnim : activeDiscardAnimations) {
		const SDL_Rect rect = discardAnim->getRect();

		if (!discardAnim->getCachedTexture()) {
			SDL_Texture* target = SDL_CreateTexture(renderer,
				SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, rect.w, rect.h);
			if (!target) continue;
			SDL_SetTextureBlendMode(target, SDL_BLENDMODE_BLEND);
			SDL_SetRenderTarget(renderer, target);
			SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
			SDL_RenderClear(renderer);
			const Card* card = discardAnim->getOwnedCard();
			if (card) {
				RenderCard::drawHandCard(renderer, textRenderer, *card,
										{0, 0, rect.w, rect.h}, uiFonts.tiny, uiFonts.small);
			} else {
				RenderCard::drawCardBack(renderer, {0, 0, rect.w, rect.h});
			}
			SDL_SetRenderTarget(renderer, nullptr);
			discardAnim->setCachedTexture(target);
		}

		discardAnim->drawDisintegration(renderer, now);
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
			const int previewH = static_cast<int>(previewW * Theme::PREVIEW_ASPECT_RATIO);
			const int previewX = Theme::Playing::PREVIEW_MARGIN;
			int previewY = 0;
			if (playing.recentSpellPreview && previewCard == playing.recentSpellPreview.get()) {
				// Spell preview: top-anchored (original behaviour)
				previewY = Theme::Playing::PREVIEW_MARGIN + Theme::Playing::OPPONENT_BAR_HEIGHT + Theme::Playing::SIDE_PREVIEW_TOP_OFFSET;
				if (previewY + previewH > screenH - Theme::Playing::PREVIEW_MARGIN) {
					previewY = screenH - previewH - Theme::Playing::PREVIEW_MARGIN;
				}
			} else {
				// Hand/board hover preview: bottom-anchored
				const int playerBarTop = screenH - Theme::Playing::PLAYER_BAR_HEIGHT - Theme::Playing::PLAYER_BAR_BOTTOM_MARGIN;
				previewY = playerBarTop - previewH - Theme::Playing::PREVIEW_MARGIN;
				if (previewY < Theme::Playing::PREVIEW_MARGIN) {
					previewY = Theme::Playing::PREVIEW_MARGIN;
				}
			}

			SDL_Rect panel{previewX, previewY, previewW, previewH};

			// Entrance + exit scale animation — only for spell previews
			if (playing.recentSpellPreview && previewCard == playing.recentSpellPreview.get()) {
				constexpr Uint32 entranceDurationMs = 180U;
				constexpr Uint32 exitDurationMs     = 130U;
				constexpr float  peakScale          = 1.12F;
				constexpr float  exitMinScale       = 0.75F;

				const Uint32 elapsed   = now - playing.recentSpellPreviewStartTick;
				const Uint32 remaining = playing.recentSpellPreviewUntil > now
									? playing.recentSpellPreviewUntil - now : 0;

				float scale = 1.0F;

				if (elapsed < entranceDurationMs) {
					// Entrance: ease out from peakScale → 1.0
					const float t     = static_cast<float>(elapsed) / static_cast<float>(entranceDurationMs);
					const float eased = 1.0F - (1.0F - t) * (1.0F - t);
					scale = peakScale - (peakScale - 1.0F) * eased;
				} else if (remaining < exitDurationMs) {
					// Exit: ease in from 1.0 → exitMinScale (shrinks away)
					const float t     = 1.0F - static_cast<float>(remaining) / static_cast<float>(exitDurationMs);
					const float eased = t * t;
					scale = 1.0F - (1.0F - exitMinScale) * eased;
				}

				if (scale != 1.0F) {
					const int scaledW = static_cast<int>(previewW * scale);
					const int scaledH = static_cast<int>(previewH * scale);
					panel = {
						previewX - (scaledW - previewW) / 2,
						previewY - (scaledH - previewH) / 2,
						scaledW,
						scaledH
					};
				}
			}
			RenderCard::drawPreview(
				renderer,
				textRenderer,
				*previewCard,
				panel,
				uiFonts.small,
				titleFonts.medium,
				playing.previewScrollOffset
			);

			// Shimmer overlay — spell previews only
			if (playing.recentSpellPreview && previewCard == playing.recentSpellPreview.get()) {
    				RenderCardOverlay::overlayShimmer(renderer, panel, now);
			}
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
	// ── Game End Screen ─────────────────────────────────────────────────────────
	if (playing.getState() != PlayingGameState::Playing) {
		std::string msg = "Defeat";
		int coinReward = playing.getCoinReward();
		if (playing.getState() == PlayingGameState::Won) {
			msg = "Victory";
		}

	// ── Camera shake intro ───────────────────────────────────────────
	PlayingGameState currentState = playing.getState();

	// Reset timer whenever we freshly enter an end state
	if (playing.lastEndState != currentState && currentState != PlayingGameState::Playing) {
		playing.gameEndStartTick = now;
	}
	playing.lastEndState = currentState;

	// Shake parameters
	constexpr Uint32 shakeDurationMs  = 600U;
	constexpr float  shakePeakPixels  = 18.0F;
	constexpr float  shakeFrequency   = 28.0F; // oscillations/sec

	int shakeX = 0, shakeY = 0;
	const Uint32 shakeElapsed = now - playing.gameEndStartTick;
	if (shakeElapsed < shakeDurationMs) {
		const float t       = static_cast<float>(shakeElapsed) / static_cast<float>(shakeDurationMs);
		const float decay   = (1.0F - t) * (1.0F - t);                         // quadratic ease-out
		const float timeS   = static_cast<float>(shakeElapsed) / 1000.0F;
		const float sineX   = std::sin(timeS * shakeFrequency * 3.14159F * 2.0F);
		const float sineY   = std::sin(timeS * shakeFrequency * 3.14159F * 2.0F * 1.3F); // slight phase offset
		shakeX = static_cast<int>(sineX * shakePeakPixels * decay);
		shakeY = static_cast<int>(sineY * shakePeakPixels * decay * 0.5F);     // vertical shake is subtler
	}

		// ── Overlay ──────────────────────────────────────────────────────
		SDL_Rect overlay{0, 0, screenW, screenH};
		SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
		SDL_SetRenderDrawColor(renderer,
			Theme::Playing::SURRENDER_OVERLAY.r,
			Theme::Playing::SURRENDER_OVERLAY.g,
			Theme::Playing::SURRENDER_OVERLAY.b,
			Theme::Playing::SURRENDER_OVERLAY.a);
		SDL_RenderFillRect(renderer, &overlay);

		// ── Victory / Defeat text ────────────────────────────────────────
		int loseTextW = 0, loseTextH = 0;
		if (titleFonts.large) {
			TTF_SizeText(titleFonts.large, msg.data(), &loseTextW, &loseTextH);
		}
		textRenderer.drawText(
			renderer, msg, titleFonts.large, Theme::TEXT_PRIMARY,
			(screenW - loseTextW) / 2 + shakeX,
			(screenH / 2) - loseTextH - 26 + shakeY
		);

		// ── Coin reward text ─────────────────────────────────────────────
		if (coinReward > 0) {
			std::string coinsMsg = std::to_string(coinReward) + " Coins Earned!";
			int coinsTextW = 0, coinsTextH = 0;
			if (uiFonts.large) {
				TTF_SizeText(uiFonts.large, coinsMsg.c_str(), &coinsTextW, &coinsTextH);
			}
			textRenderer.drawText(
				renderer, coinsMsg, uiFonts.large, Theme::TEXT_PRIMARY,
				(screenW - coinsTextW) / 2 + shakeX,
				(screenH / 2) - 16 + shakeY
			);
		}

		// ── Buttons (shake applied to their rects) ───────────────────────
		RenderButton::Style returnStyle{};
		returnStyle.fill   = Theme::BTN_START;
		returnStyle.border = Theme::BTN_BORDER;
		returnStyle.text   = Theme::BTN_TEXT;
		returnStyle.radius = 10;

		SDL_Rect shakenReturn  = playing.returnToTitleButton;
		SDL_Rect shakenRequeue = playing.requeueButton;
		shakenReturn.x  += shakeX;  shakenReturn.y  += shakeY;
		shakenRequeue.x += shakeX;  shakenRequeue.y += shakeY;

		const bool hoveringReturn  = RenderUtil::pointInRect(playing.returnToTitleButton, mouseX, mouseY);
		const bool hoveringRequeue = RenderUtil::pointInRect(playing.requeueButton,       mouseX, mouseY);

		RenderButton::drawButton(renderer, shakenReturn,  "Return to Title", uiFonts.large, returnStyle, hoveringReturn,  false);
		RenderButton::drawButton(renderer, shakenRequeue, "Requeue",         uiFonts.large, returnStyle, hoveringRequeue, false);
	}

	SDL_RenderPresent(renderer);
}
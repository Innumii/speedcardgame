#include "render/RenderTargeting.hpp"

#include "objects/Card.h"
#include "render/RenderText.hpp"
#include "render/Theme.hpp"
#include "states/Playing.hpp"
#include "utils/GetValidTargets.hpp"
#include "utils/PlayingLayoutUtil.hpp"

#include <algorithm>
#include <string>

/*
    This function is responsible for drawing the targeting overlay and prompt when the player has an active pending action that requires target selection.

    It checks if there is an active pending action in the Playing state. If so, it highlights all valid target zones (play slots and opponent slots) by drawing rectangles around them.

    It also constructs a prompt string to indicate what the player should do (e.g., "Choose target for Fireball") based on the card that is being played. This prompt is then rendered on the screen.
*/
void RenderTargeting::drawPendingTargeting(SDL_Renderer* renderer, RenderText& textRenderer, const Playing& playing, TTF_Font* fontSmall) {
	if (!renderer || !fontSmall) return;
	if (!playing.pendingAction.active) return;

    SDL_BlendMode previousBlendMode = SDL_BLENDMODE_NONE;
    SDL_GetRenderDrawBlendMode(renderer, &previousBlendMode);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

	SDL_SetRenderDrawColor(
		renderer,
		Theme::Playing::TARGET_HIGHLIGHT.r,
		Theme::Playing::TARGET_HIGHLIGHT.g,
		Theme::Playing::TARGET_HIGHLIGHT.b,
		Theme::Playing::TARGET_HIGHLIGHT.a
	);
	// Highlight valid target zones
	const auto& hand = playing.localPlayer.hand;
	auto cardIt = std::find_if(hand.begin(), hand.end(),
		[&](const std::unique_ptr<Card>& card) {
			return card && card->getId() == playing.pendingAction.cardId;
		});
	if (cardIt == hand.end() || !*cardIt) return;

    int screenW = 0;
    int screenH = 0;
    if (SDL_GetRendererOutputSize(renderer, &screenW, &screenH) != 0) {
        screenW = 800;
        screenH = 600;
    }

    const SDL_Rect opponentPlayerRect{
        (screenW - Theme::Playing::OPPONENT_BAR_WIDTH) / 2,
        Theme::Playing::OPPONENT_BAR_TOP,
        Theme::Playing::OPPONENT_BAR_WIDTH,
        Theme::Playing::OPPONENT_BAR_HEIGHT
    };

    const SDL_Rect localPlayerRect{
        (screenW - Theme::Playing::PLAYER_BAR_WIDTH) / 2,
        screenH - Theme::Playing::PLAYER_BAR_HEIGHT - Theme::Playing::PLAYER_BAR_BOTTOM_MARGIN,
        Theme::Playing::PLAYER_BAR_WIDTH,
        Theme::Playing::PLAYER_BAR_HEIGHT
    };

    auto drawTargetHighlight = [&](const SDL_Rect& rect) {
        SDL_SetRenderDrawColor(renderer,
            Theme::Playing::TARGET_HIGHLIGHT.r,
            Theme::Playing::TARGET_HIGHLIGHT.g,
            Theme::Playing::TARGET_HIGHLIGHT.b,
            55);
        SDL_RenderFillRect(renderer, &rect);

        for (int ring = 0; ring < 3; ++ring) {
            SDL_Rect borderRect{
                rect.x - ring,
                rect.y - ring,
                rect.w + ring * 2,
                rect.h + ring * 2
            };
            SDL_SetRenderDrawColor(renderer,
                Theme::Playing::TARGET_HIGHLIGHT.r,
                Theme::Playing::TARGET_HIGHLIGHT.g,
                Theme::Playing::TARGET_HIGHLIGHT.b,
                static_cast<Uint8>(220 - (ring * 50)));
            SDL_RenderDrawRect(renderer, &borderRect);
        }
    };

    std::vector<SDL_Rect> opponentHandRects = playing.computeCardLayout(playing.remotePlayer.hand.size(), screenW, screenH);
    if (!opponentHandRects.empty() && !playing.opponentSlots.empty()) {
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

    std::vector<int> validTargets = getValidTargets(playing, **cardIt, playing.pendingAction.sourceLane);
    for (int target : validTargets) {
        if (target >= 0 && target < static_cast<int>(opponentHandRects.size())) {
            // Highlight opponent hand cards
            SDL_Rect highlightRect = opponentHandRects[static_cast<std::size_t>(target)];
            drawTargetHighlight(highlightRect);
        } else if (target >= 100 && target < 200) {
            // Highlight local player board creatures
            int laneIndex = target - 100;
            if (laneIndex >= 0 && laneIndex < static_cast<int>(playing.playSlots.size())) {
                SDL_Rect highlightRect = playing.playSlots[static_cast<std::size_t>(laneIndex)];
                drawTargetHighlight(highlightRect);
            }
        } else if (target >= 200 && target < 300) {
            // Highlight opponent board creatures
            int laneIndex = target - 200;
            if (laneIndex >= 0 && laneIndex < static_cast<int>(playing.opponentSlots.size())) {
                SDL_Rect highlightRect = playing.opponentSlots[static_cast<std::size_t>(laneIndex)];
                drawTargetHighlight(highlightRect);
            }
        } else if (target == -1) {
            drawTargetHighlight(localPlayerRect);
        } else if (target == -2) {
            drawTargetHighlight(opponentPlayerRect);
        }
    }

	std::string targetPrompt = "Choose target for " + (*cardIt)->getName();

	textRenderer.drawText(
		renderer,
		targetPrompt,
		fontSmall,
		Theme::Playing::TARGET_PROMPT_TEXT,
		Theme::Playing::TARGET_PROMPT_X,
		Theme::Playing::TARGET_PROMPT_Y
	);

    SDL_SetRenderDrawBlendMode(renderer, previousBlendMode);
}

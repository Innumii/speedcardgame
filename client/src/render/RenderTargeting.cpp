#include "render/RenderTargeting.hpp"

#include "objects/Card.h"
#include "render/RenderText.hpp"
#include "render/Theme.hpp"
#include "states/Playing.hpp"

#include <algorithm>
#include <string>

void RenderTargeting::drawPendingTargeting(SDL_Renderer* renderer, RenderText& textRenderer, const Playing& playing, TTF_Font* fontSmall) {
	if (!renderer || !fontSmall) return;
	if (!playing.pendingAction.active) return;

	SDL_SetRenderDrawColor(
		renderer,
		Theme::Playing::TARGET_HIGHLIGHT.r,
		Theme::Playing::TARGET_HIGHLIGHT.g,
		Theme::Playing::TARGET_HIGHLIGHT.b,
		Theme::Playing::TARGET_HIGHLIGHT.a
	);
	for (const auto& localSlot : playing.playSlots) {
		SDL_RenderDrawRect(renderer, &localSlot);
	}
	for (const auto& opponentSlot : playing.opponentSlots) {
		SDL_RenderDrawRect(renderer, &opponentSlot);
	}

	std::string targetPrompt = "Choose target";
	if (playing.pendingAction.cardId != static_cast<std::size_t>(-1)) {
		const auto& hand = playing.localPlayer.hand;
		auto it = std::find_if(
			hand.begin(),
			hand.end(),
			[&](const std::unique_ptr<Card>& card) {
				return card && card->getId() == playing.pendingAction.cardId;
			}
		);

		if (it != hand.end() && *it) {
			targetPrompt = "Choose target for " + (*it)->getName();
		}
	}

	textRenderer.drawText(
		renderer,
		targetPrompt,
		fontSmall,
		Theme::Playing::TARGET_PROMPT_TEXT,
		Theme::Playing::TARGET_PROMPT_X,
		Theme::Playing::TARGET_PROMPT_Y
	);
}

#include "utils/PlayingRenderUtil.hpp"

#include "animation/AnimationGroup.hpp"
#include "animation/AttackAnimation.hpp"
#include "animation/DeathAnimation.hpp"
#include "animation/DrawCardAnimation.hpp"
#include "render/RenderCard.hpp"
#include "render/RenderText.hpp"
#include "render/Theme.hpp"
#include "utils/PlayingLayoutUtil.hpp"

#include <algorithm>
#include <memory>
#include <string>

namespace PlayingRenderUtil {
    void collectAttackFrames(const std::shared_ptr<const AnimationInterface>& animation,
                             std::vector<AttackRenderFrame>& frames) {
        if (!animation) {
            return;
        }

        if (const auto attack = std::dynamic_pointer_cast<const AttackAnimation>(animation)) {
            if (!attack->isFinished()) {
                frames.push_back(AttackRenderFrame{
                    attack->getCurrentRect(),
                    attack->getAlpha(),
                    attack->getLane(),
                    attack->isSelfPlayer()
                });
            }
            return;
        }

        const auto group = std::dynamic_pointer_cast<const AnimationGroup>(animation);
        if (!group) {
            return;
        }

        for (const auto& child : group->getAnimations()) {
            collectAttackFrames(child, frames);
        }
    }

    void collectDeathFrames(const std::shared_ptr<const AnimationInterface>& animation,
                            std::vector<DeathRenderFrame>& frames) {
        if (!animation) {
            return;
        }

        if (const auto death = std::dynamic_pointer_cast<const DeathAnimation>(animation)) {
            if (!death->isFinished()) {
                frames.push_back(DeathRenderFrame{death->getRect(), death->getAlpha()});
            }
            return;
        }

        const auto group = std::dynamic_pointer_cast<const AnimationGroup>(animation);
        if (!group) {
            return;
        }

        for (const auto& child : group->getAnimations()) {
            collectDeathFrames(child, frames);
        }
    }

    std::shared_ptr<const DrawCardAnimation> findDrawAnimation(
        const std::shared_ptr<const AnimationInterface>& animation) {
        if (!animation) {
            return nullptr;
        }

        if (const auto draw = std::dynamic_pointer_cast<const DrawCardAnimation>(animation)) {
            return draw;
        }

        const auto group = std::dynamic_pointer_cast<const AnimationGroup>(animation);
        if (!group) {
            return nullptr;
        }

        for (const auto& child : group->getAnimations()) {
            if (const auto draw = findDrawAnimation(child)) {
                return draw;
            }
        }

        return nullptr;
    }

    void drawOpponentDeckAndDiscard(SDL_Renderer* renderer, RenderText& textRenderer,
                                    const std::vector<SDL_Rect>& opponentSlots,
                                    std::size_t deckSize, int screenW, TTF_Font* fontSmall) {
        if (!renderer || !fontSmall || opponentSlots.empty()) {
            return;
        }

        const SDL_Rect opponentDiscard = PlayingLayoutUtil::computeDiscardRect(
            opponentSlots,
            Theme::Playing::CARD_WIDTH,
            Theme::Playing::CARD_HEIGHT,
            Theme::Playing::OPPONENT_SIDE_GAP,
            Theme::Playing::SIDE_ZONE_MARGIN
        );
        const SDL_Rect deckBase = PlayingLayoutUtil::computeDeckRect(
            opponentSlots,
            screenW,
            Theme::Playing::CARD_WIDTH,
            Theme::Playing::CARD_HEIGHT,
            Theme::Playing::OPPONENT_SIDE_GAP,
            Theme::Playing::SIDE_ZONE_MARGIN
        );

        SDL_SetRenderDrawColor(renderer, Theme::Playing::OPPONENT_DISCARD_FILL.r, Theme::Playing::OPPONENT_DISCARD_FILL.g, Theme::Playing::OPPONENT_DISCARD_FILL.b, Theme::Playing::OPPONENT_DISCARD_FILL.a);
        SDL_RenderFillRect(renderer, &opponentDiscard);
        SDL_SetRenderDrawColor(renderer, Theme::Playing::OPPONENT_DISCARD_BORDER.r, Theme::Playing::OPPONENT_DISCARD_BORDER.g, Theme::Playing::OPPONENT_DISCARD_BORDER.b, Theme::Playing::OPPONENT_DISCARD_BORDER.a);
        SDL_RenderDrawRect(renderer, &opponentDiscard);
        textRenderer.drawText(
            renderer,
            "Opponent Discard",
            fontSmall,
            Theme::TEXT_PRIMARY,
            opponentDiscard.x + Theme::Playing::ZONE_TEXT_PADDING,
            opponentDiscard.y + Theme::Playing::ZONE_TEXT_PADDING
        );

        const int stackCount = static_cast<int>(std::min<std::size_t>(deckSize, static_cast<std::size_t>(Theme::Playing::DECK_STACK_MAX_CARDS)));
        for (int i = 0; i < stackCount; ++i) {
            SDL_Rect card{deckBase.x + i * Theme::Playing::DECK_STACK_X_OFFSET, deckBase.y - i * Theme::Playing::DECK_STACK_Y_OFFSET, deckBase.w, deckBase.h};
            RenderCard::drawCardBack(renderer, card);
        }
        textRenderer.drawText(
            renderer,
            "Deck: " + std::to_string(deckSize),
            fontSmall,
            Theme::TEXT_PRIMARY,
            deckBase.x + Theme::Playing::ZONE_TEXT_PADDING,
            deckBase.y + Theme::Playing::ZONE_TEXT_PADDING
        );
    }

    void drawSelfDeck(SDL_Renderer* renderer, RenderText& textRenderer,
                      const std::vector<SDL_Rect>& playSlots,
                      int deckSize, int screenW, TTF_Font* fontSmall) {
        if (!renderer || !fontSmall || playSlots.empty()) {
            return;
        }

        const SDL_Rect deckBase = PlayingLayoutUtil::computeDeckRect(
            playSlots,
            screenW,
            Theme::Playing::CARD_WIDTH,
            Theme::Playing::CARD_HEIGHT,
            Theme::Playing::SELF_DECK_GAP,
            Theme::Playing::SIDE_ZONE_MARGIN
        );

        const int stackCount = std::min(deckSize, Theme::Playing::DECK_STACK_MAX_CARDS);
        for (int i = 0; i < stackCount; ++i) {
            SDL_Rect card{deckBase.x + i * Theme::Playing::DECK_STACK_X_OFFSET, deckBase.y - i * Theme::Playing::DECK_STACK_Y_OFFSET, deckBase.w, deckBase.h};
            RenderCard::drawCardBack(renderer, card);
        }
        textRenderer.drawText(
            renderer,
            "Deck: " + std::to_string(deckSize),
            fontSmall,
            Theme::TEXT_PRIMARY,
            deckBase.x + Theme::Playing::ZONE_TEXT_PADDING,
            deckBase.y + Theme::Playing::ZONE_TEXT_PADDING
        );
    }
}

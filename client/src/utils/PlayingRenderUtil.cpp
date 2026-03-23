#include "utils/PlayingRenderUtil.hpp"

#include "animation/AnimationGroup.hpp"
#include "animation/AttackAnimation.hpp"
#include "animation/DeathAnimation.hpp"
#include "animation/DrawCardAnimation.hpp"
#include "render/RenderCard.hpp"
#include "render/RenderText.hpp"
#include "utils/RenderUtil.hpp"
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
                                    std::size_t deckSize, int screenW, int screenH,
                                    TTF_Font* fontSmall) {
        if (!renderer || !fontSmall || opponentSlots.empty()) {
            return;
        }

        const float scale = std::min(
            static_cast<float>(screenW) / 1200.0F,
            static_cast<float>(screenH) / 850.0F);

        const int cardW       = static_cast<int>(Theme::Playing::CARD_WIDTH        * scale);
        const int cardH       = static_cast<int>(Theme::Playing::CARD_HEIGHT       * scale);
        const int sideGap     = static_cast<int>(Theme::Playing::OPPONENT_SIDE_GAP * scale);
        const int sideMargin  = static_cast<int>(Theme::Playing::SIDE_ZONE_MARGIN  * scale);
        const int textPad     = static_cast<int>(Theme::Playing::ZONE_TEXT_PADDING * scale);
        const int stackXOff   = static_cast<int>(Theme::Playing::DECK_STACK_X_OFFSET * scale);
        const int stackYOff   = static_cast<int>(Theme::Playing::DECK_STACK_Y_OFFSET * scale);
        const int stackMax    = Theme::Playing::DECK_STACK_MAX_CARDS;
        const int cornerRadius = std::max(2, static_cast<int>(Theme::Board::DISCARD_CORNER_RADIUS * scale));

        const SDL_Rect opponentDiscard = PlayingLayoutUtil::computeDiscardRect(
            opponentSlots,
            cardW,
            cardH,
            sideGap,
            sideMargin
        );
        const SDL_Rect deckBase = PlayingLayoutUtil::computeDeckRect(
            opponentSlots,
            screenW,
            cardW,
            cardH,
            sideGap,
            sideMargin
        );

        // Rounded discard zone — matches local player style
        RenderUtil::drawRoundedRect(
            renderer,
            opponentDiscard,
            cornerRadius,
            Theme::Playing::OPPONENT_DISCARD_FILL,
            Theme::Playing::OPPONENT_DISCARD_BORDER
        );
        textRenderer.drawText(
            renderer,
            "Opponent Discard",
            fontSmall,
            Theme::TEXT_PRIMARY,
            opponentDiscard.x + textPad,
            opponentDiscard.y + textPad
        );

        const int stackCount = static_cast<int>(
            std::min<std::size_t>(deckSize, static_cast<std::size_t>(stackMax)));
        for (int i = 0; i < stackCount; ++i) {
            SDL_Rect card{
                deckBase.x + i * stackXOff,
                deckBase.y - i * stackYOff,
                deckBase.w,
                deckBase.h
            };
            RenderCard::drawCardBack(renderer, card);
        }
        textRenderer.drawText(
            renderer,
            "Deck: " + std::to_string(deckSize),
            fontSmall,
            Theme::TEXT_PRIMARY,
            deckBase.x + textPad,
            deckBase.y + textPad
        );
    }

    void drawSelfDeck(SDL_Renderer* renderer, RenderText& textRenderer,
                      const std::vector<SDL_Rect>& playSlots,
                      int deckSize, int screenW, int screenH, TTF_Font* fontSmall) {
        if (!renderer || !fontSmall || playSlots.empty()) {
            return;
        }

        const float scale = std::min(
            static_cast<float>(screenW) / 1200.0F,
            static_cast<float>(screenH) / 850.0F);

        const int cardW      = static_cast<int>(Theme::Playing::CARD_WIDTH          * scale);
        const int cardH      = static_cast<int>(Theme::Playing::CARD_HEIGHT         * scale);
        const int deckGap    = static_cast<int>(Theme::Playing::SELF_DECK_GAP       * scale);
        const int sideMargin = static_cast<int>(Theme::Playing::SIDE_ZONE_MARGIN    * scale);
        const int textPad    = static_cast<int>(Theme::Playing::ZONE_TEXT_PADDING   * scale);
        const int stackXOff  = static_cast<int>(Theme::Playing::DECK_STACK_X_OFFSET * scale);
        const int stackYOff  = static_cast<int>(Theme::Playing::DECK_STACK_Y_OFFSET * scale);
        const int stackMax   = Theme::Playing::DECK_STACK_MAX_CARDS;

        const SDL_Rect deckBase = PlayingLayoutUtil::computeDeckRect(
            playSlots,
            screenW,
            cardW,
            cardH,
            deckGap,
            sideMargin
        );

        const int stackCount = std::min(deckSize, stackMax);
        for (int i = 0; i < stackCount; ++i) {
            SDL_Rect card{
                deckBase.x + i * stackXOff,
                deckBase.y - i * stackYOff,
                deckBase.w,
                deckBase.h
            };
            RenderCard::drawCardBack(renderer, card);
        }
        textRenderer.drawText(
            renderer,
            "Deck: " + std::to_string(deckSize),
            fontSmall,
            Theme::TEXT_PRIMARY,
            deckBase.x + textPad,
            deckBase.y + textPad
        );
    }
}
#include "render/RenderCard.hpp"

#include "objects/Card.h"
#include "objects/CreatureCard.h"
#include "render/RenderText.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <algorithm>
#include <string>

namespace {
    constexpr SDL_Color kCardTextColor{0, 0, 0, 255};
}

void RenderCard::drawHandCard(SDL_Renderer* renderer, RenderText& textRenderer, const Card& card, const SDL_Rect& cardRect, TTF_Font* fontSmall) {
    if (!renderer || !fontSmall) return;

    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
    SDL_RenderFillRect(renderer, &cardRect);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderDrawRect(renderer, &cardRect);

    const int textMargin = 10;
    textRenderer.drawText(renderer, card.getName(), fontSmall, kCardTextColor, cardRect.x + textMargin, cardRect.y + textMargin);

    const std::string costText = "Cost: " + std::to_string(card.getManaCost());
    int costW = 0, costH = 0;
    TTF_SizeText(fontSmall, costText.c_str(), &costW, &costH);
    textRenderer.drawText(renderer, costText, fontSmall, kCardTextColor, cardRect.x + cardRect.w - costW - textMargin, cardRect.y + textMargin);

    textRenderer.drawText(
        renderer,
        "Value: " + std::to_string(card.getManaValue()),
        fontSmall,
        kCardTextColor,
        cardRect.x + textMargin,
        cardRect.y + textMargin + 22
    );

    textRenderer.drawWrappedText(
        renderer,
        card.getText(),
        fontSmall,
        kCardTextColor,
        cardRect.x + textMargin,
        cardRect.y + textMargin + 48,
        18
    );

    if (card.getType() == CardType::Creature) {
        const auto* creature = dynamic_cast<const CreatureCard*>(&card);
        if (creature) {
            const std::string statsText =
                std::to_string(creature->getPower()) + "/" +
                std::to_string(creature->getToughness());

            int statsW = 0, statsH = 0;
            TTF_SizeText(fontSmall, statsText.c_str(), &statsW, &statsH);

            textRenderer.drawText(
                renderer,
                statsText,
                fontSmall,
                kCardTextColor,
                cardRect.x + cardRect.w - statsW - 12,
                cardRect.y + cardRect.h - statsH - 12
            );
        }
    }
}

void RenderCard::drawBoardCard(SDL_Renderer* renderer, RenderText& textRenderer, const Card& card, const SDL_Rect& cardRect, TTF_Font* fontSmall) {
    if (!renderer || !fontSmall) return;

    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
    SDL_RenderFillRect(renderer, &cardRect);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderDrawRect(renderer, &cardRect);

    const int textMargin = 10;
    textRenderer.drawText(renderer, card.getName(), fontSmall, kCardTextColor, cardRect.x + textMargin, cardRect.y + textMargin);
    textRenderer.drawText(
        renderer,
        "Cost: " + std::to_string(card.getManaCost()),
        fontSmall,
        kCardTextColor,
        cardRect.x + cardRect.w - 40,
        cardRect.y + textMargin
    );
    textRenderer.drawText(
        renderer,
        "Value: " + std::to_string(card.getManaValue()),
        fontSmall,
        kCardTextColor,
        cardRect.x + textMargin,
        cardRect.y + textMargin + 22
    );
}

void RenderCard::drawPreview(SDL_Renderer* renderer, RenderText& textRenderer, const Card& card, const SDL_Rect& previewRect, TTF_Font* fontSmall, TTF_Font* fontLarge) {
    if (!renderer || !fontSmall || !fontLarge) return;

    const int padding = 12;

    SDL_SetRenderDrawColor(renderer, 45, 45, 60, 245);
    SDL_RenderFillRect(renderer, &previewRect);
    SDL_SetRenderDrawColor(renderer, 220, 220, 240, 255);
    SDL_RenderDrawRect(renderer, &previewRect);

    textRenderer.drawText(renderer, card.getName(), fontLarge, SDL_Color{255, 255, 255, 255}, previewRect.x + padding, previewRect.y + padding);

    const std::string costText = "Cost: " + std::to_string(card.getManaCost());
    int costW = 0, costH = 0;
    TTF_SizeText(fontSmall, costText.c_str(), &costW, &costH);
    textRenderer.drawText(renderer, costText, fontSmall, SDL_Color{230, 230, 230, 255}, previewRect.x + previewRect.w - padding - costW, previewRect.y + padding);

    textRenderer.drawText(
        renderer,
        "Value: " + std::to_string(card.getManaValue()),
        fontSmall,
        SDL_Color{230, 230, 230, 255},
        previewRect.x + padding,
        previewRect.y + padding + 28
    );

    const int descY = previewRect.y + padding + 52;
    const int wrapWidth = std::max(16, (previewRect.w - 2 * padding) / 7);
    textRenderer.drawWrappedText(
        renderer,
        card.getText(),
        fontSmall,
        SDL_Color{220, 220, 220, 255},
        previewRect.x + padding,
        descY,
        static_cast<std::size_t>(wrapWidth)
    );

    if (card.getType() == CardType::Creature) {
        const auto* creature = dynamic_cast<const CreatureCard*>(&card);
        if (creature) {
            const std::string statsText =
                std::to_string(creature->getPower()) + "/" +
                std::to_string(creature->getToughness());

            int statsW = 0, statsH = 0;
            TTF_SizeText(fontSmall, statsText.c_str(), &statsW, &statsH);

            textRenderer.drawText(
                renderer,
                statsText,
                fontSmall,
                SDL_Color{230, 230, 230, 255},
                previewRect.x + previewRect.w - padding - statsW,
                previewRect.y + previewRect.h - padding - statsH
            );
        }
    }
}
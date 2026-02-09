#include "render/RenderDeckBuilding.hpp"

#include "core/Game.hpp"
#include "states/DeckBuilding.hpp"
#include "objects/Card.h"
#include "render/RenderCard.hpp"
#include "render/RenderText.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <algorithm>
#include <string>
#include <vector>

void RenderDeckBuilding::render(DeckBuilding& deckBuilding, Game& game) {
    SDL_Renderer* renderer = game.getRenderer();
    if (!renderer) return;

    if (!TTF_WasInit() && TTF_Init() != 0) {
        return;
    }

    RenderText textRenderer;
    const auto layout = deckBuilding.buildLayout(game);

    // Title Button
    TTF_Font* font = TTF_OpenFont("assets/font.TTF", 18);
    textRenderer.drawText(
        renderer,
        "Return to Title",
        font,
        SDL_Color{255, 255, 255, 255},
        deckBuilding.TitleButton.x + 10,
        deckBuilding.TitleButton.y + 10
    );

    if (font) {
        TTF_CloseFont(font);
    }

    TTF_Font* fontSmall = TTF_OpenFont("assets/font.TTF", 14);
    TTF_Font* fontTiny = TTF_OpenFont("assets/font.TTF", 12);

    SDL_SetRenderDrawColor(renderer, 40, 40, 50, 255);
    SDL_RenderFillRect(renderer, &layout.collectionArea);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &layout.collectionArea);

    if (fontSmall) {
        textRenderer.drawText(
            renderer,
            "Collection",
            fontSmall,
            SDL_Color{255, 255, 255, 255},
            layout.collectionArea.x + 10,
            layout.collectionArea.y + 10
        );
    }

    for (std::size_t i = 0; i < layout.collectionCardRects.size(); ++i) {
        const SDL_Rect cardRect = layout.collectionCardRects[i];
        const Card& card = *deckBuilding.availableCards[i];

        SDL_SetRenderDrawColor(renderer, 180, 180, 220, 255);
        SDL_RenderFillRect(renderer, &cardRect);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderDrawRect(renderer, &cardRect);

        if (fontSmall) {
            textRenderer.drawText(renderer, card.getName(), fontSmall, SDL_Color{20, 20, 20, 255}, cardRect.x + 6, cardRect.y + 6);
            textRenderer.drawText(renderer, "Cost: " + std::to_string(card.getManaCost()), fontTiny, SDL_Color{20, 20, 20, 255}, cardRect.x + 6, cardRect.y + 28);
        }
    }

    SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
    SDL_RenderFillRect(renderer, &layout.deckArea);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &layout.deckArea);

    if (fontSmall) {
        textRenderer.drawText(
            renderer,
            "Deck",
            fontSmall,
            SDL_Color{255, 255, 255, 255},
            layout.deckArea.x + 10,
            layout.deckArea.y + 10
        );
    }

    for (std::size_t i = 0; i < layout.deckEntryRects.size(); ++i) {
        const SDL_Rect entryRect = layout.deckEntryRects[i];
        const int cardIndex = layout.deckEntryCardIndices[i];
        const Card& card = *deckBuilding.availableCards[cardIndex];
        const int copies = deckBuilding.deckCopies[cardIndex];

        SDL_SetRenderDrawColor(renderer, 70, 70, 90, 255);
        SDL_RenderFillRect(renderer, &entryRect);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderDrawRect(renderer, &entryRect);

        if (fontTiny) {
            textRenderer.drawText(renderer, card.getName(), fontTiny, SDL_Color{255, 255, 255, 255}, entryRect.x + 6, entryRect.y + 6);
            textRenderer.drawText(renderer, "Cost: " + std::to_string(card.getManaCost()), fontTiny, SDL_Color{200, 200, 200, 255}, entryRect.x + 130, entryRect.y + 6);
            textRenderer.drawText(renderer, "x" + std::to_string(copies), fontTiny, SDL_Color{255, 255, 255, 255}, entryRect.x + entryRect.w - 30, entryRect.y + 6);
        }
    }

    if (deckBuilding.dragging && deckBuilding.draggedCardIndex >= 0 && deckBuilding.draggedCardIndex < static_cast<int>(deckBuilding.availableCards.size())) {
        const Card& card = *deckBuilding.availableCards[deckBuilding.draggedCardIndex];
        SDL_Rect dragRect{
            deckBuilding.dragPos.x - deckBuilding.dragOffset.x,
            deckBuilding.dragPos.y - deckBuilding.dragOffset.y,
            110,
            150
        };

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 200, 200, 230, 200);
        SDL_RenderFillRect(renderer, &dragRect);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderDrawRect(renderer, &dragRect);

        if (fontSmall) {
            textRenderer.drawText(renderer, card.getName(), fontSmall, SDL_Color{20, 20, 20, 255}, dragRect.x + 6, dragRect.y + 6);
            textRenderer.drawText(renderer, "Cost: " + std::to_string(card.getManaCost()), fontTiny, SDL_Color{20, 20, 20, 255}, dragRect.x + 6, dragRect.y + 28);
        }

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    }

    if (fontSmall) {
        TTF_CloseFont(fontSmall);
    }
    if (fontTiny) {
        TTF_CloseFont(fontTiny);
    }
}
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

    const bool canPlay = deckBuilding.hasCardsInDeck();
    SDL_SetRenderDrawColor(renderer, canPlay ? 80 : 60, canPlay ? 200 : 60, canPlay ? 120 : 60, 255);
    SDL_RenderFillRect(renderer, &deckBuilding.PlayButton);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &deckBuilding.PlayButton);

    textRenderer.drawText(
        renderer,
        "Play",
        font,
        SDL_Color{255, 255, 255, 255},
        deckBuilding.PlayButton.x + 10,
        deckBuilding.PlayButton.y + 10
    );

    if (font) {
        TTF_CloseFont(font);
    }

    TTF_Font* fontSmall = TTF_OpenFont("assets/font.TTF", 14);
    TTF_Font* fontTiny = TTF_OpenFont("assets/font.TTF", 12);
    TTF_Font* fontLarge = TTF_OpenFont("assets/font.TTF", 18);

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
        if (i >= layout.collectionCardIndices.size()) continue;
        const int cardIndex = layout.collectionCardIndices[i];
        if (cardIndex < 0 || cardIndex >= static_cast<int>(deckBuilding.availableCards.size())) continue;
        const Card& card = *deckBuilding.availableCards[cardIndex];

        RenderCard::drawCardFace(renderer, textRenderer, card, cardRect, fontSmall, fontTiny, false);
    }

    if (layout.pageCount > 1) {
        SDL_SetRenderDrawColor(renderer, 60, 60, 70, 255);
        SDL_RenderFillRect(renderer, &layout.prevPageButton);
        SDL_RenderFillRect(renderer, &layout.nextPageButton);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderDrawRect(renderer, &layout.prevPageButton);
        SDL_RenderDrawRect(renderer, &layout.nextPageButton);

        if (fontTiny) {
            textRenderer.drawText(renderer, "Prev", fontTiny, SDL_Color{255, 255, 255, 255}, layout.prevPageButton.x + 12, layout.prevPageButton.y + 4);
            textRenderer.drawText(renderer, "Next", fontTiny, SDL_Color{255, 255, 255, 255}, layout.nextPageButton.x + 12, layout.nextPageButton.y + 4);

            const std::string pageText = "Page " + std::to_string(layout.pageIndex + 1) + " / " + std::to_string(layout.pageCount);
            textRenderer.drawText(renderer, pageText, fontTiny, SDL_Color{230, 230, 230, 255}, layout.pageLabelRect.x + 6, layout.pageLabelRect.y + 4);
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
        int dragW = 110;
        int dragH = 150;
        if (!layout.collectionCardRects.empty()) {
            dragW = layout.collectionCardRects.front().w;
            dragH = layout.collectionCardRects.front().h;
        }

        SDL_Rect dragRect{
            deckBuilding.dragPos.x - deckBuilding.dragOffset.x,
            deckBuilding.dragPos.y - deckBuilding.dragOffset.y,
            dragW,
            dragH
        };

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        RenderCard::drawCardFace(renderer, textRenderer, card, dragRect, fontSmall, fontTiny, false);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    }

    const Uint32 now = SDL_GetTicks();
    int mouseX = 0;
    int mouseY = 0;
    SDL_GetMouseState(&mouseX, &mouseY);
    const SDL_Point mousePoint{mouseX, mouseY};

    std::size_t newHoverIndex = static_cast<std::size_t>(-1);
    if (!deckBuilding.dragging) {
        for (std::size_t i = 0; i < layout.collectionCardRects.size(); ++i) {
            if (SDL_PointInRect(&mousePoint, &layout.collectionCardRects[i]) == SDL_TRUE) {
                if (i < layout.collectionCardIndices.size()) {
                    newHoverIndex = static_cast<std::size_t>(layout.collectionCardIndices[i]);
                } else {
                    newHoverIndex = i;
                }
                break;
            }
        }

        if (newHoverIndex == static_cast<std::size_t>(-1)) {
            for (std::size_t i = 0; i < layout.deckEntryRects.size(); ++i) {
                if (SDL_PointInRect(&mousePoint, &layout.deckEntryRects[i]) == SDL_TRUE) {
                    newHoverIndex = static_cast<std::size_t>(layout.deckEntryCardIndices[i]);
                    break;
                }
            }
        }
    }

    if (newHoverIndex != deckBuilding.hoverIndex) {
        deckBuilding.hoverIndex = newHoverIndex;
        deckBuilding.hoverStartTick = now;
    }

    constexpr Uint32 hoverDelayMs = 350;
    const bool showPreview =
        deckBuilding.hoverIndex != static_cast<std::size_t>(-1) &&
        deckBuilding.hoverIndex < deckBuilding.availableCards.size() &&
        now - deckBuilding.hoverStartTick >= hoverDelayMs;

    if (showPreview) {
        int screenW = 800;
        int screenH = 600;
        SDL_GetRendererOutputSize(renderer, &screenW, &screenH);

        int previewWidth = std::min(360, screenW / 2);
        int previewHeight = static_cast<int>(previewWidth * 1.4f);
        if (previewHeight > screenH - 40) {
            previewHeight = screenH - 40;
            previewWidth = static_cast<int>(previewHeight / 1.4f);
        }

        int previewX = layout.deckArea.x - previewWidth - 16;
        if (previewX < 20) previewX = 20;
        int previewY = layout.deckArea.y;
        if (previewY + previewHeight > screenH - 20) {
            previewY = screenH - previewHeight - 20;
        }
        if (previewY < 20) previewY = 20;

        SDL_Rect panel{previewX, previewY, previewWidth, previewHeight};
        RenderCard::drawCardFace(renderer, textRenderer, *deckBuilding.availableCards[deckBuilding.hoverIndex], panel, fontLarge ? fontLarge : fontSmall, fontSmall, false);
    }

    if (fontSmall) {
        TTF_CloseFont(fontSmall);
    }
    if (fontTiny) {
        TTF_CloseFont(fontTiny);
    }
    if (fontLarge) {
        TTF_CloseFont(fontLarge);
    }
}
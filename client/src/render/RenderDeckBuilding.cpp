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
#include <render/RenderButton.hpp>

void RenderDeckBuilding::render(DeckBuilding& deckBuilding, Game& game) {
    SDL_Renderer* renderer = game.getRenderer();
    if (!renderer) return;

    RenderText textRenderer;
    const auto layout = deckBuilding.buildLayout(game);
    int mouseX = 0;
    int mouseY = 0;
    SDL_GetMouseState(&mouseX, &mouseY);
    const SDL_Point mousePoint{mouseX, mouseY};

    RenderText::FontSet fonts = RenderText::loadFonts("assets/font.TTF", 14, 12, 18);
    TTF_Font* fontSmall = fonts.small;
    TTF_Font* fontTiny = fonts.tiny;
    TTF_Font* fontLarge = fonts.large;

    TTF_Font* buttonFont = fontSmall ? fontSmall : fontLarge;
    const SDL_Color buttonBase{80, 120, 200, 255};
    const SDL_Color buttonHighlight{100, 150, 250, 255};
    const SDL_Color buttonPressed{60, 90, 180, 255};
    const SDL_Color buttonText{255, 255, 255, 255};

    // Title Button
    const bool canPlay = deckBuilding.hasCardsInDeck();
    RenderButton::drawButton(
        renderer,
        deckBuilding.PlayButton,
        "Play",
        false,
        !canPlay,
        buttonBase,
        buttonHighlight,
        buttonPressed,
        buttonText,
        buttonFont
    );

    RenderButton::drawButton(
        renderer,
        deckBuilding.TitleButton,
        "Return to Title",
        false,
        false,
        buttonBase,
        buttonHighlight,
        buttonPressed,
        buttonText,
        buttonFont
    );

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
        const SDL_Color pagerBase{60, 60, 70, 255};
        const SDL_Color pagerHighlight{80, 80, 95, 255};
        const SDL_Color pagerPressed{40, 40, 55, 255};
        const SDL_Color pagerText{255, 255, 255, 255};
        const SDL_Color pagerDisabled{140, 140, 140, 255};

        const bool canPrev = layout.pageIndex > 0;
        const bool canNext = layout.pageIndex < layout.pageCount - 1;
        const bool hoverPrev = canPrev && SDL_PointInRect(&mousePoint, &layout.prevPageButton) == SDL_TRUE;
        const bool hoverNext = canNext && SDL_PointInRect(&mousePoint, &layout.nextPageButton) == SDL_TRUE;

        RenderButton::drawButton(
            renderer,
            layout.prevPageButton,
            "Prev",
            hoverPrev,
            false,
            canPrev ? pagerBase : pagerPressed,
            canPrev ? pagerHighlight : pagerPressed,
            pagerPressed,
            canPrev ? pagerText : pagerDisabled,
            fontTiny
        );

        RenderButton::drawButton(
            renderer,
            layout.nextPageButton,
            "Next",
            hoverNext,
            false,
            canNext ? pagerBase : pagerPressed,
            canNext ? pagerHighlight : pagerPressed,
            pagerPressed,
            canNext ? pagerText : pagerDisabled,
            fontTiny
        );

        if (fontTiny) {
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

    RenderText::closeFonts(fonts);
}
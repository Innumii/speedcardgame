#include "render/RenderDeckBuilding.hpp"

#include "core/Game.hpp"
#include "states/DeckBuilding.hpp"
#include "objects/Card.h"
#include "objects/Deck.h"
#include "objects/Inventory.hpp"
#include "render/RenderCard.hpp"
#include "render/RenderText.hpp"
#include "render/RenderButton.hpp"
#include "render/Theme.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <algorithm>
#include <string>
#include <vector>

void RenderDeckBuilding::render(DeckBuilding& deckBuilding, Game& game) {
    SDL_Renderer* renderer = game.getRenderer();
    if (!renderer) return;

    RenderText textRenderer;
    auto layout = deckBuilding.buildLayout(game);
    if (deckBuilding.deckScrollOffset < 0) {
        deckBuilding.deckScrollOffset = 0;
        layout = deckBuilding.buildLayout(game);
    } else if (deckBuilding.deckScrollOffset > layout.maxDeckScrollOffset) {
        deckBuilding.deckScrollOffset = layout.maxDeckScrollOffset;
        layout = deckBuilding.buildLayout(game);
    }
    int mouseX = 0;
    int mouseY = 0;
    SDL_GetMouseState(&mouseX, &mouseY);
    const SDL_Point mousePoint{mouseX, mouseY};
    const Uint32 now = SDL_GetTicks();

    // ── fonts ────────────────────────────────────────────────────────
    const RenderText::FontSet& fonts = game.getUIFonts();
    TTF_Font* fontSmall = fonts.small;
    TTF_Font* fontTiny  = fonts.tiny;
    TTF_Font* fontLarge = fonts.large;
    TTF_Font* buttonFont = fontSmall ? fontSmall : fontLarge;

    // ── menu buttons ─────────────────────────────────────────────────
    const bool canPlay = Deck::hasFullDeck(deckBuilding.deckCopies);
    const bool canSave = Deck::hasFullDeck(deckBuilding.deckCopies);
    const bool hoverTitle = SDL_PointInRect(&mousePoint, &deckBuilding.TitleButton) == SDL_TRUE;
    const bool hoverPlay = SDL_PointInRect(&mousePoint, &deckBuilding.PlayButton) == SDL_TRUE;
    const bool hoverSave = SDL_PointInRect(&mousePoint, &deckBuilding.SaveButton) == SDL_TRUE;



    RenderButton::drawButton(renderer, deckBuilding.PlayButton,
                              "Play", buttonFont,
                              canPlay ? Theme::BTN_START     : Theme::BTN_SECONDARY,
                              Theme::BTN_BORDER, Theme::BTN_TEXT,
                              hoverPlay, !canPlay);

    RenderButton::drawButton(renderer, deckBuilding.SaveButton,
                              "Save Deck", buttonFont,
                              canSave ? Theme::BTN_BUILD     : Theme::BTN_SECONDARY,
                              Theme::BTN_BORDER, Theme::BTN_TEXT,
                              hoverSave, !canSave);

    RenderButton::drawButton(renderer, deckBuilding.TitleButton,
                              "Return to Title", buttonFont,
                              Theme::BTN_CONNECT,
                              Theme::BTN_BORDER, Theme::BTN_TEXT,
                              hoverTitle, false);

    if (fontSmall && deckBuilding.isStatusMessageActive(now)) {
        int msgX = layout.collectionArea.x;
        int msgY = layout.collectionArea.y - Theme::DeckBuilding::STATUS_MSG_TOP_OFFSET;
        if (msgY < Theme::DeckBuilding::STATUS_MSG_MIN_Y) {
            msgY = layout.collectionArea.y + Theme::DeckBuilding::STATUS_MSG_FALLBACK_Y_OFFSET;
        }
        textRenderer.drawText(
            renderer,
            deckBuilding.getStatusMessage(),
            fontSmall,
            Theme::DeckBuilding::STATUS_ERROR_TEXT,
            msgX,
            msgY
        );
    }

    SDL_SetRenderDrawColor(renderer,
                           Theme::DeckBuilding::COLLECTION_FILL.r,
                           Theme::DeckBuilding::COLLECTION_FILL.g,
                           Theme::DeckBuilding::COLLECTION_FILL.b,
                           Theme::DeckBuilding::COLLECTION_FILL.a);
    SDL_RenderFillRect(renderer, &layout.collectionArea);
    SDL_SetRenderDrawColor(renderer,
                           Theme::DeckBuilding::COLLECTION_BORDER.r,
                           Theme::DeckBuilding::COLLECTION_BORDER.g,
                           Theme::DeckBuilding::COLLECTION_BORDER.b,
                           Theme::DeckBuilding::COLLECTION_BORDER.a);
    SDL_RenderDrawRect(renderer, &layout.collectionArea);

    if (fontSmall) {
        textRenderer.drawText(
            renderer,
            "Collection",
            fontSmall,
            Theme::DeckBuilding::COLLECTION_TITLE_TEXT,
            layout.collectionArea.x + Theme::DeckBuilding::SECTION_TITLE_X_OFFSET,
            layout.collectionArea.y + Theme::DeckBuilding::SECTION_TITLE_Y_OFFSET
        );
    }

    for (std::size_t i = 0; i < layout.collectionCardRects.size(); ++i) {
        const SDL_Rect cardRect = layout.collectionCardRects[i];
        if (i >= layout.collectionCardIndices.size()) continue;
        const int cardIndex = layout.collectionCardIndices[i];
        if (cardIndex < 0 || cardIndex >= static_cast<int>(deckBuilding.availableCards.size())) continue;
        const Card& card = *deckBuilding.availableCards[cardIndex];

        const int remaining = Inventory::getRemainingCount(
            deckBuilding.inventoryCopies,
            deckBuilding.inventoryLoaded,
            deckBuilding.deckCopies,
            cardIndex,
            Deck::getDeckCopiesLimit()
        );
        const bool dimmed = remaining <= 0;
        RenderCard::drawCardFace(renderer, textRenderer, card, cardRect, fontSmall, fontTiny, dimmed);

        if (fontTiny) {
            const std::string qtyText = "x" + std::to_string(remaining);
            const SDL_Color qtyColor = dimmed ? Theme::DeckBuilding::QUANTITY_TEXT_DIM : Theme::DeckBuilding::QUANTITY_TEXT;
            textRenderer.drawText(renderer, qtyText, fontTiny, qtyColor, cardRect.x + Theme::DeckBuilding::CARD_QTY_X_OFFSET, cardRect.y + cardRect.h + Theme::DeckBuilding::CARD_QTY_Y_OFFSET);
        }
    }

    if (layout.pageCount > 1) {
        const bool canPrev = layout.pageIndex > 0;
        const bool canNext = layout.pageIndex < layout.pageCount - 1;
        const bool hoverPrev = canPrev && SDL_PointInRect(&mousePoint, &layout.prevPageButton) == SDL_TRUE;
        const bool hoverNext = canNext && SDL_PointInRect(&mousePoint, &layout.nextPageButton) == SDL_TRUE;

        RenderButton::drawButton(renderer, layout.prevPageButton,
                                  "Prev", fontTiny,
                                  canPrev ? Theme::BTN_CONNECT : Theme::BTN_SECONDARY,
                                  Theme::BTN_BORDER,
                                  canPrev ? Theme::DeckBuilding::PAGER_TEXT : Theme::DeckBuilding::PAGER_DISABLED_TEXT,
                                  hoverPrev, !canPrev);

        RenderButton::drawButton(renderer, layout.nextPageButton,
                                  "Next", fontTiny,
                                  canNext ? Theme::BTN_CONNECT : Theme::BTN_SECONDARY,
                                  Theme::BTN_BORDER,
                                  canNext ? Theme::DeckBuilding::PAGER_TEXT : Theme::DeckBuilding::PAGER_DISABLED_TEXT,
                                  hoverNext, !canNext);

        if (fontTiny) {
            const std::string pageText = "Page " + std::to_string(layout.pageIndex + 1) + " / " + std::to_string(layout.pageCount);
            textRenderer.drawText(renderer, pageText, fontTiny, Theme::DeckBuilding::PAGE_LABEL_TEXT, layout.pageLabelRect.x + Theme::DeckBuilding::PAGE_LABEL_X_OFFSET, layout.pageLabelRect.y + Theme::DeckBuilding::PAGE_LABEL_Y_OFFSET);
        }
    }

    SDL_SetRenderDrawColor(renderer,
                           Theme::DeckBuilding::DECK_FILL.r,
                           Theme::DeckBuilding::DECK_FILL.g,
                           Theme::DeckBuilding::DECK_FILL.b,
                           Theme::DeckBuilding::DECK_FILL.a);
    SDL_RenderFillRect(renderer, &layout.deckArea);
    SDL_SetRenderDrawColor(renderer,
                           Theme::DeckBuilding::DECK_BORDER.r,
                           Theme::DeckBuilding::DECK_BORDER.g,
                           Theme::DeckBuilding::DECK_BORDER.b,
                           Theme::DeckBuilding::DECK_BORDER.a);
    SDL_RenderDrawRect(renderer, &layout.deckArea);

    if (fontSmall) {
        const int deckCount = Deck::getDeckCardCount(deckBuilding.deckCopies);
        const int deckLimit = Deck::getDeckSizeLimit();
        const std::string deckCountText = "Deck " + std::to_string(deckCount) + "/" + std::to_string(deckLimit);
        textRenderer.drawText(
            renderer,
            deckCountText,
            fontSmall,
            Theme::DeckBuilding::DECK_COUNT_TEXT,
            layout.deckArea.x + Theme::DeckBuilding::SECTION_TITLE_X_OFFSET,
            layout.deckArea.y + Theme::DeckBuilding::SECTION_TITLE_Y_OFFSET
        );
    }

    SDL_RenderSetClipRect(renderer, &layout.deckEntriesClipRect);
    for (std::size_t i = 0; i < layout.deckEntryRects.size(); ++i) {
        const SDL_Rect entryRect = layout.deckEntryRects[i];
        if (entryRect.y + entryRect.h <= layout.deckEntriesClipRect.y ||
            entryRect.y >= layout.deckEntriesClipRect.y + layout.deckEntriesClipRect.h) {
            continue;
        }

        const int cardIndex = layout.deckEntryCardIndices[i];
        const Card& card = *deckBuilding.availableCards[cardIndex];
        const int copies = deckBuilding.deckCopies[cardIndex];

        SDL_SetRenderDrawColor(renderer,
                               Theme::DeckBuilding::ENTRY_FILL.r,
                               Theme::DeckBuilding::ENTRY_FILL.g,
                               Theme::DeckBuilding::ENTRY_FILL.b,
                               Theme::DeckBuilding::ENTRY_FILL.a);
        SDL_RenderFillRect(renderer, &entryRect);
        SDL_SetRenderDrawColor(renderer,
                               Theme::DeckBuilding::ENTRY_BORDER.r,
                               Theme::DeckBuilding::ENTRY_BORDER.g,
                               Theme::DeckBuilding::ENTRY_BORDER.b,
                               Theme::DeckBuilding::ENTRY_BORDER.a);
        SDL_RenderDrawRect(renderer, &entryRect);

        if (fontTiny) {
            textRenderer.drawText(renderer, card.getName(), fontTiny, Theme::DeckBuilding::ENTRY_NAME_TEXT, entryRect.x + Theme::DeckBuilding::ENTRY_TEXT_X_OFFSET, entryRect.y + Theme::DeckBuilding::ENTRY_TEXT_Y_OFFSET);
            textRenderer.drawText(renderer, "Cost: " + std::to_string(card.getManaCost()), fontTiny, Theme::DeckBuilding::ENTRY_COST_TEXT, entryRect.x + Theme::DeckBuilding::ENTRY_COST_X_OFFSET, entryRect.y + Theme::DeckBuilding::ENTRY_TEXT_Y_OFFSET);
            textRenderer.drawText(renderer, "x" + std::to_string(copies), fontTiny, Theme::DeckBuilding::ENTRY_COUNT_TEXT, entryRect.x + entryRect.w - Theme::DeckBuilding::ENTRY_COUNT_X_RIGHT_INSET, entryRect.y + Theme::DeckBuilding::ENTRY_TEXT_Y_OFFSET);
        }
    }
    SDL_RenderSetClipRect(renderer, nullptr);

    if (layout.maxDeckScrollOffset > 0 && layout.deckEntriesClipRect.h > 0) {
        const SDL_Rect trackRect{
            layout.deckArea.x + layout.deckArea.w - Theme::DeckBuilding::SCROLLBAR_WIDTH - 4,
            layout.deckEntriesClipRect.y,
            Theme::DeckBuilding::SCROLLBAR_WIDTH,
            layout.deckEntriesClipRect.h
        };
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer,
                               Theme::DeckBuilding::SCROLLBAR_TRACK.r,
                               Theme::DeckBuilding::SCROLLBAR_TRACK.g,
                               Theme::DeckBuilding::SCROLLBAR_TRACK.b,
                               Theme::DeckBuilding::SCROLLBAR_TRACK.a);
        SDL_RenderFillRect(renderer, &trackRect);

        const int thumbHeight = std::max(
            Theme::DeckBuilding::SCROLLBAR_THUMB_MIN_H,
            (trackRect.h * trackRect.h) / (trackRect.h + layout.maxDeckScrollOffset)
        );
        const int maxThumbTravel = std::max(0, trackRect.h - thumbHeight);
        const int thumbY = trackRect.y +
            (layout.maxDeckScrollOffset > 0
                ? (deckBuilding.deckScrollOffset * maxThumbTravel) / layout.maxDeckScrollOffset
                : 0);
        const SDL_Rect thumbRect{trackRect.x, thumbY, trackRect.w, thumbHeight};
        SDL_SetRenderDrawColor(renderer,
                               Theme::DeckBuilding::SCROLLBAR_THUMB.r,
                               Theme::DeckBuilding::SCROLLBAR_THUMB.g,
                               Theme::DeckBuilding::SCROLLBAR_THUMB.b,
                               Theme::DeckBuilding::SCROLLBAR_THUMB.a);
        SDL_RenderFillRect(renderer, &thumbRect);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    }

    if (deckBuilding.dragging && deckBuilding.draggedCardIndex >= 0 && deckBuilding.draggedCardIndex < static_cast<int>(deckBuilding.availableCards.size())) {
        const Card& card = *deckBuilding.availableCards[deckBuilding.draggedCardIndex];
        int dragW = Theme::DeckBuilding::DRAG_FALLBACK_WIDTH;
        int dragH = Theme::DeckBuilding::DRAG_FALLBACK_HEIGHT;
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
                if (SDL_PointInRect(&mousePoint, &layout.deckEntriesClipRect) == SDL_TRUE &&
                    SDL_PointInRect(&mousePoint, &layout.deckEntryRects[i]) == SDL_TRUE) {
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

    constexpr Uint32 hoverDelayMs = Theme::DeckBuilding::HOVER_PREVIEW_DELAY_MS;
    const bool showPreview =
        deckBuilding.hoverIndex != static_cast<std::size_t>(-1) &&
        deckBuilding.hoverIndex < deckBuilding.availableCards.size() &&
        now - deckBuilding.hoverStartTick >= hoverDelayMs;

    if (showPreview) {
        int screenW = Theme::DeckBuilding::DEFAULT_SCREEN_WIDTH;
        int screenH = Theme::DeckBuilding::DEFAULT_SCREEN_HEIGHT;
        SDL_GetRendererOutputSize(renderer, &screenW, &screenH);

        int previewWidth = std::min(Theme::DeckBuilding::PREVIEW_MAX_WIDTH, screenW / Theme::DeckBuilding::PREVIEW_SCREEN_WIDTH_RATIO_DIV);
        int previewHeight = static_cast<int>(previewWidth * Theme::DeckBuilding::PREVIEW_ASPECT_RATIO);
        if (previewHeight > screenH - (Theme::DeckBuilding::PREVIEW_EDGE_MARGIN * 2)) {
            previewHeight = screenH - (Theme::DeckBuilding::PREVIEW_EDGE_MARGIN * 2);
            previewWidth = static_cast<int>(previewHeight / Theme::DeckBuilding::PREVIEW_ASPECT_RATIO);
        }

        int previewX = layout.deckArea.x - previewWidth - Theme::DeckBuilding::PREVIEW_DECK_GAP;
        if (previewX < Theme::DeckBuilding::PREVIEW_EDGE_MARGIN) previewX = Theme::DeckBuilding::PREVIEW_EDGE_MARGIN;
        int previewY = layout.deckArea.y;
        if (previewY + previewHeight > screenH - Theme::DeckBuilding::PREVIEW_EDGE_MARGIN) {
            previewY = screenH - previewHeight - Theme::DeckBuilding::PREVIEW_EDGE_MARGIN;
        }
        if (previewY < Theme::DeckBuilding::PREVIEW_EDGE_MARGIN) previewY = Theme::DeckBuilding::PREVIEW_EDGE_MARGIN;

        SDL_Rect panel{previewX, previewY, previewWidth, previewHeight};
        RenderCard::drawPreview(
            renderer,
            textRenderer,
            *deckBuilding.availableCards[deckBuilding.hoverIndex],
            panel,
            fontSmall,
            fontLarge ? fontLarge : fontSmall,
            0
        );
    }
}
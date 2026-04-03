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
#include <render/RenderBackdrop.hpp>
#include <render/RenderCardOverlay.hpp>

void RenderDeckBuilding::render(DeckBuilding& deckBuilding, Game& game) {
    SDL_Renderer* renderer = game.getRenderer();
    if (!renderer) return;

    int screenW, screenH;
    SDL_GetRendererOutputSize(renderer, &screenW, &screenH);

    // ── background ─────────────────────────────
    RenderBackdrop::drawBackgroundWithVignette(
        renderer,
        screenW,
        screenH,
        Theme::BG,
        Theme::Loading::VIGNETTE_COLOR,
        Theme::Loading::VIGNETTE_LAYERS,
        Theme::Loading::VIGNETTE_ALPHA_FALLOFF,
        Theme::Loading::VIGNETTE_MAX_ALPHA
    );
    
    // ── fonts ────────────────────────────────────────────────────────
    const RenderText::FontSet& fonts = game.getUIFonts();
    const RenderText::FontSet& titleFonts = game.getTitleFonts();

    TTF_Font* fontSmall  = fonts.small;
    TTF_Font* fontTiny   = fonts.tiny;
    TTF_Font* fontLarge  = fonts.large;
    TTF_Font* buttonFont = fontSmall ? fontSmall : fontLarge;

    const int headerY = Theme::DeckBuilding::HEADER_Y;
    {
        int titleW = 0, titleH = 0;
        RenderText::measureText(titleFonts.large, "Deck", titleW, titleH);
        RenderText::drawText(renderer, "Deck", titleFonts.large, Theme::BANNER_TEXT, 40, headerY);
    }

    RenderText textRenderer;
    auto layout = deckBuilding.buildLayout(game);
    int contentYOffset = static_cast<int>(screenH * 0.02f);
    layout.applyYOffset(contentYOffset);
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
    const auto& availableCards = deckBuilding.getAvailableCards();
    const auto& inventoryCopies = Inventory::getCachedInventoryCopies();
    const bool inventoryLoaded = Inventory::isInventoryCacheLoaded();



    // ── menu buttons ─────────────────────────────────────────────────
    const bool canPlay  = Deck::hasFullDeck(deckBuilding.deckCopies);
    const bool canSave  = Deck::hasFullDeck(deckBuilding.deckCopies);
    const bool hoverTitle = SDL_PointInRect(&mousePoint, &deckBuilding.TitleButton) == SDL_TRUE;
    const bool hoverPlay  = SDL_PointInRect(&mousePoint, &deckBuilding.PlayButton)  == SDL_TRUE;
    const bool hoverSave  = SDL_PointInRect(&mousePoint, &deckBuilding.SaveButton)  == SDL_TRUE;
    const bool hoverClear = SDL_PointInRect(&mousePoint, &deckBuilding.ClearButton) == SDL_TRUE;

    RenderButton::drawButton(renderer, deckBuilding.PlayButton,
                              "Play", buttonFont,
                              canPlay ? Theme::BTN_START     : Theme::BTN_SECONDARY,
                              Theme::BTN_BORDER, Theme::BTN_TEXT,
                              hoverPlay, !canPlay);

    RenderButton::drawButton(renderer, deckBuilding.SaveButton,
                              "Save Deck", buttonFont,
                              canSave ? Theme::BTN_PRIMARY     : Theme::BTN_SECONDARY,
                              Theme::BTN_BORDER, Theme::BTN_TEXT,
                              hoverSave, !canSave);

    RenderButton::drawButton(renderer, deckBuilding.TitleButton,
                              "Back to Title", buttonFont,
                              Theme::BTN_QUIT,
                              Theme::BTN_BORDER, Theme::BTN_TEXT,
                              hoverTitle, false);

    RenderButton::drawButton(renderer, deckBuilding.ClearButton,
                              "Clear Deck", buttonFont,
                              Theme::BTN_PRIMARY,
                              Theme::BTN_BORDER, Theme::BTN_TEXT,
                              hoverClear, false);

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

    // ── Collection cards ─────────────────────────────────────────────
    for (std::size_t i = 0; i < layout.collectionCardRects.size(); ++i) {
        const SDL_Rect cardRect = layout.collectionCardRects[i];
        if (i >= layout.collectionCardIndices.size()) continue;
        const int cardIndex = layout.collectionCardIndices[i];
        if (cardIndex < 0 || cardIndex >= static_cast<int>(availableCards.size())) continue;
        const Card& card = *availableCards[cardIndex];

        const int remaining = Inventory::getRemainingCount(
            inventoryCopies,
            inventoryLoaded,
            deckBuilding.deckCopies,
            cardIndex,
            Deck::getDeckCopiesLimit()
        );
        const bool dimmed = remaining <= 0;
        RenderCard::drawCardFace(renderer, textRenderer, card, cardRect, fontSmall, fontTiny, dimmed);

        // ── Feature 1: strong grey overlay when fully exhausted ───────
        if (dimmed) {
            RenderCardOverlay::overlayDim(renderer, cardRect, 150);
        }

        // ── Feature 2: quantity as "remaining/total" ──────────────────
        if (fontTiny) {
            int inventoryTotal = 0;
            if (inventoryLoaded &&
                cardIndex >= 0 &&
                cardIndex < static_cast<int>(inventoryCopies.size())) {
                inventoryTotal = inventoryCopies[cardIndex];
            }
            const std::string qtyText =
                std::to_string(remaining) + "/" + std::to_string(inventoryTotal);
            const SDL_Color qtyColor = dimmed
                ? Theme::DeckBuilding::QUANTITY_TEXT_DIM
                : Theme::DeckBuilding::QUANTITY_TEXT;
            textRenderer.drawText(
                renderer, qtyText, fontTiny, qtyColor,
                cardRect.x + Theme::DeckBuilding::CARD_QTY_X_OFFSET,
                cardRect.y + cardRect.h + Theme::DeckBuilding::CARD_QTY_Y_OFFSET
            );
        }
    }

    // ── Pager ─────────────────────────────────────────────────────────
    if (layout.pageCount > 1) {
        const bool canPrev  = layout.pageIndex > 0;
        const bool canNext  = layout.pageIndex < layout.pageCount - 1;
        const bool hoverPrev = canPrev && SDL_PointInRect(&mousePoint, &layout.prevPageButton) == SDL_TRUE;
        const bool hoverNext = canNext && SDL_PointInRect(&mousePoint, &layout.nextPageButton) == SDL_TRUE;

        RenderButton::drawButton(renderer, layout.prevPageButton,
                                  "Prev", fontTiny,
                                  canPrev ? Theme::BTN_PRIMARY : Theme::BTN_SECONDARY,
                                  Theme::BTN_BORDER,
                                  canPrev ? Theme::DeckBuilding::PAGER_TEXT : Theme::DeckBuilding::PAGER_DISABLED_TEXT,
                                  hoverPrev, !canPrev);

        RenderButton::drawButton(renderer, layout.nextPageButton,
                                  "Next", fontTiny,
                                  canNext ? Theme::BTN_PRIMARY : Theme::BTN_SECONDARY,
                                  Theme::BTN_BORDER,
                                  canNext ? Theme::DeckBuilding::PAGER_TEXT : Theme::DeckBuilding::PAGER_DISABLED_TEXT,
                                  hoverNext, !canNext);

        if (fontTiny) {
            const std::string pageText = "Page " + std::to_string(layout.pageIndex + 1) + " / " + std::to_string(layout.pageCount);
            textRenderer.drawText(renderer, pageText, fontTiny, Theme::DeckBuilding::PAGE_LABEL_TEXT,
                layout.pageLabelRect.x + Theme::DeckBuilding::PAGE_LABEL_X_OFFSET,
                layout.pageLabelRect.y + Theme::DeckBuilding::PAGE_LABEL_Y_OFFSET);
        }
    }

    // ── Deck panel ────────────────────────────────────────────────────
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

    // ── Deck entries ──────────────────────────────────────────────────
    SDL_RenderSetClipRect(renderer, &layout.deckEntriesClipRect);
    for (std::size_t i = 0; i < layout.deckEntryRects.size(); ++i) {
        const SDL_Rect entryRect = layout.deckEntryRects[i];
        if (entryRect.y + entryRect.h <= layout.deckEntriesClipRect.y ||
            entryRect.y >= layout.deckEntriesClipRect.y + layout.deckEntriesClipRect.h) {
            continue;
        }

        const int cardIndex = layout.deckEntryCardIndices[i];
        const Card& card    = *availableCards[cardIndex];
        const int copies    = deckBuilding.deckCopies[cardIndex];

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
            textRenderer.drawText(renderer, card.getName(), fontTiny,
                Theme::DeckBuilding::ENTRY_NAME_TEXT,
                entryRect.x + Theme::DeckBuilding::ENTRY_TEXT_X_OFFSET,
                entryRect.y + Theme::DeckBuilding::ENTRY_TEXT_Y_OFFSET);

            textRenderer.drawText(renderer, "Cost: " + std::to_string(card.getManaCost()), fontTiny,
                Theme::DeckBuilding::ENTRY_COST_TEXT,
                entryRect.x + Theme::DeckBuilding::ENTRY_COST_X_OFFSET,
                entryRect.y + Theme::DeckBuilding::ENTRY_TEXT_Y_OFFSET);

            // Copy count — nudged left to make room for the remove button
            const int removeBtnSize   = Theme::DeckBuilding::ENTRY_REMOVE_BTN_SIZE;
            const int removeBtnMargin = Theme::DeckBuilding::ENTRY_REMOVE_BTN_MARGIN;
            textRenderer.drawText(renderer, "x" + std::to_string(copies), fontTiny,
                Theme::DeckBuilding::ENTRY_COUNT_TEXT,
                entryRect.x + entryRect.w
                    - removeBtnSize - removeBtnMargin
                    - Theme::DeckBuilding::ENTRY_COUNT_X_RIGHT_INSET,
                entryRect.y + Theme::DeckBuilding::ENTRY_TEXT_Y_OFFSET);
        }

        // ── Feature 3: remove (✕) button ─────────────────────────────
        if (i < layout.deckEntryRemoveRects.size()) {
            const SDL_Rect& removeRect = layout.deckEntryRemoveRects[i];
            const bool hoverRemove =
                SDL_PointInRect(&mousePoint, &layout.deckEntriesClipRect) == SDL_TRUE &&
                SDL_PointInRect(&mousePoint, &removeRect) == SDL_TRUE;

            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer,
                hoverRemove ? 220 : 160,
                40, 40,
                hoverRemove ? 255 : 210);
            SDL_RenderFillRect(renderer, &removeRect);

            // Thin white border
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, hoverRemove ? 200 : 120);
            SDL_RenderDrawRect(renderer, &removeRect);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

            if (fontTiny) {
                // Centre "x" label inside the button
                int textW = 0, textH = 0;
                TTF_SizeText(fontTiny, "x", &textW, &textH);
                textRenderer.drawText(renderer, "x", fontTiny,
                    SDL_Color{255, 255, 255, 255},
                    removeRect.x + (removeRect.w - textW) / 2,
                    removeRect.y + (removeRect.h - textH) / 2);
            }
        }
    }
    SDL_RenderSetClipRect(renderer, nullptr);

    // ── Scrollbar ─────────────────────────────────────────────────────
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

    // ── Drag ghost ────────────────────────────────────────────────────
    if (deckBuilding.dragging && deckBuilding.draggedCardIndex >= 0 &&
        deckBuilding.draggedCardIndex < static_cast<int>(availableCards.size())) {
        const Card& card = *availableCards[deckBuilding.draggedCardIndex];
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

    // ── Hover preview ─────────────────────────────────────────────────
    std::size_t newHoverIndex = static_cast<std::size_t>(-1);
    if (!deckBuilding.dragging) {
        for (std::size_t i = 0; i < layout.collectionCardRects.size(); ++i) {
            if (SDL_PointInRect(&mousePoint, &layout.collectionCardRects[i]) == SDL_TRUE) {
                newHoverIndex = (i < layout.collectionCardIndices.size())
                    ? static_cast<std::size_t>(layout.collectionCardIndices[i])
                    : i;
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
        deckBuilding.hoverIndex      = newHoverIndex;
        deckBuilding.hoverStartTick  = now;
    }

    constexpr Uint32 hoverDelayMs = Theme::DeckBuilding::HOVER_PREVIEW_DELAY_MS;
    const bool showPreview =
        deckBuilding.hoverIndex != static_cast<std::size_t>(-1) &&
        deckBuilding.hoverIndex < availableCards.size() &&
        now - deckBuilding.hoverStartTick >= hoverDelayMs;

    if (showPreview) {
        const int hoveredCardIndex = static_cast<int>(deckBuilding.hoverIndex);
        const SDL_Rect* hoveredRect = nullptr;

        // Look up visible rect in collection first
        if (hoveredCardIndex >= 0 && hoveredCardIndex < static_cast<int>(availableCards.size())) {
            auto it = std::find(layout.collectionCardIndices.begin(),
                                layout.collectionCardIndices.end(),
                                hoveredCardIndex);
            if (it != layout.collectionCardIndices.end()) {
                hoveredRect = &layout.collectionCardRects[std::distance(layout.collectionCardIndices.begin(), it)];
            } else {
                // Look in deck entries
                auto itDeck = std::find(layout.deckEntryCardIndices.begin(),
                                        layout.deckEntryCardIndices.end(),
                                        hoveredCardIndex);
                if (itDeck != layout.deckEntryCardIndices.end()) {
                    hoveredRect = &layout.deckEntryRects[std::distance(layout.deckEntryCardIndices.begin(), itDeck)];
                }
            }
        }

        // Only render preview if we found a visible rect
        if (hoveredRect) {
            // Compute preview size once
            int previewWidth  = std::min(
                Theme::DeckBuilding::PREVIEW_MAX_WIDTH,
                screenW / Theme::DeckBuilding::PREVIEW_SCREEN_WIDTH_RATIO_DIV
            );
            int previewHeight = static_cast<int>(previewWidth * Theme::PREVIEW_ASPECT_RATIO);
            const int maxHeight = screenH - (Theme::DeckBuilding::PREVIEW_EDGE_MARGIN * 2);
            if (previewHeight > maxHeight) {
                previewHeight = maxHeight;
                previewWidth  = static_cast<int>(previewHeight / Theme::PREVIEW_ASPECT_RATIO);
            }

            constexpr int gap = 8;

            // Position right of hovered card
            int previewX = hoveredRect->x + hoveredRect->w + gap;
            if (previewX + previewWidth > screenW - Theme::DeckBuilding::PREVIEW_EDGE_MARGIN) {
                previewX = hoveredRect->x - previewWidth - gap;
                if (previewX < Theme::DeckBuilding::PREVIEW_EDGE_MARGIN)
                    previewX = Theme::DeckBuilding::PREVIEW_EDGE_MARGIN;
            }

            int previewY = hoveredRect->y;
            if (previewY + previewHeight > screenH - Theme::DeckBuilding::PREVIEW_EDGE_MARGIN)
                previewY = screenH - previewHeight - Theme::DeckBuilding::PREVIEW_EDGE_MARGIN;
            if (previewY < Theme::DeckBuilding::PREVIEW_EDGE_MARGIN)
                previewY = Theme::DeckBuilding::PREVIEW_EDGE_MARGIN;

            SDL_Rect panel{previewX, previewY, previewWidth, previewHeight};

            // Draw preview
            RenderCard::drawPreview(
                renderer, textRenderer,
                *availableCards[hoveredCardIndex],
                panel,
                fontSmall,
                fontLarge ? fontLarge : fontSmall,
                0
            );
        }
    }
}
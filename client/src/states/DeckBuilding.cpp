// 1. gets all cards in the game from server
// 2. if
    // user owns it, it is not greyed out
    // else greyed out and not interactable
// 3. user can add cards to their deck buy dragging them in the deck area
// 4. user can remove cards from their deck by dragging them out of the deck area
    // cards are sorted by mana similar to hearthstone

#include "states/DeckBuilding.hpp"
#include "render/RenderDeckBuilding.hpp"
#include "core/Game.hpp"
#include "core/NetworkClient.hpp"
#include "objects/Card.h"
#include "objects/Inventory.hpp"
#include "render/Theme.hpp"
#include "utils/LoadAvailableCards.hpp"
#include <SDL2/SDL.h>
#include <iostream>
#include <algorithm>
#include <cstdlib>


namespace {
    SDL_Point getPoint(int x, int y) {
        SDL_Point point{x, y};
        return point;
    }

    bool pointInRect(const SDL_Point& point, const SDL_Rect& rect) {
        return SDL_PointInRect(&point, &rect) == SDL_TRUE;
    }

    int clampScrollOffset(int value, int maxValue) {
        if (value < 0) return 0;
        if (value > maxValue) return maxValue;
        return value;
    }
}

DeckBuilding::DeckBuilding() = default;

bool DeckBuilding::refreshFromService(Game& game) {
    const auto& availableCards = getAvailableCards();
    cardsLoadedFromService = !availableCards.empty();

    if (availableCards.empty()) {
        deckCopies.clear();
        return false;
    }

    deckCopies.assign(availableCards.size(), 0);

    const bool deckLoaded = Deck::loadDeckCopiesFromService(
        game,
        availableCards,
        deckCopies,
        Deck::getDeckCopiesLimit()
    );
    Inventory::ensureInventoryAndCoinsLoaded(
        game,
        availableCards,
        Deck::getDeckCopiesLimit()
    );
    return deckLoaded;
}

void DeckBuilding::enter(Game& game) {
    refreshFromService(game);
    collectionPage = 0;
    deckScrollOffset = 0;
    statusMessage.clear();
    statusMessageUntil = 0;
}

void DeckBuilding::exit(const Game& game) {
    (void)game;
}

void DeckBuilding::handleEvents(Game& game, const SDL_Event& event) {
    const auto& availableCards = getAvailableCards();
    const auto& inventoryCopies = Inventory::getCachedInventoryCopies();
    const bool inventoryLoaded = Inventory::isInventoryCacheLoaded();

    if (event.type == SDL_QUIT) {
        game.setNextState(GameState::Title);
    }

    auto layout = buildLayout(game);
    const int previousScrollOffset = deckScrollOffset;
    deckScrollOffset = clampScrollOffset(deckScrollOffset, layout.maxDeckScrollOffset);
    if (deckScrollOffset != previousScrollOffset) {
        layout = buildLayout(game);
    }

    if (event.type == SDL_MOUSEWHEEL) {
        int mouseX = 0;
        int mouseY = 0;
        SDL_GetMouseState(&mouseX, &mouseY);
        const SDL_Point mousePoint{mouseX, mouseY};

        if (pointInRect(mousePoint, layout.deckArea) && layout.maxDeckScrollOffset > 0) {
            deckScrollOffset -= event.wheel.y * static_cast<int>(Theme::DeckBuilding::SCROLL_STEP_PIXELS * currentScale);
            deckScrollOffset = clampScrollOffset(deckScrollOffset, layout.maxDeckScrollOffset);
            return;
        }
    }

    const bool inTitle = (event.type == SDL_MOUSEBUTTONDOWN) &&
                     (event.button.button == SDL_BUTTON_LEFT) &&
                     (event.button.x >= TitleButton.x && event.button.x <= (TitleButton.x + TitleButton.w)) &&
                     (event.button.y >= TitleButton.y && event.button.y <= (TitleButton.y + TitleButton.h));
    if (inTitle) {
        game.setNextState(GameState::Title);
        return;
    }

    const bool inSave = (event.type == SDL_MOUSEBUTTONDOWN) &&
                    (event.button.button == SDL_BUTTON_LEFT) &&
                    (event.button.x >= SaveButton.x && event.button.x <= (SaveButton.x + SaveButton.w)) &&
                    (event.button.y >= SaveButton.y && event.button.y <= (SaveButton.y + SaveButton.h));
    if (inSave) {
        if (!Deck::hasFullDeck(deckCopies)) {
            const int deckCount = Deck::getDeckCardCount(deckCopies);
            const int deckLimit = Deck::getDeckSizeLimit();
            setStatusMessage(
                "Deck size too small (" + std::to_string(deckCount) + "/" + std::to_string(deckLimit) + ").",
                2500
            );
            return;
        }
        if (Deck::saveDeckCopiesToService(game, availableCards, deckCopies)) {
            setStatusMessage("Deck saved.", 2000);
        } else {
            setStatusMessage("Failed to save deck.", 2000);
        }
        if (!game.refreshPlayerDeckFromService()) {
            setStatusMessage("Failed to load deck.", 2000);
        }
        return;
    }

    const bool inPlay = (event.type == SDL_MOUSEBUTTONDOWN) &&
                    (event.button.button == SDL_BUTTON_LEFT) &&
                    (event.button.x >= PlayButton.x && event.button.x <= (PlayButton.x + PlayButton.w)) &&
                    (event.button.y >= PlayButton.y && event.button.y <= (PlayButton.y + PlayButton.h));
    if (inPlay) {
        if (!Deck::hasFullDeck(deckCopies)) {
            const int deckCount = Deck::getDeckCardCount(deckCopies);
            const int deckLimit = Deck::getDeckSizeLimit();
            setStatusMessage(
                "Deck size too small (" + std::to_string(deckCount) + "/" + std::to_string(deckLimit) + ").",
                2500
            );
            return;
        }
        if (!Deck::saveDeckCopiesToService(game, availableCards, deckCopies)) {
            setStatusMessage("Failed to save deck.", 2000);
            return;
        }
        if (!game.refreshPlayerDeckFromService()) {
            setStatusMessage("Failed to load deck.", 2000);
        }
        game.setNextState(GameState::Connecting);
        return;
    }

    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        const SDL_Point point = getPoint(event.button.x, event.button.y);

        if (layout.pageCount > 1 && pointInRect(point, layout.prevPageButton) && collectionPage > 0) {
            collectionPage -= 1;
            return;
        }
        if (layout.pageCount > 1 && pointInRect(point, layout.nextPageButton) && collectionPage < layout.pageCount - 1) {
            collectionPage += 1;
            return;
        }

        for (std::size_t i = 0; i < layout.collectionCardRects.size(); ++i) {
            if (pointInRect(point, layout.collectionCardRects[i])) {
                dragging = true;
                draggingFromDeck = false;
                if (i < layout.collectionCardIndices.size()) {
                    draggedCardIndex = layout.collectionCardIndices[i];
                } else {
                    draggedCardIndex = static_cast<int>(i);
                }
                if (Inventory::getRemainingCount(
                        inventoryCopies,
                        inventoryLoaded,
                        deckCopies,
                        draggedCardIndex,
                        Deck::getDeckCopiesLimit()
                    ) <= 0) {
                    dragging = false;
                    draggedCardIndex = -1;
                    return;
                }
                dragPos = point;
                dragOffset.x = point.x - layout.collectionCardRects[i].x;
                dragOffset.y = point.y - layout.collectionCardRects[i].y;
                return;
            }
        }

        for (std::size_t i = 0; i < layout.deckEntryRects.size(); ++i) {
            if (pointInRect(point, layout.deckEntriesClipRect) && pointInRect(point, layout.deckEntryRects[i])) {
                dragging = true;
                draggingFromDeck = true;
                draggedCardIndex = layout.deckEntryCardIndices[i];
                dragPos = point;
                dragOffset.x = point.x - layout.deckEntryRects[i].x;
                dragOffset.y = point.y - layout.deckEntryRects[i].y;
                return;
            }
        }
    }

    if (event.type == SDL_MOUSEMOTION && dragging) {
        dragPos = getPoint(event.motion.x, event.motion.y);
    }

    if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT && dragging) {
        const SDL_Point point = getPoint(event.button.x, event.button.y);
        const bool overDeck = pointInRect(point, layout.deckArea);

        if (!draggingFromDeck && overDeck) {
            const int remaining = Inventory::getRemainingCount(
                inventoryCopies,
                inventoryLoaded,
                deckCopies,
                draggedCardIndex,
                Deck::getDeckCopiesLimit()
            );
            Deck::addCopy(
                deckCopies,
                draggedCardIndex,
                Deck::getDeckCopiesLimit(),
                Deck::getDeckSizeLimit(),
                remaining
            );
        }

        if (draggingFromDeck && !overDeck) {
            Deck::removeCopy(deckCopies, draggedCardIndex);
        }

        dragging = false;
        draggingFromDeck = false;
        draggedCardIndex = -1;
    }
}

void DeckBuilding::update(Game& game) {
}

void DeckBuilding::render(Game& game) {
    SDL_Renderer* renderer = game.getRenderer();

    const auto layout = buildLayout(game);
    updateMenuButtons(layout);

    RenderDeckBuilding::render(*this, game);
}

DeckBuilding::Layout DeckBuilding::buildLayout(const Game& game) const {
    Layout layout;
    const auto& availableCards = getAvailableCards();

    int screenW = Theme::SCREEN_DEFAULT_WIDTH;
    int screenH = Theme::SCREEN_DEFAULT_HEIGHT;
    if (SDL_Renderer* renderer = game.getRenderer()) {
        SDL_GetRendererOutputSize(renderer, &screenW, &screenH);
    }

    const float scale = std::min(
        static_cast<float>(screenW) / static_cast<float>(Theme::SCREEN_DEFAULT_WIDTH),
        static_cast<float>(screenH) / static_cast<float>(Theme::SCREEN_DEFAULT_HEIGHT)
    );
    // Store scale for use in updateMenuButtons and scroll handling
    const_cast<DeckBuilding*>(this)->currentScale = scale;

    auto sc = [scale](int v) { return static_cast<int>(v * scale); };

    const int cardWidth      = sc(Theme::DeckBuilding::CARD_WIDTH);
    const int cardHeight     = sc(Theme::DeckBuilding::CARD_HEIGHT);
    const int marginX        = sc(Theme::DeckBuilding::GRID_MARGIN_X);
    const int marginY        = sc(Theme::DeckBuilding::GRID_MARGIN_Y);
    const int rightPadding   = sc(Theme::DeckBuilding::RIGHT_PADDING);
    const int deckGap        = sc(Theme::DeckBuilding::DECK_GAP);
    const int deckWidth      = sc(Theme::DeckBuilding::DECK_WIDTH);
    const int pagerHeight    = sc(Theme::DeckBuilding::PAGER_HEIGHT);
    const int pagerSpacing   = sc(Theme::DeckBuilding::PAGER_SPACING);
    const int bottomPadding  = sc(Theme::DeckBuilding::BOTTOM_PADDING);
    const int entryHeight    = sc(Theme::DeckBuilding::ENTRY_HEIGHT);
    const int entrySpacing   = sc(Theme::DeckBuilding::ENTRY_SPACING);

    const int gridRows = Theme::DeckBuilding::GRID_ROWS;
    const int maxContentWidth = screenW - (rightPadding * 2);
    const int availableGridWidth = maxContentWidth - deckWidth - deckGap - sc(Theme::DeckBuilding::GRID_EXTRA_WIDTH);
    int gridCols = (availableGridWidth + marginX) / (cardWidth + marginX);
    if (gridCols < Theme::DeckBuilding::GRID_MIN_COLS) gridCols = Theme::DeckBuilding::GRID_MIN_COLS;
    if (gridCols > Theme::DeckBuilding::GRID_MAX_COLS) gridCols = Theme::DeckBuilding::GRID_MAX_COLS;
    const int maxSlots = gridCols * gridRows;

    const int gridWidth  = gridCols * cardWidth  + (gridCols - 1) * marginX;
    const int gridHeight = gridRows * cardHeight + (gridRows - 1) * marginY;

    const int collectionHeight = gridHeight + sc(Theme::DeckBuilding::COLLECTION_EXTRA_HEIGHT);
    const int totalHeight = collectionHeight + pagerSpacing + pagerHeight;
    const int maxTop = screenH - bottomPadding - totalHeight;
    int collectionY = (screenH - totalHeight) / 2;
    if (collectionY > maxTop) collectionY = maxTop;
    if (collectionY < sc(Theme::DeckBuilding::COLLECTION_MIN_TOP)) collectionY = sc(Theme::DeckBuilding::COLLECTION_MIN_TOP);

    const int totalWidth = (gridWidth + sc(Theme::DeckBuilding::GRID_EXTRA_WIDTH)) + deckGap + deckWidth;
    int leftPadding = (screenW - totalWidth) / 2;
    if (leftPadding < sc(Theme::DeckBuilding::PANEL_MIN_LEFT)) leftPadding = sc(Theme::DeckBuilding::PANEL_MIN_LEFT);

    layout.collectionArea = SDL_Rect{
        leftPadding,
        collectionY,
        gridWidth + sc(Theme::DeckBuilding::GRID_EXTRA_WIDTH),
        collectionHeight
    };

    int deckX = layout.collectionArea.x + layout.collectionArea.w + deckGap;
    int deckY = layout.collectionArea.y;
    int deckH = layout.collectionArea.h;
    if (deckX + deckWidth > screenW - rightPadding) {
        deckX = screenW - deckWidth - rightPadding;
    }
    if (deckX < layout.collectionArea.x + layout.collectionArea.w + deckGap) {
        deckX = layout.collectionArea.x + layout.collectionArea.w + deckGap;
    }
    layout.deckArea = SDL_Rect{deckX, deckY, deckWidth, deckH};

    const int totalCards = static_cast<int>(availableCards.size());
    int pageCount = (totalCards + maxSlots - 1) / maxSlots;
    if (pageCount < 1) pageCount = 1;

    int pageIndex = collectionPage;
    if (pageIndex < 0) pageIndex = 0;
    if (pageIndex > pageCount - 1) pageIndex = pageCount - 1;

    const int startIndex = pageIndex * maxSlots;
    const int remaining = totalCards - startIndex;
    const int slotCount = std::min(remaining < 0 ? 0 : remaining, maxSlots);
    layout.maxSlots   = maxSlots;
    layout.pageCount  = pageCount;
    layout.pageIndex  = pageIndex;
    layout.collectionCardRects.reserve(slotCount);
    layout.collectionCardIndices.reserve(slotCount);

    const int startX = layout.collectionArea.x + sc(Theme::DeckBuilding::GRID_START_X_PADDING);
    const int startY = layout.collectionArea.y + sc(Theme::DeckBuilding::GRID_START_Y_PADDING);
    for (int i = 0; i < slotCount; ++i) {
        int row = i / gridCols;
        int col = i % gridCols;
        SDL_Rect cardRect{
            startX + col * (cardWidth + marginX),
            startY + row * (cardHeight + marginY),
            cardWidth,
            cardHeight
        };
        layout.collectionCardRects.push_back(cardRect);
        layout.collectionCardIndices.push_back(startIndex + i);
    }

    const int pagerY = layout.collectionArea.y + layout.collectionArea.h + pagerSpacing;
    layout.prevPageButton = SDL_Rect{
        layout.collectionArea.x + sc(Theme::DeckBuilding::PREV_BUTTON_X_OFFSET),
        pagerY,
        sc(Theme::DeckBuilding::PREV_BUTTON_WIDTH),
        pagerHeight
    };
    layout.nextPageButton = SDL_Rect{
        layout.collectionArea.x + layout.collectionArea.w - sc(Theme::DeckBuilding::NEXT_BUTTON_X_INSET),
        pagerY,
        sc(Theme::DeckBuilding::NEXT_BUTTON_WIDTH),
        pagerHeight
    };
    layout.pageLabelRect = SDL_Rect{
        layout.prevPageButton.x + layout.prevPageButton.w + sc(Theme::DeckBuilding::PAGE_LABEL_LEFT_GAP),
        pagerY,
        layout.nextPageButton.x - (layout.prevPageButton.x + layout.prevPageButton.w + sc(Theme::DeckBuilding::PAGE_LABEL_RIGHT_GAP)),
        pagerHeight
    };

    const auto deckOrder = getDeckEntryOrder();
    layout.deckEntryCardIndices = deckOrder;
    layout.deckEntryRects.reserve(deckOrder.size());

    const int entryStartY   = layout.deckArea.y + sc(Theme::DeckBuilding::ENTRY_START_Y_PADDING);
    const int entriesBottom = layout.deckArea.y + layout.deckArea.h - sc(Theme::DeckBuilding::ENTRY_BOTTOM_PADDING);
    const int clipHeight    = std::max(0, entriesBottom - entryStartY);
    layout.deckEntriesClipRect = SDL_Rect{
        layout.deckArea.x + sc(Theme::DeckBuilding::ENTRY_X_PADDING),
        entryStartY,
        layout.deckArea.w - sc(Theme::DeckBuilding::ENTRY_X_TOTAL_PADDING),
        clipHeight
    };

    int contentHeight = 0;
    if (!deckOrder.empty()) {
        contentHeight = static_cast<int>(deckOrder.size()) * entryHeight +
                        static_cast<int>(deckOrder.size() - 1) * entrySpacing;
    }
    layout.maxDeckScrollOffset = std::max(0, contentHeight - layout.deckEntriesClipRect.h);

    for (std::size_t i = 0; i < deckOrder.size(); ++i) {
        SDL_Rect entryRect{
            layout.deckArea.x + sc(Theme::DeckBuilding::ENTRY_X_PADDING),
            entryStartY + static_cast<int>(i) * (entryHeight + entrySpacing) - deckScrollOffset,
            layout.deckArea.w - sc(Theme::DeckBuilding::ENTRY_X_TOTAL_PADDING),
            entryHeight
        };
        layout.deckEntryRects.push_back(entryRect);
    }

    return layout;
}

void DeckBuilding::updateMenuButtons(const Layout& layout) {
    const int contentX = layout.collectionArea.x;
    const int contentRight = layout.deckArea.x + layout.deckArea.w;
    const int contentW = contentRight - contentX;

    const int buttonGap = Theme::DeckBuilding::MENU_BUTTON_GAP;
    const int totalButtonsW = TitleButton.w + SaveButton.w + PlayButton.w + (buttonGap * 2);
    int startX = contentX + (contentW - totalButtonsW) / 2;
    if (startX < Theme::DeckBuilding::MENU_MIN_LEFT) startX = Theme::DeckBuilding::MENU_MIN_LEFT;

    int buttonY = layout.collectionArea.y - TitleButton.h - Theme::DeckBuilding::MENU_TOP_GAP;
    if (buttonY < Theme::DeckBuilding::MENU_MIN_TOP) buttonY = Theme::DeckBuilding::MENU_MIN_TOP;

    TitleButton.x = startX;
    TitleButton.y = buttonY;
    SaveButton.x = startX + TitleButton.w + buttonGap;
    SaveButton.y = buttonY;
    PlayButton.x = SaveButton.x + SaveButton.w + buttonGap;
    PlayButton.y = buttonY;
}

std::vector<int> DeckBuilding::getDeckEntryOrder() const {
    const auto& availableCards = getAvailableCards();
    std::vector<int> indices;
    indices.reserve(availableCards.size());
    for (std::size_t i = 0; i < availableCards.size(); ++i) {
        if (deckCopies[i] > 0) {
            indices.push_back(static_cast<int>(i));
        }
    }

    std::sort(indices.begin(), indices.end(), [&availableCards](int a, int b) {
        const int manaA = availableCards[a]->getManaCost();
        const int manaB = availableCards[b]->getManaCost();
        if (manaA != manaB) {
            return manaA < manaB;
        }
        return availableCards[a]->getName() < availableCards[b]->getName();
    });

    return indices;
}

const std::string& DeckBuilding::getStatusMessage() const {
    return statusMessage;
}

bool DeckBuilding::isStatusMessageActive(Uint32 now) const {
    return !statusMessage.empty() && now <= statusMessageUntil;
}

void DeckBuilding::setStatusMessage(const std::string& message, Uint32 durationMs) {
    statusMessage = message;
    statusMessageUntil = SDL_GetTicks() + durationMs;
}

const std::vector<std::unique_ptr<Card>>& DeckBuilding::getAvailableCards() const {
    return LoadAvailableCardsUtil::getAvailableCards();
}
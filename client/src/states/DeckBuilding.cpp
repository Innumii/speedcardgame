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
#include "objects/CreatureCard.h"
#include "objects/SpellCard.h"
#include <SDL2/SDL.h>
#include <iostream>
#include <algorithm>

namespace {
    SDL_Point getPoint(int x, int y) {
        SDL_Point point{x, y};
        return point;
    }

    bool pointInRect(const SDL_Point& point, const SDL_Rect& rect) {
        return SDL_PointInRect(&point, &rect) == SDL_TRUE;
    }
}

DeckBuilding::DeckBuilding() {
    availableCards.push_back(std::make_unique<CreatureCard>("Blazing Drake", "Flying", 3, 3, 3, 2));
    availableCards.push_back(std::make_unique<CreatureCard>("River Sentinel", "Guard", 2, 2, 2, 3));
    availableCards.push_back(std::make_unique<CreatureCard>("Stone Golem", "Heavy", 4, 4, 4, 4));
    availableCards.push_back(std::make_unique<SpellCard>("Spark", "Deal 2 damage", 1, 1));
    availableCards.push_back(std::make_unique<SpellCard>("Frost Bind", "Freeze a foe", 2, 2));
    availableCards.push_back(std::make_unique<CreatureCard>("Night Stalker", "Stealth", 3, 3, 3, 1));
    availableCards.push_back(std::make_unique<SpellCard>("Arcane Surge", "Draw 1", 3, 3));
    availableCards.push_back(std::make_unique<CreatureCard>("Sunblade", "Charge", 5, 5, 5, 4));

    deckCopies.resize(availableCards.size(), 0);
}

void DeckBuilding::handleEvents(Game& game, const SDL_Event& event) {
    if (event.type == SDL_QUIT) {
        game.setNextState(GameState::Title);
    }

    // Further event handling for deck building would go here
    const bool inTitle = (event.type == SDL_MOUSEBUTTONDOWN) &&
                     (event.button.button == SDL_BUTTON_LEFT) &&
                     (event.button.x >= TitleButton.x && event.button.x <= (TitleButton.x + TitleButton.w)) &&
                     (event.button.y >= TitleButton.y && event.button.y <= (TitleButton.y + TitleButton.h));
    if (inTitle) {
        game.setNextState(GameState::Title);
    };

    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        const auto layout = buildLayout(game);
        const SDL_Point point = getPoint(event.button.x, event.button.y);

        for (std::size_t i = 0; i < layout.collectionCardRects.size(); ++i) {
            if (pointInRect(point, layout.collectionCardRects[i])) {
                dragging = true;
                draggingFromDeck = false;
                draggedCardIndex = static_cast<int>(i);
                dragPos = point;
                dragOffset.x = point.x - layout.collectionCardRects[i].x;
                dragOffset.y = point.y - layout.collectionCardRects[i].y;
                return;
            }
        }

        for (std::size_t i = 0; i < layout.deckEntryRects.size(); ++i) {
            if (pointInRect(point, layout.deckEntryRects[i])) {
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
        const auto layout = buildLayout(game);
        const SDL_Point point = getPoint(event.button.x, event.button.y);
        const bool overDeck = pointInRect(point, layout.deckArea);

        if (!draggingFromDeck && overDeck) {
            tryAddToDeck(draggedCardIndex);
        }

        if (draggingFromDeck && !overDeck) {
            tryRemoveFromDeck(draggedCardIndex);
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

    // return to title menu
    SDL_SetRenderDrawColor(renderer, 25, 25, 25, 255);
    SDL_RenderFillRect(renderer, &TitleButton);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &TitleButton);

    // render cards and deck building UI
    RenderDeckBuilding::render(*this, game);
}

DeckBuilding::Layout DeckBuilding::buildLayout(Game& game) const {
    Layout layout;

    int screenW = 800;
    int screenH = 600;
    if (SDL_Renderer* renderer = game.getRenderer()) {
        SDL_GetRendererOutputSize(renderer, &screenW, &screenH);
    }

    const int cardWidth = 110;
    const int cardHeight = 150;
    const int marginX = 16;
    const int marginY = 16;
    const int gridCols = 4;
    const int gridRows = 2;
    const int maxSlots = gridCols * gridRows;

    const int gridWidth = gridCols * cardWidth + (gridCols - 1) * marginX;
    const int gridHeight = gridRows * cardHeight + (gridRows - 1) * marginY;
    const int startX = 40;
    const int startY = 100;

    layout.collectionArea = SDL_Rect{startX - 20, startY - 40, gridWidth + 40, gridHeight + 60};
    layout.deckArea = SDL_Rect{screenW - 270, 90, 240, screenH - 140};

    const int slotCount = std::min(static_cast<int>(availableCards.size()), maxSlots);
    layout.collectionCardRects.reserve(slotCount);
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
    }

    const auto deckOrder = getDeckEntryOrder();
    layout.deckEntryCardIndices = deckOrder;
    layout.deckEntryRects.reserve(deckOrder.size());

    const int entryHeight = 28;
    const int entryStartY = layout.deckArea.y + 40;
    for (std::size_t i = 0; i < deckOrder.size(); ++i) {
        SDL_Rect entryRect{
            layout.deckArea.x + 10,
            entryStartY + static_cast<int>(i) * (entryHeight + 6),
            layout.deckArea.w - 20,
            entryHeight
        };
        layout.deckEntryRects.push_back(entryRect);
    }

    return layout;
}

std::vector<int> DeckBuilding::getDeckEntryOrder() const {
    std::vector<int> indices;
    indices.reserve(availableCards.size());
    for (std::size_t i = 0; i < availableCards.size(); ++i) {
        if (deckCopies[i] > 0) {
            indices.push_back(static_cast<int>(i));
        }
    }

    std::sort(indices.begin(), indices.end(), [this](int a, int b) {
        const int manaA = availableCards[a]->getManaCost();
        const int manaB = availableCards[b]->getManaCost();
        if (manaA != manaB) {
            return manaA < manaB;
        }
        return availableCards[a]->getName() < availableCards[b]->getName();
    });

    return indices;
}

void DeckBuilding::tryAddToDeck(int cardIndex) {
    if (cardIndex < 0 || cardIndex >= static_cast<int>(deckCopies.size())) return;
    if (deckCopies[cardIndex] >= MaxDeckCopies) return;
    deckCopies[cardIndex] += 1;
}

void DeckBuilding::tryRemoveFromDeck(int cardIndex) {
    if (cardIndex < 0 || cardIndex >= static_cast<int>(deckCopies.size())) return;
    if (deckCopies[cardIndex] <= 0) return;
    deckCopies[cardIndex] -= 1;
}


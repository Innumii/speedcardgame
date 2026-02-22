#include "states/Playing.hpp"

#include "core/Game.hpp"
#include "render/RenderPLaying.hpp"
#include "render/RenderText.hpp"
#include <SDL2/SDL.h>
#include "objects/CreatureCard.h"
#include "objects/SpellCard.h"
#include <algorithm>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    SDL_Rect playZoneBand{0, 0, 0, 0};
}

bool Playing::pointInRect(const SDL_Rect& rect, int x, int y) {
    return x >= rect.x && x < rect.x + rect.w && y >= rect.y && y < rect.y + rect.h;
}

Playing::Playing(int drawIntervalSeconds)
    : drawIntervalSeconds(drawIntervalSeconds) {}

Playing::~Playing() = default;

void Playing::setDeck(Deck newDeck) {
    deck = std::move(newDeck);
}

void Playing::setup(Game& game) {
    renderer = game.getRenderer();
    if (!renderer) {
        throw std::runtime_error("Renderer not available from Game");
    }

    player = Player();
    board = Board();
    drag = DragState{};
    hoverIndex = static_cast<std::size_t>(-1);
    hoverStartTick = 0;

    if (deck.size() == 0) {
        for (int i = 0; i < 20; i++) {
            deck.addCard(std::make_unique<CreatureCard>(
                "Goblin",
                "A small but angry creature",
                1, 1, 1, 1
            ));

            deck.addCard(std::make_unique<SpellCard>(
                "Fireball",
                "Deal 3 damage",
                2, 2
            ));
        }
    }

    deck.shuffle();

    constexpr int startingHandSize = 6;
    for (int i = 0; i < startingHandSize; ++i) {
        player.drawCard(deck);
    }

    lastDrawTick = SDL_GetTicks();
    running = true;
}

std::vector<SDL_Rect> Playing::computeCardLayout(std::size_t count, int screenW, int screenH) const {
    std::vector<SDL_Rect> layout;
    if (count == 0 || screenW <= 0 || screenH <= 0) return layout;

    const int cardWidth = 110;
    const int cardHeight = 165;
    const int maxWidth = static_cast<int>(screenW * 0.8f); // Use 80% of screen width
    
    // 1. Calculate how much space we need if cards didn't overlap
    int totalWidthNoOverlap = static_cast<int>(count) * cardWidth;
    
    // 2. Determine spacing. 
    // If totalWidth > maxWidth, spacing becomes negative (overlap).
    int spacing = 10; // Default gap between cards
    if (totalWidthNoOverlap > maxWidth && count > 1) {
        spacing = (maxWidth - totalWidthNoOverlap) / static_cast<int>(count - 1);
    }

    // 3. Calculate actual total width with the new spacing
    int finalHandWidth = (static_cast<int>(count) * cardWidth) + (static_cast<int>(count - 1) * spacing);
    
    // 4. Center the start position
    int startX = (screenW - finalHandWidth) / 2;
    int startY = screenH - cardHeight - 30; // Fixed distance from bottom

    for (std::size_t i = 0; i < count; ++i) {
        layout.push_back(SDL_Rect{
            startX + static_cast<int>(i) * (cardWidth + spacing),
            startY,
            cardWidth,
            cardHeight
        });
    }

    return layout;
}

void Playing::computeZones(int screenW, int screenH) {
    playSlots.clear();

    if (screenW <= 0 || screenH <= 0) {
        discardZone = SDL_Rect{0, 0, 0, 0};
        return;
    }

    const int handCardHeight = 165;
    const int handYOffset = 30;
    const int slotCount = 5;
    const int slotWidth = 120;
    const int slotHeight = 160;
    const int slotSpacing = 12;
    const int margin = 20;
    const int discardWidth = 110;
    const int discardHeight = 130;
    const int gapToDiscard = 18;

    int handY = screenH - handCardHeight - handYOffset;
    if (!cardRects.empty()) {
        handY = cardRects.front().y;
    }

    int totalSlotsWidth = slotCount * slotWidth + (slotCount - 1) * slotSpacing;

    // Center the play zone itself; deck/discard sit outside of this band.
    int startX = std::max(margin, (screenW - totalSlotsWidth) / 2);
    int slotY = handY - slotHeight - 16;
    if (slotY < margin) slotY = margin;

    playSlots.reserve(static_cast<std::size_t>(slotCount));
    for (int i = 0; i < slotCount; ++i) {
        playSlots.push_back(SDL_Rect{
            startX + i * (slotWidth + slotSpacing),
            slotY,
            slotWidth,
            slotHeight
        });
    }
    if (!playSlots.empty()) {
        const SDL_Rect& firstSlot = playSlots.front();
        const SDL_Rect& lastSlot = playSlots.back();

        playZoneBand.x = firstSlot.x;
        playZoneBand.y = firstSlot.y;
        playZoneBand.w = (lastSlot.x + lastSlot.w) - firstSlot.x; // from left edge of first to right edge of last
        playZoneBand.h = firstSlot.h; // all slots have same height
    } else {
        playZoneBand = SDL_Rect{0,0,0,0};
    }

    int discardX = startX + totalSlotsWidth + gapToDiscard;
    if (discardX + discardWidth + margin > screenW) {
        discardX = screenW - discardWidth - margin;
    }

    int discardY = slotY + (slotHeight - discardHeight) / 2;
    discardZone = SDL_Rect{discardX, discardY, discardWidth, discardHeight};
}

void Playing::handleEvents(Game& game, const SDL_Event& event) {
    if (!renderer) return;

    int screenW = 0, screenH = 0;
    if (SDL_GetRendererOutputSize(renderer, &screenW, &screenH) != 0) {
        return;
    }

    cardRects = computeCardLayout(player.hand.size(), screenW, screenH);
    computeZones(screenW, screenH);

    switch (event.type) {
        case SDL_MOUSEBUTTONDOWN:
            if (event.button.button == SDL_BUTTON_LEFT) {
                const int mx = event.button.x;
                const int my = event.button.y;
                
                // Don't start drag if preview is locked
                if (previewLocked) return;
                
                for (int i = static_cast<int>(cardRects.size()) - 1; i >= 0; --i) {
                    if (pointInRect(cardRects[static_cast<std::size_t>(i)], mx, my)) {
                        drag.active = true;
                        drag.index = static_cast<std::size_t>(i);
                        drag.offsetX = mx - cardRects[drag.index].x;
                        drag.offsetY = my - cardRects[drag.index].y;
                        drag.x = cardRects[drag.index].x;
                        drag.y = cardRects[drag.index].y;
                        break;
                    }
                }
            }
            break;
        case SDL_MOUSEMOTION:
            if (drag.active) {
                drag.x = event.motion.x - drag.offsetX;
                drag.y = event.motion.y - drag.offsetY;
            }
            break;
        case SDL_MOUSEBUTTONUP:
            if (drag.active && event.button.button == SDL_BUTTON_LEFT) {
                const int releaseX = event.button.x;
                const int releaseY = event.button.y;

                const bool droppedInDiscard =
                    drag.index < player.hand.size() &&
                    pointInRect(discardZone, releaseX, releaseY);
                const bool droppedInPlay = 
                    drag.index < player.hand.size() &&
                    pointInRect(playZoneBand, releaseX, releaseY);
                
                if (droppedInDiscard) {
                    std::cout << "Discarding " << player.hand[drag.index].get()->getName() << "\n";
                    const auto manaGain = player.hand[drag.index].get()->getManaValue();
                    if (board.addToDiscard(std::move(player.hand[drag.index]), player.id)) {

                        std::cout << "Adding mana\n";
                        player.addMana(manaGain);
                        
                        std::cout << "Removing card from hand\n";
                        player.hand.erase(player.hand.begin() + static_cast<std::ptrdiff_t>(drag.index));   
                    } 
                } else if (droppedInPlay) {
                    std::cout << "Playing " << player.hand[drag.index].get()->getName() << "\n";
                    //move card to board object
                    //if succeed, cut mana by cost
                    //erase card from hand

                    //Determine lane
                    const int laneWidth = playSlots.front().w; // assuming uniform width
                    const int laneSpacing = (playSlots.size() > 1)
                        ? playSlots[1].x - (playSlots[0].x + playSlots[0].w)
                        : 0;

                    const int relativeX = releaseX - playZoneBand.x;
                    const int bandSegmentWidth = laneWidth + laneSpacing;
                    int laneIndex = relativeX / bandSegmentWidth;

                    if (laneIndex >= 0 && laneIndex < static_cast<int>(playSlots.size())) {
                        std::cout << "Playing " << player.hand[drag.index]->getName()
                                << " into lane " << laneIndex << "\n";

                        const int cost = player.hand[drag.index]->getManaCost();
                        if (player.mana >= cost && board.isZoneEmpty(laneIndex, player.id)) {
                            if (board.addToPlay(laneIndex, player.id, std::move(player.hand[drag.index]))) {
                                player.mana -= cost;
                                player.hand.erase(player.hand.begin() + static_cast<std::ptrdiff_t>(drag.index));
                            }
                            
                        } else {
                            std::cout << "Not enough mana\n";
                        }
                    }
                }

                drag.active = false;
                cardRects = computeCardLayout(player.hand.size(), screenW, screenH);
                computeZones(screenW, screenH);
                // board.displayDiscard(player.id);
                board.displayPlay(player.id);
                
            }
            break;
        case SDL_MOUSEWHEEL:
            if (previewLocked) {
                previewScrollOffset -= event.wheel.y * 20;
                if (previewScrollOffset < 0) previewScrollOffset = 0;
            }
            break;
        default:
            break;
    }
}

void Playing::run() {
    if (!renderer) {
        throw std::runtime_error("Call setup() before run()");
    }

    SDL_Event event;
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
        }

        const Uint32 now = SDL_GetTicks();
        if (now - lastDrawTick >= static_cast<Uint32>(drawIntervalSeconds * 1000)) {
            if (!player.handFull()) {
                player.drawCard(deck);
            }
            lastDrawTick = now;
        }
    }
}

void Playing::update(Game& /*game*/) {
    if (!renderer) return;

    const Uint32 now = SDL_GetTicks();
    if (now - lastDrawTick >= static_cast<Uint32>(drawIntervalSeconds * 1000)) {
        if (!player.handFull()) {
            player.drawCard(deck);
        }
        lastDrawTick = now;
    }

    int screenW = 0, screenH = 0;
    SDL_GetRendererOutputSize(renderer, &screenW, &screenH);
    
    int mouseX = 0, mouseY = 0;
    SDL_GetMouseState(&mouseX, &mouseY);

    // Preview card rectangle
    const int previewW = std::min(360, screenW - 80);
    const int previewH = static_cast<int>(previewW * 1.5f);
    const int previewX = (screenW - previewW) / 2;
    const int previewY = (screenH - previewH) / 2;
    SDL_Rect previewRect{previewX, previewY, previewW, previewH};

    const bool mouseOverPreview = pointInRect(previewRect, mouseX, mouseY);

    // If mouse is over preview, keep it locked and don't update anything
    if (mouseOverPreview && previewLocked) {
        return;
    }

    // Check if hovering over any hand card
    const bool draggingCard = drag.active && drag.index < player.hand.size();
    std::size_t newHoverIndex = static_cast<std::size_t>(-1);
    
    if (!draggingCard && !mouseOverPreview) {
        for (int i = static_cast<int>(cardRects.size()) - 1; i >= 0; --i) {
            if (pointInRect(cardRects[static_cast<std::size_t>(i)], mouseX, mouseY)) {
                newHoverIndex = static_cast<std::size_t>(i);
                break;
            }
        }
    }

    // Update hover index when not over preview
    if (!mouseOverPreview) {
        if (newHoverIndex != hoverIndex) {
            hoverIndex = newHoverIndex;
            hoverStartTick = now;
            previewScrollOffset = 0;
            previewLocked = false;
        }
    }

    constexpr Uint32 hoverDelayMs = 1000;
    
    const bool hoverTimerReady =
        hoverIndex != static_cast<std::size_t>(-1) &&
        hoverIndex < player.hand.size() &&
        now - hoverStartTick >= hoverDelayMs;

    // Lock preview when timer is ready or mouse is over preview
    if ((hoverTimerReady || mouseOverPreview) && hoverIndex < player.hand.size()) {
        previewLocked = true;
    } else if (!mouseOverPreview && !hoverTimerReady) {
        previewLocked = false;
    }
}

void Playing::render(Game& game) {
    RenderPlaying::render(*this, game);
}
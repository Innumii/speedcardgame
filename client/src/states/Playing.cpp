#include "states/Playing.hpp"

#include "core/Game.hpp"
#include "render/RenderPLaying.hpp"
#include "render/RenderText.hpp"
#include <SDL2/SDL.h>
#include "objects/CreatureCard.h"
#include "objects/SpellCard.h"
#include <algorithm>
#include <cctype>
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

bool Playing::isTargetedSpell(const Card& card) const {
    if (card.getType() != CardType::Spell) {
        return false;
    }

    std::string text = card.getText();
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });

    return text.find("target") != std::string::npos;
}

bool Playing::consumeSpell(std::unique_ptr<Card> spell) {
    if (!spell) {
        return false;
    }
    return board.addToDiscard(std::move(spell), player.id);
}

bool Playing::resolvePendingSpellTargetAt(int x, int y) {
    if (!pendingSpellTarget.active || !pendingSpellTarget.spell) {
        return false;
    }

    for (std::size_t lane = 0; lane < playSlots.size(); ++lane) {
        SDL_Rect localRect = playSlots[lane];
        if (pointInRect(localRect, x, y)) {
            std::cout << "Target selected: player " << player.id << ", lane " << lane << "\n";
            consumeSpell(std::move(pendingSpellTarget.spell));
            pendingSpellTarget.active = false;
            return true;
        }

        SDL_Rect opponentRect = playSlots[lane];
        opponentRect.y -= 200;
        const int opponentId = player.id == 0 ? 1 : 0;
        if (pointInRect(opponentRect, x, y)) {
            std::cout << "Target selected: player " << opponentId << ", lane " << lane << "\n";
            consumeSpell(std::move(pendingSpellTarget.spell));
            pendingSpellTarget.active = false;
            return true;
        }
    }

    return false;
}

Playing::Playing(int drawIntervalSeconds)
    : drawIntervalSeconds(drawIntervalSeconds) {}

Playing::~Playing() = default;

void Playing::setDeck(Deck newDeck) {
    deck = std::move(newDeck);
}

SDL_Rect Playing::computeSelfDeckRect(int screenW, int screenH) const {
    if (screenW <= 0 || screenH <= 0 || playSlots.empty()) {
        return SDL_Rect{0, 0, 0, 0};
    }

    const int gap = 18;
    const int margin = 10;
    const int deckW = discardZone.w > 0 ? discardZone.w : 110;
    const int deckH = discardZone.h > 0 ? discardZone.h : 130;
    const int deckY = playSlots.front().y + (playSlots.front().h - deckH) / 2;

    int deckX = playSlots.back().x + playSlots.back().w + gap;
    if (deckX + deckW > screenW - margin) deckX = std::max(margin, screenW - margin - deckW);

    return SDL_Rect{deckX, deckY, deckW, deckH};
}

bool Playing::tryDrawCardWithAnimation(Uint32 now) {
    if (player.handFull()) {
        return false;
    }

    const std::size_t handSizeBefore = player.hand.size();
    player.drawCard(deck);

    if (player.hand.size() <= handSizeBefore || !renderer) {
        return false;
    }

    int screenW = 0;
    int screenH = 0;
    if (SDL_GetRendererOutputSize(renderer, &screenW, &screenH) != 0) {
        return false;
    }

    cardRects = computeCardLayout(player.hand.size(), screenW, screenH);
    computeZones(screenW, screenH);

    const SDL_Rect fromRect = computeSelfDeckRect(screenW, screenH);
    const std::size_t handIndex = player.hand.size() - 1;
    if (handIndex < cardRects.size()) {
        animationQueue.enqueueDrawCard(fromRect, cardRects[handIndex], handIndex, 320);
    }

    return true;
}

void Playing::setup(const Game& game) {
    renderer = game.getRenderer();
    if (!renderer) {
        throw std::runtime_error("Renderer not available from Game");
    }

    player = Player();
    board = Board();
    drag = DragState{};
    hoverIndex = static_cast<std::size_t>(-1);
    hoverStartTick = 0;
    menuOpen = false;
    surrendered = false;
    animationQueue.clear();
    pendingSpellTarget = PendingSpellTargetState{};

    if (deck.size() == 0) {
        for (int i = 0; i < 20; i++) {
            deck.addCard(std::make_unique<CreatureCard>(
                "Goblin",
                "A small but angry creature",
                1, 1, 1, 1, 2
            ));

            deck.addCard(std::make_unique<SpellCard>(
                "Fireball",
                "Deal 3 damage",
                2, 2, 1
            ));
        }
    }

    deck.shuffle();

    constexpr int startingHandSize = 6;
    for (int i = 0; i < startingHandSize; ++i) {
        // get starting player hand
        player.drawCard(deck);

        // get remote player from server and draw their starting hand as well
        // TODO
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

    int discardX = startX - gapToDiscard - discardWidth;
    if (discardX < margin) {
        discardX = margin;
    }

    int discardY = slotY + (slotHeight - discardHeight) / 2;
    discardZone = SDL_Rect{discardX, discardY, discardWidth, discardHeight};
}

void Playing::computeUiRects(int screenW, int screenH) {
    if (screenW <= 0 || screenH <= 0) {
        menuButton = SDL_Rect{0, 0, 0, 0};
        exitGameButton = SDL_Rect{0, 0, 0, 0};
        returnToTitleButton = SDL_Rect{0, 0, 0, 0};
        return;
    }

    const int margin = 20;
    const int menuW = 140;
    const int menuH = 44;
    const int exitW = 180;
    const int exitH = 44;
    const int returnW = 260;
    const int returnH = 62;

    menuButton = SDL_Rect{screenW - menuW - margin, margin, menuW, menuH};
    exitGameButton = SDL_Rect{menuButton.x + (menuButton.w - exitW), menuButton.y + menuButton.h + 12, exitW, exitH};
    returnToTitleButton = SDL_Rect{(screenW - returnW) / 2, (screenH / 2) + 24, returnW, returnH};
}

void Playing::handleEvents(Game& game, const SDL_Event& event) {
    if (!renderer) return;

    int screenW = 0, screenH = 0;
    if (SDL_GetRendererOutputSize(renderer, &screenW, &screenH) != 0) {
        return;
    }

    cardRects = computeCardLayout(player.hand.size(), screenW, screenH);
    computeZones(screenW, screenH);
    computeUiRects(screenW, screenH);

    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        const int mouseX = event.button.x;
        const int mouseY = event.button.y;

        if (surrendered) {
            if (pointInRect(returnToTitleButton, mouseX, mouseY)) {
                surrendered = false;
                menuOpen = false;
                game.setNextState(GameState::Title);
            }
            return;
        }

        if (pointInRect(menuButton, mouseX, mouseY)) {
            menuOpen = !menuOpen;
            return;
        }

        if (menuOpen && pointInRect(exitGameButton, mouseX, mouseY)) {
            surrendered = true;
            menuOpen = false;
            drag.active = false;
            return;
        }

        if (menuOpen) {
            menuOpen = false;
        }
    }

    if (surrendered) {
        return;
    }

    if (pendingSpellTarget.active) {
        if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
            if (resolvePendingSpellTargetAt(event.button.x, event.button.y)) {
                board.displayDiscard(player.id);
            }
        }
        return;
    }

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
                int laneIndex = -1;
                if (drag.index < player.hand.size()) {
                    for (std::size_t slot = 0; slot < playSlots.size(); ++slot) {
                        if (pointInRect(playSlots[slot], releaseX, releaseY)) {
                            laneIndex = static_cast<int>(slot);
                            break;
                        }
                    }
                }
                const bool droppedInPlay = laneIndex >= 0;
                
                if (droppedInDiscard) {
                    std::cout << "Discarding " << player.hand[drag.index].get()->getName() << "\n";
                    const auto manaGain = player.hand[drag.index].get()->getManaValue();
                    if (board.addToDiscard(std::move(player.hand[drag.index]), player.id)) {

                        std::cout << "Adding mana\n";
                        player.addMana(manaGain);
                        
                        std::cout << "Removing card from hand\n";
                        player.hand.erase(player.hand.begin() + static_cast<std::ptrdiff_t>(drag.index));   
                    } 
                } else if (drag.index < player.hand.size()) {
                    Card* draggedCard = player.hand[drag.index].get();
                    if (!draggedCard) {
                        break;
                    }

                    const int cost = draggedCard->getManaCost();
                    if (player.mana < cost) {
                        std::cout << "Not enough mana\n";
                        break;
                    }

                    if (draggedCard->getType() == CardType::Spell) {
                        std::unique_ptr<Card> castSpell = std::move(player.hand[drag.index]);
                        player.hand.erase(player.hand.begin() + static_cast<std::ptrdiff_t>(drag.index));
                        player.mana -= cost;

                        if (castSpell && isTargetedSpell(*castSpell)) {
                            std::cout << "Cast targeted spell: " << castSpell->getName()
                                      << " (choose a target)\n";
                            pendingSpellTarget.active = true;
                            pendingSpellTarget.spell = std::move(castSpell);
                        } else {
                            std::cout << "Cast untargeted spell\n";
                            consumeSpell(std::move(castSpell));
                        }
                    } else if (droppedInPlay && laneIndex < static_cast<int>(playSlots.size())) {
                        std::cout << "Playing " << draggedCard->getName()
                                  << " into lane " << laneIndex << "\n";

                        if (board.isZoneEmpty(laneIndex, player.id)) {
                            if (board.addToPlay(laneIndex, player.id, std::move(player.hand[drag.index]))) {
                                player.mana -= cost;
                                player.hand.erase(player.hand.begin() + static_cast<std::ptrdiff_t>(drag.index));
                            }
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
        animationQueue.update(now);
        if (now - lastDrawTick >= static_cast<Uint32>(drawIntervalSeconds * 1000)) {
            tryDrawCardWithAnimation(now);
            lastDrawTick = now;
        }
    }
}

void Playing::update(Game& /*game*/) {
    if (!renderer) return; // not yet ready
    if (surrendered) return;

    const Uint32 now = SDL_GetTicks();
    animationQueue.update(now);
    if (now - lastDrawTick >= static_cast<Uint32>(drawIntervalSeconds * 1000)) {
        tryDrawCardWithAnimation(now);
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

void Playing::render(const Game& game) {
    RenderPlaying::render(*this, game);
}
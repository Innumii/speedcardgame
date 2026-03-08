#include "states/Playing.hpp"

#include "core/Game.hpp"
#include "render/RenderPlaying.hpp"
#include "render/RenderText.hpp"
#include "gameplay/LocalAuthority.hpp"

#include <SDL2/SDL.h>
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <sstream>

namespace {
    SDL_Rect playZoneBand{0, 0, 0, 0};
}

// -------------------------
// Helpers
// -------------------------
bool Playing::pointInRect(const SDL_Rect& rect, int x, int y) {
    return x >= rect.x && x < rect.x + rect.w &&
           y >= rect.y && y < rect.y + rect.h;
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
    return board.addToDiscard(std::move(spell), localPlayer.id);
}

bool Playing::resolvePendingActionAt(int x, int y) {
    if (!pendingAction.active)
        return false;

    int targetLane = -1;
    int targetOpponent = -1;

    // Check local lanes first
    for (std::size_t slot = 0; slot < playSlots.size(); ++slot) {
        if (pointInRect(playSlots[slot], x, y)) {
            targetLane = static_cast<int>(slot);
            targetOpponent = 0;
            break;
        }
    }

    // Check opponent lanes (offset by render, e.g., 200 px above)
    for (std::size_t slot = 0; slot < playSlots.size(); ++slot) {
        SDL_Rect opponentSlot = playSlots[slot];
        opponentSlot.y -= 200; // same offset as rendering
        if (pointInRect(opponentSlot, x, y)) {
            targetLane = static_cast<int>(slot);
            targetOpponent = 1;
            break;
        }
    }

    if (targetLane >= 0) {
        std::cout<< "[Playing] Casting at targetOpponent: " + std::to_string(targetOpponent) + "\n";
        std::cout<< "[Playing] Casting at targetLane: " + std::to_string(targetLane) + "\n";

        authority->playCard(
            static_cast<int>(pendingAction.cardId),
            pendingAction.sourceLane, // where the spell was dropped
            targetLane,               // target lane
            targetOpponent              // target player (local or opponent)
        );

        pendingAction.clear();
        return true;
    }

    return false;
}

// -------------------------
// Constructor/Destructor
// -------------------------
Playing::Playing(int drawIntervalSeconds)
    : drawIntervalSeconds(drawIntervalSeconds) {}

Playing::~Playing() = default;

// -------------------------
// Setup
// -------------------------
void Playing::setup(const Game& game) {
    renderer = game.getRenderer();
    if (!renderer)
        throw std::runtime_error("Renderer not available");

    drag = DragState{};
    hoverIndex = static_cast<std::size_t>(-1);
    hoverStartTick = 0;
    menuOpen = false;
    pauseModalOpen = false;
    exitModalOpen = false;
    surrendered = false;
    animationQueue.clear();
    pendingAction.clear();
    running = true;
    lastDrawTick = SDL_GetTicks();

    if (!authority) {
        authority = std::make_unique<LocalAuthority>(
            &const_cast<Game&>(game).getNetworkClient());
    }
}

void Playing::setDeck(Deck newDeck) {
    deck = std::move(newDeck);
}

void Playing::setupPlayers(Player&& local, Player&& remote) {
    localPlayer = std::move(local);
    remotePlayer = std::move(remote);
}

SDL_Rect Playing::computeSelfDeckRect(int screenW, int screenH) const {
    if (screenW <= 0 || screenH <= 0 || playSlots.empty()) {
        return SDL_Rect{0, 0, 0, 0};
    }

    const int gap = 20;
    const int margin = 10;
    const int deckSize = 120;  // Match new size
    const int deckY = playSlots.front().y + (playSlots.front().h - deckSize) / 2;

    int deckX = playSlots.back().x + playSlots.back().w + gap;
    if (deckX + deckSize > screenW - margin) {
        deckX = std::max(margin, screenW - margin - deckSize);
    }

    return SDL_Rect{deckX, deckY, deckSize, deckSize};
}

bool Playing::tryDrawCardWithAnimation(Uint32 now) {
    if (localPlayer.hand.size() >= 10) {
        return false;
    }

    if (deck.size() == 0) {
        return false;
    }

    const std::size_t handSizeBefore = localPlayer.hand.size();
    
    auto card = deck.draw();
    if (!card) {
        return false;
    }
    localPlayer.addCardToHand(std::move(card));

    if (localPlayer.hand.size() <= handSizeBefore || !renderer) {
        return false;
    }

    int screenW = 0;
    int screenH = 0;
    if (SDL_GetRendererOutputSize(renderer, &screenW, &screenH) != 0) {
        return false;
    }

    cardRects = computeCardLayout(localPlayer.hand.size(), screenW, screenH);
    computeZones(screenW, screenH);

    const SDL_Rect fromRect = computeSelfDeckRect(screenW, screenH);
    const std::size_t handIndex = localPlayer.hand.size() - 1;
    if (handIndex < cardRects.size()) {
        animationQueue.enqueueDrawCard(fromRect, cardRects[handIndex], handIndex, 320);
    }

    return true;
}

// -------------------------
// Event handling
// -------------------------
void Playing::handleEvents(Game& game, const SDL_Event& event) {
    if (!renderer) return;

    int screenW = 0, screenH = 0;
    SDL_GetRendererOutputSize(renderer, &screenW, &screenH);

    cardRects = computeCardLayout(localPlayer.hand.size(), screenW, screenH);
    computeZones(screenW, screenH);
    computeUiRects(screenW, screenH);

    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        const int mouseX = event.button.x;
        const int mouseY = event.button.y;

        if (surrendered) {
            if (pointInRect(returnToTitleButton, mouseX, mouseY)) {
                surrendered = false;
                pauseModalOpen = false;
                exitModalOpen = false;
                game.setNextState(GameState::Title);
            }
            return;
        }

        if (exitModalOpen) {
            if (pointInRect(saveExitButton, mouseX, mouseY)) {
                surrendered = false;
                pauseModalOpen = false;
                exitModalOpen = false;
                game.setNextState(GameState::Title);
                return;
            }
            if (pointInRect(noSaveExitButton, mouseX, mouseY)) {
                surrendered = false;
                pauseModalOpen = false;
                exitModalOpen = false;
                game.setNextState(GameState::Title);
                return;
            }
            if (!pointInRect(exitModal, mouseX, mouseY)) {
                exitModalOpen = false;
            }
            return;
        }

        if (pauseModalOpen) {
            if (pointInRect(resumeButton, mouseX, mouseY)) {
                pauseModalOpen = false;
                return;
            }
            if (pointInRect(pauseExitButton, mouseX, mouseY)) {
                pauseModalOpen = false;
                exitModalOpen = true;
                return;
            }
            if (!pointInRect(pauseModal, mouseX, mouseY)) {
                pauseModalOpen = false;
            }
            return;
        }

        if (pointInRect(menuButton, mouseX, mouseY)) {
            pauseModalOpen = true;
            return;
        }

        if (menuOpen) {
            menuOpen = false;
        }
    }

    if (surrendered) return;

    if (pendingAction.active) {
        if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
            resolvePendingActionAt(event.button.x, event.button.y);
        }
        return;
    }

    switch (event.type) {
        case SDL_MOUSEBUTTONDOWN:
            if (event.button.button == SDL_BUTTON_LEFT) {
                const int mx = event.button.x;
                const int my = event.button.y;
                
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

                int laneIndex = -1;
                for (std::size_t slot = 0; slot < playSlots.size(); ++slot) {
                    if (pointInRect(playSlots[slot], releaseX, releaseY)) {
                        laneIndex = static_cast<int>(slot);
                        break;
                    }
                }

                bool droppedInDiscard = pointInRect(discardZone, releaseX, releaseY);

                if (droppedInDiscard) {
                    auto& card = localPlayer.hand[drag.index];
                    std::cout << "[Playing] Attempting to Discard " << card->getName() << "\n";
                    authority->discardCard(card->getId());
                }
                else if (laneIndex >= 0) {
                    auto& card = localPlayer.hand[drag.index];
                    if (card->getType() == CardType::Creature) {
                        if (localPlayer.mana >= card->getManaCost()) { //Check if player has sufficient mana
                            authority->playCard(
                                card->getId(),
                                laneIndex,
                                std::nullopt,  //NO TARGETING
                                std::nullopt  //NO TARGETING

                            );
                        }
                    } else if (card->getType() == CardType::Spell) { // Spell enters targeting mode, may need some logic to skip targeting for universal target spells
                        pendingAction.active = true;
                        pendingAction.cardId = card->getId();
                        pendingAction.sourceLane = laneIndex; // store where it was dropped
                    }
                }

                drag.active = false;
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

// -------------------------
// Main loop & Update
// -------------------------
void Playing::run() {
    if (!renderer)
        throw std::runtime_error("Call setup() first");

    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
        }

        animationQueue.update(SDL_GetTicks());
    }
}

void Playing::update(Game& game) {
    if (!renderer) return;
    if (surrendered) return;

    auto& net = game.getNetworkClient();

    char buffer[512];
    int received = net.receive(buffer, sizeof(buffer));

    if (received == -1) {
        std::cerr << "Server disconnected\n";
        game.setNextState(GameState::Title);
        return;
    }

    if (received > 0) {
        recvBuffer.append(buffer, received);

        size_t pos;
        while ((pos = recvBuffer.find('\n')) != std::string::npos) {
            std::string line = recvBuffer.substr(0, pos);
            recvBuffer.erase(0, pos + 1);
            handleServerMessage(line);
        }
    }

    const Uint32 now = SDL_GetTicks();
    animationQueue.update(now);

    int screenW = 0, screenH = 0;
    SDL_GetRendererOutputSize(renderer, &screenW, &screenH);
    
    int mouseX = 0, mouseY = 0;
    SDL_GetMouseState(&mouseX, &mouseY);

    const int previewW = std::min(360, screenW - 80);
    const int previewH = static_cast<int>(previewW * 1.5f);
    const int previewX = (screenW - previewW) / 2;
    const int previewY = (screenH - previewH) / 2;
    SDL_Rect previewRect{previewX, previewY, previewW, previewH};

    const bool mouseOverPreview = pointInRect(previewRect, mouseX, mouseY);

    if (mouseOverPreview && previewLocked) {
        return;
    }

    const bool draggingCard = drag.active && drag.index < localPlayer.hand.size();
    std::size_t newHoverIndex = static_cast<std::size_t>(-1);
    
    if (!draggingCard && !mouseOverPreview) {
        for (int i = static_cast<int>(cardRects.size()) - 1; i >= 0; --i) {
            if (pointInRect(cardRects[static_cast<std::size_t>(i)], mouseX, mouseY)) {
                newHoverIndex = static_cast<std::size_t>(i);
                break;
            }
        }
    }

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
        hoverIndex < localPlayer.hand.size() &&
        now - hoverStartTick >= hoverDelayMs;

    if ((hoverTimerReady || mouseOverPreview) && hoverIndex < localPlayer.hand.size()) {
        previewLocked = true;
    } else if (!mouseOverPreview && !hoverTimerReady) {
        previewLocked = false;
    }
}

bool Playing::handleServerMessage(const std::string& msg) {
    std::istringstream iss(msg);
    std::string cmd;
    iss >> cmd;

// std::cout << "[Playing]: " << msg << "\n";
    
    if (cmd == "DRAW") {
        int playerId, cardId;
        iss >> playerId >> cardId;
        drawCard(playerId, cardId);
    } else if (cmd == "DISCARD") {
        int playerId, cardId;
        iss >> playerId >> cardId;
        discardCard(playerId, cardId);
    } else if (cmd == "PLAY") {
        int playerId, cardId, lane;
        iss >> playerId >> cardId >> lane;

        Player& player = (playerId == localPlayer.id) ? localPlayer : remotePlayer;

        // Find card in hand by ID
        auto it = std::find_if(
            player.hand.begin(), player.hand.end(),
            [cardId](const std::unique_ptr<Card>& c){ return c->getId() == cardId; }
        );

        if (it == player.hand.end()) {
            std::cerr << "[ERROR] Card ID " << cardId
                    << " not found in player " << playerId << "'s hand\n";
            return false;
        }

        CardType type = (*it)->getType();

        if (type == CardType::Creature) {
            // Move card into board
            playCreature(playerId, std::move(*it), lane);
            // Remove from hand
            player.hand.erase(it);
        }
        else if (type == CardType::Spell) {
            // Optionally read target lane and target opponent
            std::cout<< "[Playing] Casting!\n";
            std::optional<int> targetLane;
            if (iss.peek() != EOF) {
                int tmp;
                if (iss >> tmp) targetLane = tmp;
            }

            std::optional<int> targetOpponent;
            if (iss.peek() != EOF) {
                int tmp;
                if (iss >> tmp) targetOpponent = tmp;
            }

            // Move card into spell handler
            playSpell(playerId, std::move(*it), lane, targetLane, targetOpponent);
            player.hand.erase(it);
        }
    }

    return true;
}

void Playing::playCreature(int playerId, std::unique_ptr<Card> card, int lane) {
    // Determine player
    Player& player = (playerId == localPlayer.id) ? localPlayer : remotePlayer;

    if (lane < 0 || lane >= board.getLaneCount()) return;
    if (!card) return;

    if (card->getType() != CardType::Creature) return;

    std::string name = card->getName();
    player.mana -= card->getManaCost();

    // Move the card into the board
    int boardIndex = (playerId == localPlayer.id) ? 0:1;
    board.addToPlay(lane, boardIndex, std::move(card));

    // Optional: debug output
    std::cout << "[Playing] Summoned" << name
              << " for player " << playerId 
              << " at lane " << lane << "\n";
}

void Playing::playSpell(int playerId, std::unique_ptr<Card> card, int sourceLane, std::optional<int> targetLane, std::optional<int> targetOpponent) {
    Player& player = (playerId == localPlayer.id) ? localPlayer : remotePlayer;
    if (sourceLane < 0 || sourceLane >= board.getLaneCount()) return;
    if (!card) return;

    if (card->getType() != CardType::Spell) return;
    std::cout<< "[Playing] Casting from playSpell!\n";

    std::string name = card->getName();
    player.mana -= card->getManaCost();

    // board.addToPlay(sourceLane, boardIndex, std::move(card));

    //enact effect
    if (playerId != localPlayer.id) {
        if (targetOpponent == 0) targetOpponent = 1;
        else if (targetOpponent == 1) targetOpponent = 0;
    }

    std::cout << "[Playing] Casted" << name
              << " for player " << playerId 
              << " at lane " << sourceLane << "\n";
}




bool Playing::drawCard(int playerId, int cardId) {
    Player& player = (playerId == localPlayer.id)
                     ? localPlayer
                     : remotePlayer;

    auto card = player.getDeck().takeCardById(cardId);

    if (!card) {
        std::cerr << "Missing card id " << cardId << "\n";
        return false;
    }

    player.addCardToHand(std::move(card));

    int screenW = 0;
    int screenH = 0;
    if (SDL_GetRendererOutputSize(renderer, &screenW, &screenH) != 0) {
        return false;
    }

    cardRects = computeCardLayout(player.hand.size(), screenW, screenH);
    computeZones(screenW, screenH);
    const SDL_Rect fromRect = computeSelfDeckRect(screenW, screenH);
    const std::size_t handIndex = player.hand.size() - 1;

    if (playerId == localPlayer.id && handIndex < cardRects.size()) {
        animationQueue.enqueueDrawCard(fromRect, cardRects[handIndex], handIndex, 320);    
    }

    return true;
}

void Playing::discardCard(int playerId, int cardId) {
    Player* player = (playerId == localPlayer.id) ? &localPlayer : &remotePlayer;

    auto it = std::find_if(player->hand.begin(), player->hand.end(),
                           [cardId](const std::unique_ptr<Card>& c) {
                               return c->getId() == cardId;
                           });

    if (it == player->hand.end()) {
        // Card not found (maybe already discarded?), just ignore
        return;
    }

    std::unique_ptr<Card> cardToDiscard = std::move(*it);
    std::string name = cardToDiscard->getName();
    player->hand.erase(it);
    player->addMana(cardToDiscard->getManaValue());

    int boardIndex = (playerId == localPlayer.id) ? 0:1;
    board.addToDiscard(std::move(cardToDiscard), boardIndex);

    // You can animate this if desired
    std::cout<< "[Playing] " << playerId << " Discarded " << name << "\n";
}



void Playing::render(const Game& game) {
    RenderPlaying::render(*this, game);
}

// -------------------------
// Layout Computation
// -------------------------
std::vector<SDL_Rect> Playing::computeCardLayout(std::size_t count, int screenW, int screenH) const {
    std::vector<SDL_Rect> layout;
    if (count == 0 || screenW <= 0 || screenH <= 0) return layout;

    const int cardWidth = 100;
    const int cardHeight = 150;
    const int maxWidth = static_cast<int>(screenW * 0.75f);
    
    int totalWidthNoOverlap = static_cast<int>(count) * cardWidth;
    
    int spacing = 10;
    if (totalWidthNoOverlap > maxWidth && count > 1) {
        spacing = (maxWidth - totalWidthNoOverlap) / static_cast<int>(count - 1);
    }

    int finalHandWidth = (static_cast<int>(count) * cardWidth) + (static_cast<int>(count - 1) * spacing);
    
    int startX = (screenW - finalHandWidth) / 2;
    int startY = screenH - cardHeight - 80;
    
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
    playSlots.clear();       // local player zones
    opponentSlots.clear();   // opponent zones

    if (screenW <= 0 || screenH <= 0) {
        discardZone = SDL_Rect{0, 0, 0, 0};
        return;
    }

    const int handCardHeight = 150;
    const int handYOffset = 80;
    const int slotCount = 5;
    const int slotWidth = 115;
    const int slotHeight = 145;
    const int slotSpacing = 15;
    const int margin = 20;
    const int discardWidth = 110;
    const int discardHeight = 130;
    const int opponentOffset = 200; // vertical offset for opponent zones
    const int discardSize = 120;
    const int gapToDiscard = 20;

    // Hand Y position
    int handY = screenH - handCardHeight - handYOffset;
    if (!cardRects.empty()) {
        handY = cardRects.front().y;
    }

    int totalSlotsWidth = slotCount * slotWidth + (slotCount - 1) * slotSpacing;
    int startX = std::max(margin, (screenW - totalSlotsWidth) / 2);

    // Local player slots
    int slotY = handY - slotHeight - 16;
    if (slotY < margin) slotY = margin;

    playSlots.reserve(static_cast<std::size_t>(slotCount));
    opponentSlots.reserve(static_cast<std::size_t>(slotCount));

    for (int i = 0; i < slotCount; ++i) {
        SDL_Rect localRect{
            startX + i * (slotWidth + slotSpacing),
            slotY,
            slotWidth,
            slotHeight
        };
        playSlots.push_back(localRect);

        // Opponent zones are offset upward by opponentOffset
        SDL_Rect oppRect = localRect;
        oppRect.y -= opponentOffset;
        opponentSlots.push_back(oppRect);
    }

    // Optional: set playZoneBand for local rendering / input
    if (!playSlots.empty()) {
        const SDL_Rect& firstSlot = playSlots.front();
        const SDL_Rect& lastSlot = playSlots.back();

        playZoneBand.x = firstSlot.x;
        playZoneBand.y = firstSlot.y;
        playZoneBand.w = (lastSlot.x + lastSlot.w) - firstSlot.x;
        playZoneBand.h = firstSlot.h;
    } else {
        playZoneBand = SDL_Rect{0,0,0,0};
    }

    // Discard zone
    int discardX = startX - gapToDiscard - discardWidth;
    if (discardX < margin) discardX = margin;

    int discardY = slotY + (slotHeight - discardSize) / 2;
    discardZone = SDL_Rect{discardX, discardY, discardSize, discardSize};
}

void Playing::computeUiRects(int screenW, int screenH) {
    if (screenW <= 0 || screenH <= 0) {
        menuButton = SDL_Rect{0, 0, 0, 0};
        pauseModal = SDL_Rect{0, 0, 0, 0};
        resumeButton = SDL_Rect{0, 0, 0, 0};
        pauseExitButton = SDL_Rect{0, 0, 0, 0};
        exitModal = SDL_Rect{0, 0, 0, 0};
        saveExitButton = SDL_Rect{0, 0, 0, 0};
        noSaveExitButton = SDL_Rect{0, 0, 0, 0};
        returnToTitleButton = SDL_Rect{0, 0, 0, 0};
        return;
    }

    const int margin = 20;
    
    // Menu button - TOP RIGHT
    const int menuW = 120;
    const int menuH = 50;
    menuButton = SDL_Rect{screenW - menuW - margin, margin, menuW, menuH};
    
    const int pauseModalW = 400;
    const int pauseModalH = 280;
    pauseModal = SDL_Rect{(screenW - pauseModalW) / 2, (screenH - pauseModalH) / 2, pauseModalW, pauseModalH};
    
    const int buttonW = 320;
    const int buttonH = 60;
    const int buttonSpacing = 20;
    const int firstButtonY = pauseModal.y + 100;
    
    resumeButton = SDL_Rect{(screenW - buttonW) / 2, firstButtonY, buttonW, buttonH};
    pauseExitButton = SDL_Rect{(screenW - buttonW) / 2, firstButtonY + buttonH + buttonSpacing, buttonW, buttonH};
    
    const int exitModalW = 480;
    const int exitModalH = 320;
    exitModal = SDL_Rect{(screenW - exitModalW) / 2, (screenH - exitModalH) / 2, exitModalW, exitModalH};
    
    const int exitButtonW = 200;
    const int exitButtonH = 60;
    const int exitButtonSpacing = 20;
    const int exitFirstButtonY = exitModal.y + 140;
    
    saveExitButton = SDL_Rect{(screenW - exitButtonW * 2 - exitButtonSpacing) / 2, exitFirstButtonY, exitButtonW, exitButtonH};
    noSaveExitButton = SDL_Rect{saveExitButton.x + exitButtonW + exitButtonSpacing, exitFirstButtonY, exitButtonW, exitButtonH};
    
    const int returnW = 260;
    const int returnH = 62;
    returnToTitleButton = SDL_Rect{(screenW - returnW) / 2, (screenH / 2) + 24, returnW, returnH};
}
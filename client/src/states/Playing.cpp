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

// Resolve targeting click
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
// Setup
// -------------------------

Playing::~Playing() {
    RenderText::closeFonts(fonts);
    RenderText::shutdownTtf();
}

void Playing::setup(Game& game) {
    renderer = game.getRenderer();
    if (!renderer)
        throw std::runtime_error("Renderer not available");

    drag = DragState{};
    hoverIndex = static_cast<std::size_t>(-1);
    hoverStartTick = 0;
    menuOpen = false;
    surrendered = false;
    animationQueue.clear();
    pendingAction.clear();
    running = true;

    if (!RenderText::ensureTtfReady()) {
        throw std::runtime_error(
            std::string("TTF_Init failed: ") + TTF_GetError());
    }

    fonts = RenderText::loadFonts("assets/font.TTF", 14, 12, 24);
    if (!fonts.large || !fonts.small) {
        RenderText::closeFonts(fonts);
        throw std::runtime_error("Failed to load fonts");
    }

    if (!authority) {
        authority = std::make_unique<LocalAuthority>(
            &game.getNetworkClient());
    }
    std::cout << "exiting Playing setup...\n";
}

void Playing::setupPlayers(Player&& local, Player&& remote) {
    localPlayer = std::move(local);
    remotePlayer = std::move(remote);

    localPlayer.deck.toString();
}

// -------------------------
// Event handling
// -------------------------
void Playing::handleEvents(Game& game, const SDL_Event& event) {
    if (!renderer) return;

    int screenW = 0, screenH = 0;
    SDL_GetRendererOutputSize(renderer, &screenW, &screenH);

    computeZones(screenW, screenH);
    computeUiRects(screenW, screenH);

    cardRects = computeCardLayout(
        localPlayer.hand.size(),
        screenW,
        screenH
    );

    int mx = 0;
    int my = 0;

    if (event.type == SDL_MOUSEBUTTONDOWN ||
        event.type == SDL_MOUSEBUTTONUP) {
        mx = event.button.x;
        my = event.button.y;
    }
    else if (event.type == SDL_MOUSEMOTION) {
        mx = event.motion.x;
        my = event.motion.y;
    }

    // -------------------------
    // UI Buttons
    // -------------------------
    if (event.type == SDL_MOUSEBUTTONDOWN &&
        event.button.button == SDL_BUTTON_LEFT) {

        if (surrendered) {
            if (pointInRect(returnToTitleButton, mx, my)) {
                surrendered = false;
                menuOpen = false;
                game.setNextState(GameState::Title);
            }
            return;
        }

        if (pointInRect(menuButton, mx, my)) {
            menuOpen = !menuOpen;
            return;
        }

        if (menuOpen && pointInRect(exitGameButton, mx, my)) {
            surrendered = true;
            menuOpen = false;
            drag.active = false;
            return;
        }

        if (menuOpen) {
            menuOpen = false;
        }
    }

    if (surrendered) return;

    // -------------------------
    // Targeting mode
    // -------------------------
    if (pendingAction.active) {
        if (event.type == SDL_MOUSEBUTTONDOWN &&
            event.button.button == SDL_BUTTON_LEFT) {
            resolvePendingActionAt(mx, my);
        }
        return;
    }

    // -------------------------
    // Card Dragging
    // -------------------------
    switch (event.type) {

        case SDL_MOUSEBUTTONDOWN:
            if (event.button.button == SDL_BUTTON_LEFT) {
                for (int i = static_cast<int>(localPlayer.hand.size()) - 1;
                     i >= 0; --i) {

                    if (pointInRect(cardRects[i], mx, my)) {
                        drag.active = true;
                        drag.index = static_cast<std::size_t>(i);
                        drag.offsetX = mx - cardRects[i].x;
                        drag.offsetY = my - cardRects[i].y;
                        drag.x = cardRects[i].x;
                        drag.y = cardRects[i].y;
                        break;
                    }
                }
            }
            break;

        case SDL_MOUSEMOTION:
            if (drag.active) {
                drag.x = mx - drag.offsetX;
                drag.y = my - drag.offsetY;
            }
            break;

        case SDL_MOUSEBUTTONUP:
            if (drag.active &&
                event.button.button == SDL_BUTTON_LEFT) {

                int laneIndex = -1;
                for (std::size_t slot = 0;
                     slot < playSlots.size(); ++slot) {

                    if (pointInRect(playSlots[slot], mx, my)) {
                        laneIndex = static_cast<int>(slot);
                        break;
                    }
                }

                bool droppedInDiscard =
                    pointInRect(discardZone, mx, my);

                if (droppedInDiscard) {
                    auto& card = localPlayer.hand[drag.index];
                    std::cout << "[Playing] Attempting to Discard " << card->getName() << "\n";
                    authority->discardCard(card->getId());
                }
                else if (laneIndex >= 0) {
                    auto& card = localPlayer.hand[drag.index];
                    int laneIndex = -1;
                    for (std::size_t slot = 0; slot < playSlots.size(); ++slot) { //find out where card was played, zone wise
                        if (pointInRect(playSlots[slot], mx, my)) {
                            laneIndex = static_cast<int>(slot);
                            break;
                        }
                    }
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

        default:
            break;
    }
}

// -------------------------
// Main loop
// -------------------------
void Playing::run() {
    if (!renderer)
        throw std::runtime_error("Call setup() first");

    SDL_Event event;

    //check localplayer deck
    localPlayer.deck.toString();


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

    animationQueue.update(SDL_GetTicks());
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

    //check if deck has card
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

    // Animate only local draws
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
    playSlots.clear();       // local player zones
    opponentSlots.clear();   // opponent zones

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
    const int opponentOffset = 200; // vertical offset for opponent zones

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
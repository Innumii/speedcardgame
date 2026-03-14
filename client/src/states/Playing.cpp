#include "states/Playing.hpp"

#include "core/Game.hpp"
#include "render/RenderPlaying.hpp"
#include "render/RenderText.hpp"
#include "render/Theme.hpp"
#include "gameplay/LocalAuthority.hpp"
#include "utils/PlayingLayoutUtil.hpp"
#include "utils/RenderUtil.hpp"

#include <SDL2/SDL.h>
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <objects/CreatureCard.h>
// #include <functional>

// -------------------------
// Helpers
// -------------------------

bool Playing::resolvePendingActionAt(int x, int y) {
    if (!pendingAction.active)
        return false;

    int targetLane = -1;
    int targetIndex = -1;

    // Check local lanes first
    for (std::size_t slot = 0; slot < playSlots.size(); ++slot) {
        if (RenderUtil::pointInRect(playSlots[slot], x, y)) {
            targetLane = static_cast<int>(slot);
            targetIndex = 0;
            break;
        }
    }

    // Check opponent lanes (offset by render, e.g., 200 px above)
    for (std::size_t slot = 0; slot < opponentSlots.size(); ++slot) {
        if (RenderUtil::pointInRect(opponentSlots[slot], x, y)) {
            targetLane = static_cast<int>(slot);
            targetIndex = 1;
            break;
        }
    }

    if (targetLane >= 0) {
        std::cout<< "[Playing] Casting at targetIndex: " + std::to_string(targetIndex) + "\n";
        std::cout<< "[Playing] Casting at targetLane: " + std::to_string(targetLane) + "\n";

        authority->playCard(
            static_cast<int>(pendingAction.cardId),
            pendingAction.sourceLane, // where the spell was dropped
            targetLane,               // target lane
            targetIndex              // target player (local or opponent)
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

    return PlayingLayoutUtil::computeDeckRect(
        playSlots,
        screenW,
        Theme::Playing::CARD_WIDTH,
        Theme::Playing::CARD_HEIGHT,
        Theme::Playing::SELF_DECK_GAP,
        Theme::Playing::SIDE_ZONE_MARGIN
    );
}

bool Playing::tryDrawCardWithAnimation(Uint32 now) {
    if (localPlayer.hand.size() >= Theme::Playing::MAX_HAND_SIZE) {
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
            if (RenderUtil::pointInRect(returnToTitleButton, mouseX, mouseY)) {
                surrendered = false;
                pauseModalOpen = false;
                exitModalOpen = false;
                game.setNextState(GameState::Title);
            }
            return;
        }

        if (exitModalOpen) {
            if (RenderUtil::pointInRect(saveExitButton, mouseX, mouseY)) {
                surrendered = true;
                pauseModalOpen = false;
                exitModalOpen = false;
                return;
            }
            if (RenderUtil::pointInRect(noSaveExitButton, mouseX, mouseY)) {
                exitModalOpen = false;
                pauseModalOpen = true;
                return;
            }
            if (!RenderUtil::pointInRect(exitModal, mouseX, mouseY)) {
                exitModalOpen = false;
                pauseModalOpen = true;
                return;
            }
            return;
        }

        if (pauseModalOpen) {
            if (RenderUtil::pointInRect(resumeButton, mouseX, mouseY)) {
                pauseModalOpen = false;
                return;
            }
            if (RenderUtil::pointInRect(pauseExitButton, mouseX, mouseY)) {
                pauseModalOpen = false;
                exitModalOpen = true;
                return;
            }
            if (!RenderUtil::pointInRect(pauseModal, mouseX, mouseY)) {
                pauseModalOpen = false;
            }
            return;
        }

        if (RenderUtil::pointInRect(menuButton, mouseX, mouseY)) {
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
                
                for (int i = static_cast<int>(cardRects.size()) - 1; i >= 0; --i) {
                    if (RenderUtil::pointInRect(cardRects[static_cast<std::size_t>(i)], mx, my)) {
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
                    if (RenderUtil::pointInRect(playSlots[slot], releaseX, releaseY)) {
                        laneIndex = static_cast<int>(slot);
                        break;
                    }
                }

                bool droppedInDiscard = RenderUtil::pointInRect(discardZone, releaseX, releaseY);

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
                        if (localPlayer.mana >= card->getManaCost()) {
                            pendingAction.active = true;
                            pendingAction.cardId = card->getId();
                            pendingAction.sourceLane = laneIndex; // store where it was dropped
                        } //Check if player has sufficient mana

                    }
                }

                drag.active = false;
            }
            break;
            
        case SDL_MOUSEWHEEL:
            previewScrollOffset = 0;
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

    const bool draggingCard = drag.active && drag.index < localPlayer.hand.size();
    std::size_t newHoverIndex = static_cast<std::size_t>(-1);
    
    if (!draggingCard) {
        for (int i = static_cast<int>(cardRects.size()) - 1; i >= 0; --i) {
            if (RenderUtil::pointInRect(cardRects[static_cast<std::size_t>(i)], mouseX, mouseY)) {
                newHoverIndex = static_cast<std::size_t>(i);
                break;
            }
        }
    }

    if (newHoverIndex != hoverIndex) {
        hoverIndex = newHoverIndex;
        hoverStartTick = now;
        previewScrollOffset = 0;
        previewLocked = false;
    }

    constexpr Uint32 hoverDelayMs = 1000;
    
    const bool hoverTimerReady =
        hoverIndex != static_cast<std::size_t>(-1) &&
        hoverIndex < localPlayer.hand.size() &&
        now - hoverStartTick >= hoverDelayMs;

    previewLocked = false;
}

//need to refactor into command map at some point. Current method is repetitive
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
            // std::cout<< "[Playing] Casting!\n";
            std::optional<int> targetLane;
            if (iss.peek() != EOF) {
                int tmp;
                if (iss >> tmp) targetLane = tmp;
            }

            std::optional<int> targetIndex;
            if (iss.peek() != EOF) {
                int tmp;
                if (iss >> tmp) targetIndex = tmp;
            }

            // Move card into spell handler
            playSpell(playerId, std::move(*it), lane, targetLane, targetIndex);
            player.hand.erase(it);
        }
    } else if (cmd == "COMBAT") { //use this to call rendering for the cards attacking each other
        int playerAId, playerBId, lane, powerA, powerB;
        iss >> playerAId >> playerBId >> lane >> powerA >> powerB;
        resolveLaneCombat(playerAId, playerBId, lane, powerA, powerB);

    } else if (cmd == "DIRECT") { //use this to call rendering for direct attack
        int playerId, lane, damage;
        iss >> playerId >> lane >> damage;
        resolveDirectCombat(playerId, lane, damage);

    } else if (cmd == "AUGMENT") { //use this to modify the power/toughness of the cards
        int playerId, lane, powerDelta, toughnessDelta;
        iss >> playerId >> lane >> powerDelta >> toughnessDelta;
        augmentCreature(playerId, lane, powerDelta, toughnessDelta);

    } else if (cmd == "DESTROY") { //use this to remove cards from the board
        int playerId, lane;
        iss >> playerId >> lane;
        destroyCreature(playerId, lane);

    } else if (cmd == "HP") { //use this to modify player health
        int playerId, delta;
        iss >> playerId >> delta;
        augmentHP(playerId, delta);

    } //add a mana command later -> decouple discard logic to offload to mana logic

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
    std::cout << "[Playing] Summoned " << name
              << " for " << playerId 
              << " at lane " << lane << "\n";
}

void Playing::playSpell(int playerId, std::unique_ptr<Card> card, int sourceLane, std::optional<int> targetLane, std::optional<int> targetIndex) {
    Player& player = (playerId == localPlayer.id) ? localPlayer : remotePlayer;
    if (sourceLane < 0 || sourceLane >= board.getLaneCount()) return;
    if (!card) return;

    if (card->getType() != CardType::Spell) return;
    // std::cout<< "[Playing] Casting from playSpell!\n";

    std::string name = card->getName();
    player.mana -= card->getManaCost();

    // board.addToPlay(sourceLane, boardIndex, std::move(card));

    //reassign targetIndex value, relative to client instance
    //targetIndex value received is relative to the caster's client. If the caster is opponent, need to reverse the values
    if (playerId != localPlayer.id) {
        if (targetIndex == 0) targetIndex = 1;
        else if (targetIndex == 1) targetIndex = 0;
    }

    std::string side = (targetIndex == 0) ? "user's side" : "opponent's side";
    std::cout << "[Playing] Casted " << name
              << " by player " << playerId 
              << " at lane " << sourceLane 
              << " on " << side << "\n";
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

void Playing::augmentCreature(int playerId, int lane, int powerDelta, int toughnessDelta) {
    int boardIndex = (playerId == localPlayer.id) ? 0 : 1;
    const auto& zone = board.getZone(lane, boardIndex);

    if (!zone.has_value()) {
        std::cerr << "[augmentCreature] No creature in lane " << lane << "\n";
        return;
    }

    Card* card = zone.value().get();

    CreatureCard* creature = static_cast<CreatureCard*>(card);
    creature->augmentStats(powerDelta, toughnessDelta);
}

void Playing::destroyCreature(int playerId, int lane) {
    int boardIndex = (playerId == localPlayer.id) ? 0 : 1;

    std::unique_ptr<Card> card;
    if (!board.removeFromPlay(lane, boardIndex, card)) {
        std::cout << "[destroyCreature] No card on lane " << lane
                  << " for player " << playerId << "\n";
        return;
    }

    // Optionally move to discard
    // board.addToDiscard(std::move(card), boardIndex);
}

void Playing::resolveLaneCombat(int playerAId, int playerBId, int lane, int powerA, int powerB) {
    //can call animation here ig
}
void Playing::resolveDirectCombat(int playerId, int lane, int damage) {
    //can call animation here ig
}
void Playing::augmentHP(int playerId, int delta) {
    //get player object reference, change HP according to delta
    Player& player = (playerId == localPlayer.id) ? localPlayer : remotePlayer;
    player.health += delta;
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

    const int cardWidth = Theme::Playing::CARD_WIDTH;
    const int cardHeight = Theme::Playing::CARD_HEIGHT;
    const int maxWidth = static_cast<int>(screenW * Theme::Playing::HAND_MAX_WIDTH_RATIO);
    
    int totalWidthNoOverlap = static_cast<int>(count) * cardWidth;
    
    int spacing = Theme::Playing::HAND_DEFAULT_SPACING;
    if (totalWidthNoOverlap > maxWidth && count > 1) {
        spacing = (maxWidth - totalWidthNoOverlap) / static_cast<int>(count - 1);
    }

    int finalHandWidth = (static_cast<int>(count) * cardWidth) + (static_cast<int>(count - 1) * spacing);
    
    int startX = (screenW - finalHandWidth) / 2;
    int startY = screenH - cardHeight - Theme::Playing::HAND_Y_OFFSET;
    
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

    const int handCardHeight = Theme::Playing::CARD_HEIGHT;
    const int handYOffset = Theme::Playing::HAND_Y_OFFSET;
    const int slotCount = Theme::Playing::SLOT_COUNT;
    const int slotWidth = Theme::Playing::SLOT_WIDTH;
    const int slotHeight = Theme::Playing::SLOT_HEIGHT;
    const int slotSpacing = Theme::Playing::SLOT_SPACING;
    const int margin = Theme::Playing::SCREEN_MARGIN;
    const int opponentOffset = Theme::Playing::OPPONENT_ZONE_OFFSET;

    // Hand Y position
    int handY = screenH - handCardHeight - handYOffset;
    if (!cardRects.empty()) {
        handY = cardRects.front().y;
    }

    int totalSlotsWidth = slotCount * slotWidth + (slotCount - 1) * slotSpacing;
    int startX = std::max(margin, (screenW - totalSlotsWidth) / 2);

    // Local player slots
    int slotY = handY - slotHeight - Theme::Playing::SLOT_TO_HAND_GAP;
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

    discardZone = PlayingLayoutUtil::computeDiscardRect(
        playSlots,
        Theme::Playing::CARD_WIDTH,
        Theme::Playing::CARD_HEIGHT,
        Theme::Playing::SELF_DECK_GAP,
        Theme::Playing::SCREEN_MARGIN
    );
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

    const int margin = Theme::Playing::SCREEN_MARGIN;
    
    // Menu button - TOP RIGHT
    const int menuW = Theme::Playing::MENU_BUTTON_WIDTH;
    const int menuH = Theme::Playing::MENU_BUTTON_HEIGHT;
    menuButton = SDL_Rect{screenW - menuW - margin, margin, menuW, menuH};
    
    const int pauseModalW = Theme::Playing::PAUSE_MODAL_WIDTH;
    const int pauseModalH = Theme::Playing::PAUSE_MODAL_HEIGHT;
    pauseModal = SDL_Rect{(screenW - pauseModalW) / 2, (screenH - pauseModalH) / 2, pauseModalW, pauseModalH};
    
    const int buttonW = Theme::Playing::PAUSE_BUTTON_WIDTH;
    const int buttonH = Theme::Playing::PAUSE_BUTTON_HEIGHT;
    const int buttonSpacing = Theme::Playing::PAUSE_BUTTON_SPACING;
    const int firstButtonY = pauseModal.y + Theme::Playing::PAUSE_BUTTON_TOP;
    
    resumeButton = SDL_Rect{(screenW - buttonW) / 2, firstButtonY, buttonW, buttonH};
    pauseExitButton = SDL_Rect{(screenW - buttonW) / 2, firstButtonY + buttonH + buttonSpacing, buttonW, buttonH};
    
    const int exitModalW = Theme::Playing::EXIT_MODAL_WIDTH;
    const int exitModalH = Theme::Playing::EXIT_MODAL_HEIGHT;
    exitModal = SDL_Rect{(screenW - exitModalW) / 2, (screenH - exitModalH) / 2, exitModalW, exitModalH};
    
    const int exitButtonW = Theme::Playing::EXIT_BUTTON_WIDTH;
    const int exitButtonH = Theme::Playing::EXIT_BUTTON_HEIGHT;
    const int exitButtonSpacing = Theme::Playing::EXIT_BUTTON_SPACING;
    const int exitFirstButtonY = exitModal.y + Theme::Playing::EXIT_BUTTON_TOP;
    
    saveExitButton = SDL_Rect{(screenW - exitButtonW * 2 - exitButtonSpacing) / 2, exitFirstButtonY, exitButtonW, exitButtonH};
    noSaveExitButton = SDL_Rect{saveExitButton.x + exitButtonW + exitButtonSpacing, exitFirstButtonY, exitButtonW, exitButtonH};
    
    const int returnW = Theme::Playing::RETURN_BUTTON_WIDTH;
    const int returnH = Theme::Playing::RETURN_BUTTON_HEIGHT;
    returnToTitleButton = SDL_Rect{(screenW - returnW) / 2, (screenH / 2) + 24, returnW, returnH};
}
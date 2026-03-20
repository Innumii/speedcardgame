#include "states/Playing.hpp"

#include "core/Game.hpp"
#include "animation/AnimationGroup.hpp"
#include "animation/AttackAnimation.hpp"
#include "animation/DeathAnimation.hpp"
#include "animation/DrawCardAnimation.hpp"
#include "render/RenderPlaying.hpp"
#include "render/RenderText.hpp"
#include "render/Theme.hpp"
#include "gameplay/LocalAuthority.hpp"
#include "utils/GetValidTargets.hpp"
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

    auto cardIt = std::find_if(
        localPlayer.hand.begin(),
        localPlayer.hand.end(),
        [&](const std::unique_ptr<Card>& card) {
            return card && card->getId() == pendingAction.cardId;
        }
    );

    if (cardIt == localPlayer.hand.end() || !*cardIt) {
        pendingAction.clear();
        return false;
    }

    const std::vector<int> validTargets = getValidTargets(*this, **cardIt, pendingAction.sourceLane);

    int targetLane = -1;
    int targetIndex = -1;
    int clickedTarget = -9999;

    int screenW = 0;
    int screenH = 0;
    if (renderer && SDL_GetRendererOutputSize(renderer, &screenW, &screenH) != 0) {
        screenW = 800;
        screenH = 600;
    }

    std::vector<SDL_Rect> opponentHandRects = computeCardLayout(remotePlayer.hand.size(), screenW, screenH);
    if (!opponentHandRects.empty() && !opponentSlots.empty()) {
        const int cardHeight = opponentHandRects.front().h;
        const int topHandY = PlayingLayoutUtil::computeOpponentHandY(
            opponentSlots,
            cardHeight,
            Theme::Playing::PREVIEW_MARGIN
        );
        for (auto& rect : opponentHandRects) {
            rect.y = topHandY;
        }
    }

    const SDL_Rect opponentPlayerRect{
        (screenW - Theme::Playing::OPPONENT_BAR_WIDTH) / 2,
        Theme::Playing::OPPONENT_BAR_TOP,
        Theme::Playing::OPPONENT_BAR_WIDTH,
        Theme::Playing::OPPONENT_BAR_HEIGHT
    };

    const SDL_Rect localPlayerRect{
        (screenW - Theme::Playing::PLAYER_BAR_WIDTH) / 2,
        screenH - Theme::Playing::PLAYER_BAR_HEIGHT - Theme::Playing::PLAYER_BAR_BOTTOM_MARGIN,
        Theme::Playing::PLAYER_BAR_WIDTH,
        Theme::Playing::PLAYER_BAR_HEIGHT
    };

    for (std::size_t i = 0; i < opponentHandRects.size(); ++i) {
        if (RenderUtil::pointInRect(opponentHandRects[i], x, y)) {
            clickedTarget = static_cast<int>(i);
            break;
        }
    }

    if (clickedTarget == -9999) {
        // Check local lanes first
        for (std::size_t slot = 0; slot < playSlots.size(); ++slot) {
            if (RenderUtil::pointInRect(playSlots[slot], x, y)) {
                clickedTarget = 100 + static_cast<int>(slot);
                break;
            }
        }
    }

    if (clickedTarget == -9999) {
        // Check opponent lanes
        for (std::size_t slot = 0; slot < opponentSlots.size(); ++slot) {
            if (RenderUtil::pointInRect(opponentSlots[slot], x, y)) {
                clickedTarget = 200 + static_cast<int>(slot);
                break;
            }
        }
    }

    if (clickedTarget == -9999 && RenderUtil::pointInRect(localPlayerRect, x, y)) {
        clickedTarget = -1;
    }

    if (clickedTarget == -9999 && RenderUtil::pointInRect(opponentPlayerRect, x, y)) {
        clickedTarget = -2;
    }

    const bool isValidTarget = std::find(validTargets.begin(), validTargets.end(), clickedTarget) != validTargets.end();
    if (!isValidTarget) {
        // Cancel targeting on invalid click; card was never removed from hand.
        pendingAction.clear();
        return false;
    }

    if (clickedTarget >= 100 && clickedTarget < 200) {
        targetLane = clickedTarget - 100;
        targetIndex = 0;
    } else if (clickedTarget >= 200 && clickedTarget < 300) {
        targetLane = clickedTarget - 200;
        targetIndex = 1;
    } else if (clickedTarget == -1) {
        targetLane = pendingAction.sourceLane;
        targetIndex = 0;
    } else if (clickedTarget == -2) {
        targetLane = pendingAction.sourceLane;
        targetIndex = 1;
    }

    if (targetLane >= 0 && targetIndex >= 0) {
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

    pendingAction.clear();
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
    board = Board();
    state = PlayingGameState::Playing;

    renderer = game.getRenderer();
    if (!renderer)
        throw std::runtime_error("Renderer not available");

    drag = DragState{};
    hoverIndex = static_cast<std::size_t>(-1);
    hoverStartTick = 0;
    menuOpen = false;
    pauseModalOpen = false;
    exitModalOpen = false;
    // surrendered = false;
    animationQueue.clear();
    pendingDestroys.clear();
    pendingAction.clear();
    running = true;
    lastDrawTick = SDL_GetTicks();
    combatCycleStartTick = lastDrawTick;
    lastCombatSyncTick = 0;

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
        animationQueue.enqueue(std::make_shared<DrawCardAnimation>(fromRect, cardRects[handIndex], handIndex, 320U));
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

        if (state != PlayingGameState::Playing){
            if (RenderUtil::pointInRect(returnToTitleButton, mouseX, mouseY)) {
                // surrendered = false;
                pauseModalOpen = false;
                exitModalOpen = false;
                game.setNextState(GameState::Title);
            } else if (RenderUtil::pointInRect(requeueButton, mouseX, mouseY)) {
                authority->queue();
                pauseModalOpen = false;
                exitModalOpen = false;
                game.setNextState(GameState::Waiting);
            }
            return;
        }

        if (exitModalOpen) {
            if (RenderUtil::pointInRect(saveExitButton, mouseX, mouseY)) {
                authority->surrender();
                // surrendered = true;
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

    if (state != PlayingGameState::Playing) return;

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
                    } else if (card->getType() == CardType::Spell) {
                        if (localPlayer.mana >= card->getManaCost()) {

                            auto validTargets = getValidTargets(*this, *card, pendingAction.sourceLane);
                            if (!validTargets.empty()) {
                                int allTarget = 0;
                                switch (validTargets[0]) {
                                    case 901: // All cards on board
                                        allTarget = -1;
                                        break;
                                    case 902: // All your cards
                                        allTarget = -2;
                                        break;
                                    case 903: // All opponent cards
                                        allTarget = -3;
                                        break;
                                    default:
                                        break;
                                }
                                if (allTarget != 0) {
                                    authority->playCard(
                                        card->getId(),
                                        laneIndex,
                                        -1, // no target lane
                                        allTarget  // no target player
                                    );
                                } else {
                                    pendingAction.active = true;
                                    pendingAction.cardId = card->getId();
                                    pendingAction.sourceLane = laneIndex;
                                }
                            } 
                        }
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
    if (state != PlayingGameState::Playing) return;

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
    processPendingDestroys(now);

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

    const auto syncCombatTimeline = [this]() {
        const Uint32 now = SDL_GetTicks();

        // Gate re-syncs so multiple lane COMBAT messages in the same phase do not jitter the UI.
        if (lastCombatSyncTick != 0 && now - lastCombatSyncTick < (COMBAT_CYCLE_DURATION_MS / 2U)) {
            return;
        }

        combatCycleStartTick =
            (now > COMBAT_PREPHASE_DURATION_MS) ? (now - COMBAT_PREPHASE_DURATION_MS) : 0;
        lastCombatSyncTick = now;
    };

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
        syncCombatTimeline();
        resolveLaneCombat(playerAId, playerBId, lane, powerA, powerB);

    } else if (cmd == "DIRECT") { //use this to call rendering for direct attack
        int playerId, lane, damage;
        iss >> playerId >> lane >> damage;
        syncCombatTimeline();
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

    }
    //add a mana command later -> decouple discard logic to offload to mana logic
    else if (cmd == "MATCH_LOST") {
        state = PlayingGameState::Lost;
        std::cout << "[Playing] Lost Match!\n";
    } else if (cmd == "MATCH_WON") {
        iss >> coinReward;
        state = PlayingGameState::Won;
        std::cout << "[Playing] Won Match!\n";
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
        animationQueue.enqueue(std::make_shared<DrawCardAnimation>(fromRect, cardRects[handIndex], handIndex, 320U));
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
    std::cout << "[augmentCreature] Augmenting " << std::to_string(powerDelta) << "/" << std::to_string(toughnessDelta) << "\n";
    CreatureCard* creature = static_cast<CreatureCard*>(card);
    creature->augmentStats(powerDelta, toughnessDelta);
}

void Playing::destroyCreature(int playerId, int lane) {
    int boardIndex = (playerId == localPlayer.id) ? 0 : 1;

    if (lane < 0 || lane >= board.getLaneCount()) {
        return;
    }

    auto removeAndAnimate = [&](int targetBoardIndex, int targetLane) {
        std::unique_ptr<Card> card;
        if (!board.removeFromPlay(targetLane, targetBoardIndex, card)) {
            std::cout << "[destroyCreature] No card on lane " << targetLane
                      << " for player " << playerId << "\n";
            return;
        }

        animationQueue.enqueue(std::make_shared<DeathAnimation>(
            targetLane,
            targetBoardIndex == 0,
            playSlots,
            opponentSlots,
            260U
        ));
    };

    const auto& targetZone = board.getZone(lane, boardIndex);
    if (!targetZone.has_value() || !targetZone.value()) {
        return;
    }

    const int opposingBoardIndex = (boardIndex == 0) ? 1 : 0;
    const auto& opposingZone = board.getZone(lane, opposingBoardIndex);
    const bool contestedLane = opposingZone.has_value() && static_cast<bool>(opposingZone.value());

    if (!contestedLane) {
        removeAndAnimate(boardIndex, lane);
        return;
    }

    const bool alreadyQueued = std::any_of(
        pendingDestroys.begin(),
        pendingDestroys.end(),
        [&](const PendingDestroyState& pending) {
            return pending.boardIndex == boardIndex && pending.lane == lane;
        }
    );

    if (alreadyQueued) {
        return;
    }

    static constexpr Uint32 destroyDelayMs = 520U;
    pendingDestroys.push_back(PendingDestroyState{boardIndex, lane, SDL_GetTicks() + destroyDelayMs});
}

void Playing::processPendingDestroys(Uint32 now) {
    if (pendingDestroys.empty()) {
        return;
    }

    std::vector<PendingDestroyState> stillPending;
    stillPending.reserve(pendingDestroys.size());

    for (const PendingDestroyState& pending : pendingDestroys) {
        if (now < pending.executeAt) {
            stillPending.push_back(pending);
            continue;
        }

        std::unique_ptr<Card> card;
        if (!board.removeFromPlay(pending.lane, pending.boardIndex, card)) {
            continue;
        }

        animationQueue.enqueue(std::make_shared<DeathAnimation>(
            pending.lane,
            pending.boardIndex == 0,
            playSlots,
            opponentSlots,
            260U
        ));
    }

    pendingDestroys.swap(stillPending);
}

void Playing::resolveLaneCombat(int playerAId, int playerBId, int lane, int powerA, int powerB) {
    (void)powerA;
    (void)powerB;

    if (lane < 0) {
        return;
    }

    const int boardIndexA = (playerAId == localPlayer.id) ? 0 : 1;
    const int boardIndexB = (playerBId == localPlayer.id) ? 0 : 1;

    const std::vector<SDL_Rect>& slotsA = (boardIndexA == 0) ? playSlots : opponentSlots;
    const std::vector<SDL_Rect>& slotsB = (boardIndexB == 0) ? playSlots : opponentSlots;

    const std::size_t laneIndex = static_cast<std::size_t>(lane);
    if (laneIndex >= slotsA.size() || laneIndex >= slotsB.size()) {
        return;
    }

    const auto& creatureA = board.getZone(lane, boardIndexA);
    const auto& creatureB = board.getZone(lane, boardIndexB);
    if (!creatureA.has_value() || !creatureA.value() || !creatureB.has_value() || !creatureB.value()) {
        return;
    }

    auto attackGroup = std::make_shared<AnimationGroup>();
    attackGroup->add(std::make_shared<AttackAnimation>(
        lane,
        boardIndexA == 0,
        playSlots,
        opponentSlots,
        420U
    ));
    attackGroup->add(std::make_shared<AttackAnimation>(
        lane,
        boardIndexB == 0,
        playSlots,
        opponentSlots,
        420U
    ));
    animationQueue.enqueue(attackGroup);
}
void Playing::resolveDirectCombat(int playerId, int lane, int damage) {
    (void)damage;

    if (!renderer || lane < 0) {
        return;
    }

    const int boardIndex = (playerId == localPlayer.id) ? 0 : 1;
    const std::vector<SDL_Rect>& sourceSlots = (boardIndex == 0) ? playSlots : opponentSlots;

    const std::size_t laneIndex = static_cast<std::size_t>(lane);
    if (laneIndex >= sourceSlots.size()) {
        return;
    }

    int screenW = 0;
    int screenH = 0;
    if (SDL_GetRendererOutputSize(renderer, &screenW, &screenH) != 0) {
        return;
    }

    SDL_Rect targetRect{};
    if (boardIndex == 0) {
        targetRect = SDL_Rect{
            (screenW - Theme::Playing::OPPONENT_BAR_WIDTH) / 2,
            Theme::Playing::OPPONENT_BAR_TOP,
            Theme::Playing::OPPONENT_BAR_WIDTH,
            Theme::Playing::OPPONENT_BAR_HEIGHT
        };
    } else {
        targetRect = SDL_Rect{
            (screenW - Theme::Playing::PLAYER_BAR_WIDTH) / 2,
            screenH - Theme::Playing::PLAYER_BAR_HEIGHT - Theme::Playing::PLAYER_BAR_BOTTOM_MARGIN,
            Theme::Playing::PLAYER_BAR_WIDTH,
            Theme::Playing::PLAYER_BAR_HEIGHT
        };
    }

    animationQueue.enqueue(std::make_shared<AttackAnimation>(
        lane,
        boardIndex == 0,
        playSlots,
        opponentSlots,
        420U,
        &targetRect
    ));
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
        requeueButton = SDL_Rect{0,0,0,0};
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
    const int spacing = 20;

    const int totalWidth = returnW * 2 + spacing;
    const int startX = (screenW - totalWidth) / 2;
    const int y = (screenH / 2) + 24;

    returnToTitleButton = SDL_Rect{
        startX,
        y,
        returnW,
        returnH
    };

    requeueButton = SDL_Rect{
        startX + returnW + spacing,
        y,
        returnW,
        returnH
    };

}

PlayingGameState Playing::getState() const {
    return state;
}

int Playing::getCoinReward() const {
    return coinReward;
}

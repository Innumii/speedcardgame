#include "states/Playing.hpp"

#include "core/Game.hpp"
#include "animation/AnimationGroup.hpp"
#include "animation/AttackAnimation.hpp"
#include "animation/DeathAnimation.hpp"
#include "animation/DiscardAnimation.hpp"
#include "animation/DrawCardAnimation.hpp"
#include "featureFlag/AnimationFlag.hpp"
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

namespace {

std::string effectBitToLabel(int effectBit) {
    switch (effectBit) {
        case 1: return "Double Strike";
        case 2: return "Trample";
        case 4: return "Deathtouch";
        case 8: return "Regen";
        default: return "Effect(" + std::to_string(effectBit) + ")";
    }
}

}

const Card* Playing::findPendingActionCard() const {
    if (!pendingAction.active) {
        return nullptr;
    }

    auto cardIt = std::find_if(
        localPlayer.hand.begin(),
        localPlayer.hand.end(),
        [&](const std::unique_ptr<Card>& card) {
            return card && card->getId() == pendingAction.cardId;
        }
    );

    if (cardIt == localPlayer.hand.end() || !*cardIt) {
        return nullptr;
    }

    return cardIt->get();
}

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

    auto validTargetsOpt = getValidTargets(*this, **cardIt, pendingAction.sourceLane);
    if (!validTargetsOpt.has_value()) {
        return false;
    }
    const std::vector<int>& validTargets = *validTargetsOpt;

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
        targetLane = -1;
        targetIndex = 0;
    } else if (clickedTarget == -2) {
        targetLane = -1;
        targetIndex = 1;
    }

    if (targetIndex >= 0) {
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
    boardHoverLane = -1;
    boardHoverIndex = -1;
    boardHoverStartTick = 0;
    menuOpen = false;
    pauseModalOpen = false;
    exitModalOpen = false;
    // surrendered = false;
    animationQueue.clear();
    pendingDestroys.clear();
    pendingAction.clear();
    recentSpellPreview.reset();
    recentSpellPreviewUntil = 0;
    recentSpellPreviewStartTick = 0;
    animationsEnabled = AnimationFlag::getAnimationsEnabled();
    running = true;
    lastDrawTick = SDL_GetTicks();
    combatCycleStartTick = lastDrawTick;
    lastCombatSyncTick = 0;
    deferredStatUpdates.clear(); // add this

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
                // exitModalOpen = false;
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
                    if (animationsEnabled) {
                        SDL_Rect animRect = cardRects[drag.index];
                        animRect.x = drag.x;
                        animRect.y = drag.y;
                        DiscardAnimation::stagePending(animRect, card->getId(), 500U, card->clone());
                        DiscardAnimation::stagePending(animRect, card->getId(), 500U, card->clone());
                    }
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

                            auto validTargetsOpt = getValidTargets(*this, *card, pendingAction.sourceLane);
                            if (!validTargetsOpt.has_value()) {
                                authority->playCard(
                                        card->getId(),
                                        laneIndex,
                                        -1, // no target lane
                                        -1  // no target player
                                    );
                            } else {
                                const std::vector<int>& validTargets = *validTargetsOpt;
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

        if (animationsEnabled) {
            animationQueue.update(SDL_GetTicks());
        }
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
    if (animationsEnabled) {
        animationQueue.update(now);
    }
    processPendingDestroys(now);
    flushDeferredStatUpdatesIfReady();

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

    if (recentSpellPreviewUntil != 0 && now >= recentSpellPreviewUntil) {
        recentSpellPreview.reset();
        recentSpellPreviewUntil = 0;
        recentSpellPreviewStartTick = 0;
    }
    DiscardAnimation::cleanupStalePending(now);
}

void Playing::flushDeferredStatUpdatesIfReady() {
    if (deferredStatUpdates.empty()) {
        return;
    }

    if (animationsEnabled && animationQueue.hasActiveAnimation()) {
        return;
    }

    combatUpdateBarrierActive = false;

    auto updates = deferredStatUpdates;
    deferredStatUpdates.clear();
    for (const auto& msg : updates) {
        handleServerMessage(msg);
    }
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
    
    // Stat updates are applied only after combat animation work settles.
    static const std::vector<std::string> statUpdateCmds = {"AUGMENT", "DESTROY", "HP", "EFFECT_ADD", "EFFECT_REMOVE", "REGEN_SET", "MATCH_WON", "MATCH_LOST"};
    if (std::find(statUpdateCmds.begin(), statUpdateCmds.end(), cmd) != statUpdateCmds.end()) {
        if (animationsEnabled && (combatUpdateBarrierActive || animationQueue.hasActiveAnimation())) {
            deferredStatUpdates.push_back(msg);
            return true;
        }
    }

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
        combatUpdateBarrierActive = animationsEnabled;
        syncCombatTimeline();
        resolveLaneCombat(playerAId, playerBId, lane, powerA, powerB);

    } else if (cmd == "DIRECT") { //use this to call rendering for direct attack
        int playerId, lane, damage;
        iss >> playerId >> lane >> damage;
        combatUpdateBarrierActive = animationsEnabled;
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

    } else if (cmd == "MANA") { //use this to modify player health
        int playerId, delta;
        iss >> playerId >> delta;
        augmentMana(playerId, delta); 
    } else if (cmd == "EFFECT_ADD") {
        int playerId, lane, effectBit;
        iss >> playerId >> lane >> effectBit;

        int boardIndex = (playerId == localPlayer.id) ? 0 : 1;
        auto& zone = board.getZoneMutable(lane, boardIndex);
        if (zone.has_value() && zone.value()) {
            zone.value()->addGrantedEffect(effectBitToLabel(effectBit));
        }
    } else if (cmd == "EFFECT_REMOVE") {
        int playerId, lane, effectBit;
        iss >> playerId >> lane >> effectBit;

        int boardIndex = (playerId == localPlayer.id) ? 0 : 1;
        auto& zone = board.getZoneMutable(lane, boardIndex);
        if (zone.has_value() && zone.value()) {
            zone.value()->removeGrantedEffect(effectBitToLabel(effectBit));
        }
    } else if (cmd == "REGEN_SET") {
        int playerId, lane, regenValue;
        iss >> playerId >> lane >> regenValue;

        int boardIndex = (playerId == localPlayer.id) ? 0 : 1;
        auto& zone = board.getZoneMutable(lane, boardIndex);
        if (zone.has_value() && zone.value()) {
            zone.value()->addGrantedEffect("Regen " + std::to_string(regenValue));
        }
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

    recentSpellPreview = card->clone();
    recentSpellPreviewUntil = SDL_GetTicks() + Theme::Playing::SPELL_CAST_PREVIEW_DURATION_MS;
    recentSpellPreviewStartTick = SDL_GetTicks();

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
              << " targeting lane " << (targetLane.has_value() ? std::to_string(targetLane.value()) : "N/A")
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

    if (playerId != localPlayer.id) return true;

    cardRects = computeCardLayout(player.hand.size(), screenW, screenH);
    computeZones(screenW, screenH);
    const SDL_Rect fromRect = computeSelfDeckRect(screenW, screenH);
    const std::size_t handIndex = player.hand.size() - 1;

    if (animationsEnabled && playerId == localPlayer.id && handIndex < cardRects.size()) {
        animationQueue.enqueue(std::make_shared<DrawCardAnimation>(fromRect, cardRects[handIndex], handIndex, 320U));
    }

    // Recompute all draw destinations now that hand is fully populated
    animationQueue.updateDrawDestinations(cardRects);

    return true;
}

void Playing::discardCard(int playerId, int cardId) {
    Player* player = (playerId == localPlayer.id) ? &localPlayer : &remotePlayer;

    auto it = std::find_if(player->hand.begin(), player->hand.end(),
                           [cardId](const std::unique_ptr<Card>& c) {
                               return c->getId() == cardId;
                           });

    if (it == player->hand.end()) return;

    // Snapshot old rects before hand changes
    const std::vector<SDL_Rect> oldRects = cardRects;
    const std::size_t discardedIdx = static_cast<std::size_t>(
        std::distance(player->hand.begin(), it));

    if (animationsEnabled) {
        int screenW = 0, screenH = 0;
        SDL_GetRendererOutputSize(renderer, &screenW, &screenH);

        if (playerId == localPlayer.id) {
            auto anim = DiscardAnimation::takePending(cardId);
            if (!anim && discardedIdx < oldRects.size()) {
                anim = std::make_shared<DiscardAnimation>(oldRects[discardedIdx], cardId, 500U);
            }
            if (anim) animationQueue.enqueue(anim);
        } else {
            // Build scaled opponent hand rects the same way the renderer does
            auto oppRects = computeCardLayout(player->hand.size(), screenW, screenH);
            if (!oppRects.empty() && !opponentSlots.empty()) {
                const int cardHeight = oppRects.front().h;
                const int topHandY = PlayingLayoutUtil::computeOpponentHandY(
                    opponentSlots, cardHeight, Theme::Playing::PREVIEW_MARGIN);
                for (auto& r : oppRects) r.y = topHandY;
            }
            if (discardedIdx < oppRects.size()) {
                auto anim = std::make_shared<DiscardAnimation>(oppRects[discardedIdx], cardId, 500U);
                // Clone the card before it gets moved so the renderer can draw it
                if (discardedIdx < oppRects.size()) {
                    auto anim = std::make_shared<DiscardAnimation>(oppRects[discardedIdx], cardId, 500U);
                    animationQueue.enqueue(anim);
                }
                animationQueue.enqueue(anim);
            }
        }
    }

    std::unique_ptr<Card> cardToDiscard = std::move(*it);
    std::string name = cardToDiscard->getName();
    player->hand.erase(it);
    player->addMana(cardToDiscard->getManaValue());

    int boardIndex = (playerId == localPlayer.id) ? 0:1;
    board.addToDiscard(std::move(cardToDiscard), boardIndex);

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

        if (animationsEnabled) {
            animationQueue.enqueue(std::make_shared<DeathAnimation>(
                targetLane,
                targetBoardIndex == 0,
                playSlots,
                opponentSlots,
                260U
            ));
        }
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

    if (!animationsEnabled) {
        removeAndAnimate(boardIndex, lane);
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

        

        if (animationsEnabled) {
            animationQueue.enqueue(std::make_shared<DeathAnimation>(
                pending.lane,
                pending.boardIndex == 0,
                playSlots,
                opponentSlots,
                260U
            ));
        }
        std::unique_ptr<Card> card;
        if (!board.removeFromPlay(pending.lane, pending.boardIndex, card)) {
            continue;
        }
    }

    pendingDestroys.swap(stillPending);
}

void Playing::resolveLaneCombat(int playerAId, int playerBId, int lane, int powerA, int powerB) {
    (void)powerA;
    (void)powerB;

    if (lane < 0) {
        return;
    }

    if (!animationsEnabled) {
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

    if (animationsEnabled) {
        animationQueue.enqueue(std::make_shared<AttackAnimation>(
            lane,
            boardIndex == 0,
            playSlots,
            opponentSlots,
            420U,
            &targetRect
        ));
    }
}
void Playing::augmentHP(int playerId, int delta) {
    //get player object reference, change HP according to delta
    // std::cout << "[DEBUG] AUGMENTING OF " << delta << "\n";
    Player& player = (playerId == localPlayer.id) ? localPlayer : remotePlayer;
    player.health += delta;
}

void Playing::augmentMana(int playerId, int delta) {
    Player& player = (playerId == localPlayer.id) ? localPlayer : remotePlayer;
    player.mana += delta;
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

    const float scale = std::min(
        static_cast<float>(screenW) / 1200.0F,
        static_cast<float>(screenH) / 850.0F);

    // Shift the whole layout up so the content block is vertically centered,
    // not just bottom-anchored. Slack only exists when width limits the scale.
    const int verticalOffset = (screenH - static_cast<int>(850.0F * scale)) / 2;

    const int cardWidth   = static_cast<int>(Theme::Playing::CARD_WIDTH        * scale);
    const int cardHeight  = static_cast<int>(Theme::Playing::CARD_HEIGHT       * scale);
    const int handYOffset = static_cast<int>(Theme::Playing::HAND_Y_OFFSET     * scale);
    const int maxWidth    = static_cast<int>(screenW * Theme::Playing::HAND_MAX_WIDTH_RATIO);

    int totalWidthNoOverlap = static_cast<int>(count) * cardWidth;

    int spacing = static_cast<int>(Theme::Playing::HAND_DEFAULT_SPACING * scale);
    if (totalWidthNoOverlap > maxWidth && count > 1) {
        spacing = (maxWidth - totalWidthNoOverlap) / static_cast<int>(count - 1);
    }

    int finalHandWidth = (static_cast<int>(count) * cardWidth) + (static_cast<int>(count - 1) * spacing);

    int startX = (screenW - finalHandWidth) / 2;
    int startY = screenH - cardHeight - handYOffset - verticalOffset;

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
    opponentSlots.clear();

    if (screenW <= 0 || screenH <= 0) {
        discardZone = SDL_Rect{0, 0, 0, 0};
        return;
    }

    const float scale = std::min(
        static_cast<float>(screenW) / 1200.0F,
        static_cast<float>(screenH) / 850.0F);

    // Same centering offset as computeCardLayout — keeps slots aligned with hand.
    const int verticalOffset = (screenH - static_cast<int>(850.0F * scale)) / 2;

    const int handCardHeight  = static_cast<int>(Theme::Playing::CARD_HEIGHT          * scale);
    const int handYOffset     = static_cast<int>(Theme::Playing::HAND_Y_OFFSET         * scale);
    const int slotCount       = Theme::Playing::SLOT_COUNT;
    const int slotWidth       = static_cast<int>(Theme::Playing::SLOT_WIDTH            * scale);
    const int slotHeight      = static_cast<int>(Theme::Playing::SLOT_HEIGHT           * scale);
    const int slotSpacing     = static_cast<int>(Theme::Playing::SLOT_SPACING          * scale);
    const int slotToHandGap   = static_cast<int>(Theme::Playing::SLOT_TO_HAND_GAP      * scale);
    const int margin          = static_cast<int>(Theme::Playing::SCREEN_MARGIN         * scale);
    const int opponentOffset  = static_cast<int>(Theme::Playing::OPPONENT_ZONE_OFFSET  * scale);
    const int cardWidth       = static_cast<int>(Theme::Playing::CARD_WIDTH            * scale);
    const int deckGap         = static_cast<int>(Theme::Playing::SELF_DECK_GAP         * scale);
    const int sideMargin      = static_cast<int>(Theme::Playing::SIDE_ZONE_MARGIN      * scale);

    // Hand Y position
    int handY = screenH - handCardHeight - handYOffset - verticalOffset;
    if (!cardRects.empty()) {
        handY = cardRects.front().y;
    }

    int totalSlotsWidth = slotCount * slotWidth + (slotCount - 1) * slotSpacing;
    int startX = (screenW - totalSlotsWidth) / 2;

    // Local player slots — clamped so opponent slots never overlap the scaled stats bar.
    int slotY = handY - slotHeight - slotToHandGap;
    {
        const int statsBarBottom =
            static_cast<int>((Theme::Playing::OPPONENT_BAR_TOP +
                              Theme::Playing::PLAYER_BAR_HEIGHT) * scale)
            + margin + verticalOffset;
        slotY = std::max(slotY, statsBarBottom + opponentOffset);
    }

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

        SDL_Rect oppRect = localRect;
        oppRect.y -= opponentOffset;
        opponentSlots.push_back(oppRect);
    }

    discardZone = PlayingLayoutUtil::computeDiscardRect(
        playSlots,
        cardWidth,
        static_cast<int>(Theme::Playing::CARD_HEIGHT * scale),
        deckGap,
        sideMargin
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

    const float scale = std::min(
        static_cast<float>(screenW) / 1200.0F,
        static_cast<float>(screenH) / 850.0F);

    const int verticalOffset = (screenH - static_cast<int>(850.0F * scale)) / 2;

    const int margin = static_cast<int>(Theme::Playing::SCREEN_MARGIN * scale);

    // Menu button - TOP RIGHT
    const int menuW = static_cast<int>(Theme::Playing::MENU_BUTTON_WIDTH  * scale);
    const int menuH = static_cast<int>(Theme::Playing::MENU_BUTTON_HEIGHT * scale);
    menuButton = SDL_Rect{screenW - menuW - margin, margin + verticalOffset, menuW, menuH};

    const int pauseModalW = static_cast<int>(Theme::Playing::PAUSE_MODAL_WIDTH  * scale);
    const int pauseModalH = static_cast<int>(Theme::Playing::PAUSE_MODAL_HEIGHT * scale);
    pauseModal = SDL_Rect{(screenW - pauseModalW) / 2, (screenH - pauseModalH) / 2, pauseModalW, pauseModalH};

    const int buttonW       = static_cast<int>(Theme::Playing::PAUSE_BUTTON_WIDTH   * scale);
    const int buttonH       = static_cast<int>(Theme::Playing::PAUSE_BUTTON_HEIGHT  * scale);
    const int buttonSpacing = static_cast<int>(Theme::Playing::PAUSE_BUTTON_SPACING * scale);
    const int firstButtonY  = pauseModal.y + static_cast<int>(Theme::Playing::PAUSE_BUTTON_TOP * scale);

    resumeButton    = SDL_Rect{(screenW - buttonW) / 2, firstButtonY, buttonW, buttonH};
    pauseExitButton = SDL_Rect{(screenW - buttonW) / 2, firstButtonY + buttonH + buttonSpacing, buttonW, buttonH};

    const int exitModalW = static_cast<int>(Theme::Playing::EXIT_MODAL_WIDTH  * scale);
    const int exitModalH = static_cast<int>(Theme::Playing::EXIT_MODAL_HEIGHT * scale);
    exitModal = SDL_Rect{(screenW - exitModalW) / 2, (screenH - exitModalH) / 2, exitModalW, exitModalH};

    const int exitButtonW       = static_cast<int>(Theme::Playing::EXIT_BUTTON_WIDTH   * scale);
    const int exitButtonH       = static_cast<int>(Theme::Playing::EXIT_BUTTON_HEIGHT  * scale);
    const int exitButtonSpacing = static_cast<int>(Theme::Playing::EXIT_BUTTON_SPACING * scale);
    const int exitFirstButtonY  = exitModal.y + static_cast<int>(Theme::Playing::EXIT_BUTTON_TOP * scale);

    saveExitButton  = SDL_Rect{(screenW - exitButtonW * 2 - exitButtonSpacing) / 2, exitFirstButtonY, exitButtonW, exitButtonH};
    noSaveExitButton = SDL_Rect{saveExitButton.x + exitButtonW + exitButtonSpacing, exitFirstButtonY, exitButtonW, exitButtonH};

    const int returnW = static_cast<int>(Theme::Playing::RETURN_BUTTON_WIDTH  * scale);
    const int returnH = static_cast<int>(Theme::Playing::RETURN_BUTTON_HEIGHT * scale);
    const int spacing = static_cast<int>(20 * scale);

    const int totalWidth = returnW * 2 + spacing;
    const int startX = (screenW - totalWidth) / 2;
    const int y = (screenH / 2) + static_cast<int>(24 * scale) + verticalOffset;

    returnToTitleButton = SDL_Rect{startX,              y, returnW, returnH};
    requeueButton       = SDL_Rect{startX + returnW + spacing, y, returnW, returnH};
}

PlayingGameState Playing::getState() const {
    return state;
}

int Playing::getCoinReward() const {
    return coinReward;
}
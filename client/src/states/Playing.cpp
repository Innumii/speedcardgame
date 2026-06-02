#include "states/Playing.hpp"

#include "core/Game.hpp"
#include "core/Audio.hpp"
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
#include "utils/JsonUtil.hpp"

#include <SDL2/SDL.h>
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <objects/CreatureCard.h>
#include <utils/PlayingRenderUtil.hpp>
#include <animation/SummonAnimation.hpp>
#include <animation/FatigueAnimation.hpp>

// -------------------------
// Helpers
// -------------------------

namespace {

static constexpr std::pair<int, const char*> kEffectBits[] = {
    {  1, "Double Strike" },
    {  2, "Trample"       },
    {  4, "Deathtouch"    },
    {  8, "Regen"         },
    { 16, "Lifesteal"     },
};

std::string effectBitToLabel(int effectBit) {
    for (const auto& [bit, label] : kEffectBits)
        if (bit == effectBit) return label;
    return "Effect(" + std::to_string(effectBit) + ")";
}

// --------------------------------------------------
// Minimal JSON parser for FULL_STATE payloads
// --------------------------------------------------

struct LaneSlot {
    bool present{false};
    int  cardId{-1};
    int  augP{0};
    int  augT{0};
    int  fx{0};
};

struct PlayerSnap {
    int id{-1};
    int health{100};
    int mana{0};
    int fatigue{1};
    std::vector<int> hand;
};

struct FullSnap {
    PlayerSnap       players[2];
    LaneSlot         lanes[2][5];
    std::vector<int> discard[2];
    bool             valid{false};
};

// --------------------------------------------------
// Helpers NOT covered by JsonUtil
// --------------------------------------------------

// Skip leading whitespace; returns updated pos.
static std::size_t skipWs(const std::string& s, std::size_t pos) {
    while (pos < s.size() && std::isspace(static_cast<unsigned char>(s[pos])))
        ++pos;
    return pos;
}

// Parse a JSON integer array [ n, n, ... ] starting at pos; advances pos past ']'.
static std::vector<int> parseIntArray(const std::string& s, std::size_t& pos) {
    std::vector<int> result;
    pos = skipWs(s, pos);
    if (pos >= s.size() || s[pos] != '[') return result;
    ++pos;
    while (pos < s.size()) {
        pos = skipWs(s, pos);
        if (pos >= s.size()) break;
        if (s[pos] == ']') { ++pos; break; }
        if (s[pos] == ',') { ++pos; continue; }
        int val = 0;
        if (JsonUtil::parseJsonIntAt(s, pos, val))   // ← was local parseJsonInt
            result.push_back(val);
        else
            break;
    }
    return result;
}

// Returns the position of the first non-whitespace char after ':' for key,
// searching forward from searchFrom.  Returns npos if not found.
static std::size_t findKey(const std::string& s,
                            std::size_t        searchFrom,
                            const std::string& key) {
    const std::string quoted = "\"" + key + "\"";
    std::size_t found = s.find(quoted, searchFrom);
    if (found == std::string::npos) return std::string::npos;
    found += quoted.size();
    found = skipWs(s, found);
    if (found < s.size() && s[found] == ':') ++found;
    return skipWs(s, found);
}

// --------------------------------------------------
// Parsers
// --------------------------------------------------

// Parse a lane slot value (null or {...}) at pos; advances pos past the token.
static LaneSlot parseLaneSlot(const std::string& s, std::size_t& pos) {
    LaneSlot slot;
    pos = skipWs(s, pos);
    if (pos >= s.size()) return slot;

    if (s.size() - pos >= 4 && s.compare(pos, 4, "null") == 0) {
        pos += 4;
        return slot;
    }
    if (s[pos] != '{') return slot;

    const std::size_t objStart = pos;
    std::size_t       closePos = std::string::npos;
    if (!JsonUtil::findMatchingBrace(s, objStart, closePos)) return slot;  // ← was findMatchingClose
    const std::size_t objEnd = closePos + 1;  // exclusive upper bound

    // Operate on the bounded substring so readJsonIntField can't wander outside.
    const std::string obj = s.substr(objStart, objEnd - objStart);
    slot.present = true;
    JsonUtil::readJsonIntField(obj, "id",   slot.cardId);   // ← were readIntField calls
    JsonUtil::readJsonIntField(obj, "augP", slot.augP);
    JsonUtil::readJsonIntField(obj, "augT", slot.augT);
    JsonUtil::readJsonIntField(obj, "fx",   slot.fx);

    pos = objEnd;
    return slot;
}

// Parse a player sub-object; objStart points at the opening '{'.
static PlayerSnap parsePlayerSnap(const std::string& s, std::size_t objStart) {
    PlayerSnap p;
    std::size_t closePos = std::string::npos;
    if (!JsonUtil::findMatchingBrace(s, objStart, closePos)) return p;  // ← was findMatchingClose
    const std::size_t objEnd = closePos + 1;

    const std::string obj = s.substr(objStart, objEnd - objStart);
    JsonUtil::readJsonIntField(obj, "id",      p.id);       // ← were readIntField calls
    JsonUtil::readJsonIntField(obj, "health",  p.health);
    JsonUtil::readJsonIntField(obj, "mana",    p.mana);
    JsonUtil::readJsonIntField(obj, "fatigue", p.fatigue);

    std::size_t hpos = findKey(obj, 0, "hand");
    if (hpos != std::string::npos)
        p.hand = parseIntArray(obj, hpos);

    return p;
}

static FullSnap parseFullSnap(const std::string& json) {
    FullSnap snap;

    // Players
    for (int p = 0; p < 2; ++p) {
        std::size_t vpos = findKey(json, 0, "p" + std::to_string(p));
        if (vpos == std::string::npos) return snap;
        vpos = skipWs(json, vpos);
        if (vpos >= json.size() || json[vpos] != '{') return snap;
        snap.players[p] = parsePlayerSnap(json, vpos);
    }

    // Lanes: [[slot,...],[slot,...]]
    std::size_t lanesPos = findKey(json, 0, "lanes");
    if (lanesPos == std::string::npos) return snap;
    lanesPos = skipWs(json, lanesPos);
    if (lanesPos >= json.size() || json[lanesPos] != '[') return snap;
    ++lanesPos; // skip outer '['

    for (int p = 0; p < 2; ++p) {
        lanesPos = skipWs(json, lanesPos);
        if (lanesPos >= json.size()) break;
        if (json[lanesPos] == ',') ++lanesPos;
        lanesPos = skipWs(json, lanesPos);
        if (lanesPos >= json.size() || json[lanesPos] != '[') break;
        ++lanesPos; // skip inner '['

        for (int lane = 0; lane < 5; ++lane) {
            lanesPos = skipWs(json, lanesPos);
            if (lanesPos >= json.size()) break;
            if (json[lanesPos] == ',') { ++lanesPos; lanesPos = skipWs(json, lanesPos); }
            snap.lanes[p][lane] = parseLaneSlot(json, lanesPos);
        }
        while (lanesPos < json.size() && json[lanesPos] != ']') ++lanesPos;
        if (lanesPos < json.size()) ++lanesPos; // skip inner ']'
    }

    // Discard: [[id,...],[id,...]]
    std::size_t discardPos = findKey(json, 0, "discard");
    if (discardPos != std::string::npos) {
        discardPos = skipWs(json, discardPos);
        if (discardPos < json.size() && json[discardPos] == '[') {
            ++discardPos;
            for (int p = 0; p < 2; ++p) {
                discardPos = skipWs(json, discardPos);
                if (discardPos < json.size() && json[discardPos] == ',') ++discardPos;
                snap.discard[p] = parseIntArray(json, discardPos);
            }
        }
    }

    snap.valid = true;
    return snap;
}

}


// -------------------------
// Helpers (cont.)
// -------------------------

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
        for (std::size_t slot = 0; slot < playSlots.size(); ++slot) {
            if (RenderUtil::pointInRect(playSlots[slot], x, y)) {
                clickedTarget = 100 + static_cast<int>(slot);
                break;
            }
        }
    }

    if (clickedTarget == -9999) {
        for (std::size_t slot = 0; slot < opponentSlots.size(); ++slot) {
            if (RenderUtil::pointInRect(opponentSlots[slot], x, y)) {
                clickedTarget = 200 + static_cast<int>(slot);
                break;
            }
        }
    }

    if (clickedTarget == -9999 && RenderUtil::pointInRect(localPlayerRect, x, y))    clickedTarget = -1;
    if (clickedTarget == -9999 && RenderUtil::pointInRect(opponentPlayerRect, x, y)) clickedTarget = -2;

    const bool isValidTarget = std::find(validTargets.begin(), validTargets.end(), clickedTarget) != validTargets.end();
    if (!isValidTarget) {
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
        std::cout << "[Playing] Casting at targetIndex: " + std::to_string(targetIndex) + "\n";
        std::cout << "[Playing] Casting at targetLane: " + std::to_string(targetLane) + "\n";

        authority->playCard(
            static_cast<int>(pendingAction.cardId),
            pendingAction.sourceLane,
            targetLane,
            targetIndex
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
    deferredStatUpdates.clear();
    lastEndState = PlayingGameState::Playing;
    gameEndStartTick = 0;
    cachedFont = game.getUIFonts().medium;

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

SDL_Rect Playing::computeOpponentDeckRect(int screenW, int screenH) const {
    if (screenW <= 0 || screenH <= 0 || opponentSlots.empty()) {
        return SDL_Rect{0, 0, 0, 0};
    }

    return PlayingLayoutUtil::computeDeckRect(
        opponentSlots,
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

        if (state != PlayingGameState::Playing) {
            if (RenderUtil::pointInRect(returnToTitleButton, mouseX, mouseY)) {
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
                pauseModalOpen = false;
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
                    if (drag.index >= localPlayer.hand.size()) {
                        drag.active = false;
                        return;
                    }
                    auto& card = localPlayer.hand[drag.index];
                    std::cout << "[Playing] Attempting to Discard " << card->getName() << "\n";
                    if (animationsEnabled) {
                        SDL_Rect animRect = cardRects[drag.index];
                        animRect.x = drag.x;
                        animRect.y = drag.y;
                        DiscardAnimation::stagePending(animRect, card->getId(), 500U, card->clone());
                    }
                    authority->discardCard(card->getId());
                }
                else if (laneIndex >= 0) {
                    if (drag.index >= localPlayer.hand.size()) {
                        drag.active = false;
                        return;
                    }
                    auto& card = localPlayer.hand[drag.index];
                    if (card->getType() == CardType::Creature) {
                        if (localPlayer.mana >= card->getManaCost()) {
                            authority->playCard(
                                card->getId(),
                                laneIndex,
                                std::nullopt,
                                std::nullopt
                            );
                        }
                    } else if (card->getType() == CardType::Spell) {
                        if (localPlayer.mana >= card->getManaCost()) {

                            auto validTargetsOpt = getValidTargets(*this, *card, pendingAction.sourceLane);
                            if (!validTargetsOpt.has_value()) {
                                authority->playCard(
                                    card->getId(),
                                    laneIndex,
                                    -1,
                                    -1
                                );
                            } else {
                                const std::vector<int>& validTargets = *validTargetsOpt;
                                if (!validTargets.empty()) {
                                    int allTarget = 0;
                                    switch (validTargets[0]) {
                                        case 901: allTarget = -1; break;
                                        case 902: allTarget = -2; break;
                                        case 903: allTarget = -3; break;
                                        default:  break;
                                    }
                                    if (allTarget != 0) {
                                        authority->playCard(
                                            card->getId(),
                                            laneIndex,
                                            -1,
                                            allTarget
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

// -------------------------
// Server message dispatch
// -------------------------
bool Playing::handleServerMessage(const std::string& msg) {
    std::istringstream iss(msg);
    std::string cmd;
    iss >> cmd;

    const auto syncCombatTimeline = [this]() {
        const Uint32 now = SDL_GetTicks();
        if (lastCombatSyncTick != 0 && now - lastCombatSyncTick < (COMBAT_CYCLE_DURATION_MS / 2U)) {
            return;
        }
        combatCycleStartTick =
            (now > COMBAT_PREPHASE_DURATION_MS) ? (now - COMBAT_PREPHASE_DURATION_MS) : 0;
        lastCombatSyncTick = now;
    };

    // FULL_STATE arrives only between combat phases (server guarantees this by
    // resetting the snapshot timer whenever resolveAttackPhase() runs). Apply
    // it directly — no need to touch the deferred queue.
    if (cmd == "FULL_STATE") {
        std::string jsonStr;
        std::getline(iss, jsonStr);
        if (!jsonStr.empty() && jsonStr.front() == ' ') jsonStr.erase(0, 1);
        handleFullStateMessage(jsonStr);
        return true;
    }

    // Stat updates are held until any active combat animation settles.
    static const std::vector<std::string> statUpdateCmds = {
        "AUGMENT", "DESTROY", "HP", "EFFECT_ADD", "EFFECT_REMOVE", "REGEN_SET",
        "MATCH_WON", "MATCH_LOST"
    };
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
            playCreature(playerId, std::move(*it), lane);
            player.hand.erase(it);
        } else if (type == CardType::Spell) {
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
            playSpell(playerId, std::move(*it), lane, targetLane, targetIndex);
            player.hand.erase(it);
        }
    } else if (cmd == "COMBAT") {
        int playerAId, playerBId, lane, powerA, powerB;
        iss >> playerAId >> playerBId >> lane >> powerA >> powerB;
        combatUpdateBarrierActive = animationsEnabled;
        syncCombatTimeline();
        resolveLaneCombat(playerAId, playerBId, lane, powerA, powerB);

    } else if (cmd == "DIRECT") {
        int playerId, lane, damage;
        iss >> playerId >> lane >> damage;
        combatUpdateBarrierActive = animationsEnabled;
        syncCombatTimeline();
        resolveDirectCombat(playerId, lane, damage);

    } else if (cmd == "AUGMENT") {
        int playerId, lane, powerDelta, toughnessDelta;
        iss >> playerId >> lane >> powerDelta >> toughnessDelta;
        augmentCreature(playerId, lane, powerDelta, toughnessDelta);

    } else if (cmd == "DESTROY") {
        int playerId, lane;
        iss >> playerId >> lane;
        destroyCreature(playerId, lane);

    } else if (cmd == "HP") {
        int playerId, delta;
        iss >> playerId >> delta;
        augmentHP(playerId, delta);

    } else if (cmd == "MANA") {
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
            zone.value()->removeGrantedEffectsWithPrefix("Regen ");
            if (regenValue > 0)
                zone.value()->addGrantedEffect("Regen " + std::to_string(regenValue));
        }
    } else if (cmd == "FATIGUE") {
        int playerId, fatigueDamage;
        iss >> playerId >> fatigueDamage;
        if (animationsEnabled) {
            int screenW = 0, screenH = 0;
            SDL_GetRendererOutputSize(renderer, &screenW, &screenH);
            const SDL_Rect deckRect = (playerId == localPlayer.id)
                ? computeSelfDeckRect(screenW, screenH)
                : computeOpponentDeckRect(screenW, screenH);
            animationQueue.enqueue(
                std::make_shared<FatigueAnimation>(deckRect, fatigueDamage, 400U, cachedFont)
            );
        }
    } else if (cmd == "MATCH_LOST") {
        Mix_HaltChannel(-1);
        Audio::playSFX("gameEnd");
        state = PlayingGameState::Lost;
        std::cout << "[Playing] Lost Match!\n";
    } else if (cmd == "MATCH_WON") {
        Mix_HaltChannel(-1);
        Audio::playSFX("gameEnd");
        iss >> coinReward;
        state = PlayingGameState::Won;
        std::cout << "[Playing] Won Match!\n";
    }

    return true;
}

// -------------------------
// Full state snapshot
// -------------------------

void Playing::syncEffectsFromMask(Card* card, int fx) {
    if (!card) return;
    card->clearGrantedEffects();
    for (const auto& [bit, label] : kEffectBits)
        if (fx & bit) card->addGrantedEffect(label);
}

void Playing::handleFullStateMessage(const std::string& json) {
    const FullSnap snap = parseFullSnap(json);
    if (!snap.valid) {
        std::cerr << "[FULL_STATE] Failed to parse snapshot – ignoring\n";
        return;
    }

    // Map server indices (p0/p1) to local/remote by player ID.
    int localSrvIdx  = -1;
    int remoteSrvIdx = -1;
    for (int i = 0; i < 2; ++i) {
        if      (snap.players[i].id == localPlayer.id)  localSrvIdx  = i;
        else if (snap.players[i].id == remotePlayer.id) remoteSrvIdx = i;
    }
    if (localSrvIdx == -1 || remoteSrvIdx == -1) {
        std::cerr << "[FULL_STATE] Could not map player IDs (local="
                  << localPlayer.id << " remote=" << remotePlayer.id << ")\n";
        return;
    }

    // skip all reconciliation work — nothing has drifted.
    auto alreadySynced = [&]() -> bool {
        // --- Player stats ---
        const auto checkPlayer = [](const Player& player, const PlayerSnap& ps) {
            return player.health        == ps.health
                && player.mana          == ps.mana
                && player.fatigueDamage == ps.fatigue;
        };
        if (!checkPlayer(localPlayer,  snap.players[localSrvIdx]))  return false;
        if (!checkPlayer(remotePlayer, snap.players[remoteSrvIdx])) return false;

        // --- Hands (order-independent set comparison) ---
        const auto handMatchesSnap = [](const std::vector<std::unique_ptr<Card>>& hand,
                                        const std::vector<int>& snapHand) {
            if (hand.size() != snapHand.size()) return false;

            // Build count maps for both sides and compare
            std::unordered_map<int, int> handCounts, snapCounts;
            for (const auto& c : hand)
                if (c) handCounts[c->getId()]++;
            for (int id : snapHand)
                snapCounts[id]++;

            return handCounts == snapCounts;
        };
        if (!handMatchesSnap(localPlayer.hand,  snap.players[localSrvIdx].hand))  return false;
        if (!handMatchesSnap(remotePlayer.hand, snap.players[remoteSrvIdx].hand)) return false;

        // --- Board lanes ---
        const int laneCount = board.getLaneCount();
        for (int srvIdx = 0; srvIdx < 2; ++srvIdx) {
            const int boardIdx = (srvIdx == localSrvIdx) ? 0 : 1;
            for (int lane = 0; lane < laneCount; ++lane) {
                const LaneSlot& slot      = snap.lanes[srvIdx][lane];
                const bool boardOccupied  = !board.isZoneEmpty(lane, boardIdx);

                if (slot.present != boardOccupied) return false;
                if (!slot.present) continue; // both empty — nothing to check

                const auto& zoneOpt = board.getZone(lane, boardIdx);
                if (!zoneOpt.has_value() || !zoneOpt.value()) return false;
                const Card* card = zoneOpt.value().get();

                if (card->getId() != slot.cardId) return false;

                if (card->getType() == CardType::Creature) {
                    const auto* creature = static_cast<const CreatureCard*>(card);
                    const int currentAugP = creature->getPower()     - creature->getBasePower();
                    const int currentAugT = creature->getToughness() - creature->getBaseToughness();
                    if (currentAugP != slot.augP || currentAugT != slot.augT) return false;
                }

                // Check granted effects match the snapshot's fx mask
                {
                    int currentGrantedMask = 0;
                    for (const auto& [bit, label] : kEffectBits)   // ← no local redeclaration
                        if (card->hasGrantedEffect(label)) currentGrantedMask |= bit;
                    if (currentGrantedMask != slot.fx) return false;
                }
            }
        }

        return true; // everything matches
    };

    if (alreadySynced()) {
        std::cout << "[FULL_STATE] Already in sync: skipping reconciliation\n";
        return;
    }

    // 1. Player stats
    auto reconcileStats = [](Player& player, const PlayerSnap& ps) {
        if (player.health        != ps.health)  player.health        = ps.health;
        if (player.mana          != ps.mana)    player.mana          = ps.mana;
        if (player.fatigueDamage != ps.fatigue) player.fatigueDamage = ps.fatigue;
    };

    reconcileStats(localPlayer,  snap.players[localSrvIdx]);
    reconcileStats(remotePlayer, snap.players[remoteSrvIdx]);

    // 2. Hands — remove cards absent from snapshot, pull missing ones from deck.
    auto reconcileHand = [](Player& player, const std::vector<int>& snapHand) {
        // Pull all current hand cards into a temporary pool
        std::vector<std::unique_ptr<Card>> pool;
        pool.swap(player.hand);

        // Rebuild hand exactly as the snapshot dictates
        for (int cardId : snapHand) {
            // Prefer a card already in the pool (avoids unnecessary deck lookups)
            auto it = std::find_if(pool.begin(), pool.end(),
                [cardId](const std::unique_ptr<Card>& c) {
                    return c && c->getId() == cardId;
                });

            if (it != pool.end()) {
                player.hand.push_back(std::move(*it));
                pool.erase(it);
            } else {
                auto card = player.getDeck().takeCardById(cardId);
                if (card)
                    player.addCardToHand(std::move(card));
                else
                    std::cerr << "[FULL_STATE] Card " << cardId
                            << " not found in deck for hand sync\n";
            }
        }
        // Cards left in pool were removed server-side — silently drop them
    };
    reconcileHand(localPlayer,  snap.players[localSrvIdx].hand);
    reconcileHand(remotePlayer, snap.players[remoteSrvIdx].hand);

    // 3. Board lanes
    // Pull a card by ID — checks hand first, then deck.
    auto acquireCard = [](Player& player, int cardId) -> std::unique_ptr<Card> {
        auto it = std::find_if(player.hand.begin(), player.hand.end(),
            [cardId](const std::unique_ptr<Card>& c) { return c->getId() == cardId; });
        if (it != player.hand.end()) {
            auto card = std::move(*it);
            player.hand.erase(it);
            return card;
        }
        return player.getDeck().takeCardById(cardId);
    };

    // Apply a snapshot's augP/augT to a card that is at its base stats.
    auto applyAugments = [](Card* card, const LaneSlot& slot) {
        if (card->getType() != CardType::Creature) return;
        if (slot.augP == 0 && slot.augT == 0) return;
        static_cast<CreatureCard*>(card)->augmentStats(slot.augP, slot.augT);
    };

    const int laneCount = board.getLaneCount();

    for (int srvIdx = 0; srvIdx < 2; ++srvIdx) {
        const int boardIdx = (srvIdx == localSrvIdx) ? 0 : 1;
        Player& player     = (boardIdx == 0) ? localPlayer : remotePlayer;

        for (int lane = 0; lane < laneCount; ++lane) {
            const LaneSlot& slot     = snap.lanes[srvIdx][lane];
            const bool boardOccupied = !board.isZoneEmpty(lane, boardIdx);

            if (!slot.present && boardOccupied) {
                // Server: empty — remove stale client creature.
                std::unique_ptr<Card> removed;
                board.removeFromPlay(lane, boardIdx, removed);

            } else if (slot.present && !boardOccupied) {
                // Server: has creature — client is missing it.
                auto card = acquireCard(player, slot.cardId);
                if (card) {
                    applyAugments(card.get(), slot);
                    syncEffectsFromMask(card.get(), slot.fx);
                    board.addToPlay(lane, boardIdx, std::move(card));
                } else {
                    std::cerr << "[FULL_STATE] Cannot find card " << slot.cardId
                              << " for lane " << lane << " player " << player.id << "\n";
                }

            } else if (slot.present && boardOccupied) {
                auto& zoneOpt = board.getZoneMutable(lane, boardIdx);
                if (!zoneOpt.has_value() || !zoneOpt.value()) continue;
                Card* card = zoneOpt.value().get();

                if (card->getId() != slot.cardId) {
                    // Wrong card on the board — replace it.
                    std::unique_ptr<Card> removed;
                    board.removeFromPlay(lane, boardIdx, removed);

                    auto newCard = acquireCard(player, slot.cardId);
                    if (newCard) {
                        applyAugments(newCard.get(), slot);
                        syncEffectsFromMask(newCard.get(), slot.fx);
                        board.addToPlay(lane, boardIdx, std::move(newCard));
                    } else {
                        std::cerr << "[FULL_STATE] Cannot find replacement card "
                                  << slot.cardId << " for lane " << lane << "\n";
                    }

                } else {
                    // Correct card — sync augments and effects.
                    if (card->getType() == CardType::Creature) {
                        CreatureCard* creature = static_cast<CreatureCard*>(card);
                        const int currentAugP = creature->getPower()     - creature->getBasePower();
                        const int currentAugT = creature->getToughness() - creature->getBaseToughness();
                        const int deltaP = slot.augP - currentAugP;
                        const int deltaT = slot.augT - currentAugT;
                        if (deltaP != 0 || deltaT != 0)
                            creature->augmentStats(deltaP, deltaT);
                    }
                    syncEffectsFromMask(card, slot.fx);
                }
            }
            // Both empty → nothing to do.
        }
    }
    drag.active = false;
    drag.index = 0;
    hoverIndex = static_cast<std::size_t>(-1);
    cardRects.clear();
    std::cout << "[FULL_STATE] Reconciliation complete\n";
}

// -------------------------
// Game actions
// -------------------------

void Playing::playCreature(int playerId, std::unique_ptr<Card> card, int lane) {
    Player& player = (playerId == localPlayer.id) ? localPlayer : remotePlayer;

    if (lane < 0 || lane >= board.getLaneCount()) return;
    if (!card) return;
    if (card->getType() != CardType::Creature) return;

    std::string name = card->getName();
    player.mana -= card->getManaCost();

    int boardIndex = (playerId == localPlayer.id) ? 0 : 1;
    Audio::playSFX("summon");
    board.addToPlay(lane, boardIndex, std::move(card));

    if (animationsEnabled) {
        const std::vector<SDL_Rect>& slots = (boardIndex == 0) ? playSlots : opponentSlots;
        if (lane >= 0 && static_cast<std::size_t>(lane) < slots.size()) {
            animationQueue.enqueue(
                std::make_shared<SummonAnimation>(slots[static_cast<std::size_t>(lane)], 600U)
            );
        }
    }

    std::cout << "[Playing] Summoned " << name
              << " for " << playerId
              << " at lane " << lane << "\n";
}

void Playing::playSpell(int playerId, std::unique_ptr<Card> card, int sourceLane, std::optional<int> targetLane, std::optional<int> targetIndex) {
    Player& player = (playerId == localPlayer.id) ? localPlayer : remotePlayer;
    if (sourceLane < 0 || sourceLane >= board.getLaneCount()) return;
    if (!card) return;
    if (card->getType() != CardType::Spell) return;

    recentSpellPreview = card->clone();
    recentSpellPreviewUntil = SDL_GetTicks() + Theme::Playing::SPELL_CAST_PREVIEW_DURATION_MS;
    recentSpellPreviewStartTick = SDL_GetTicks();

    std::string name = card->getName();
    player.mana -= card->getManaCost();
    Audio::playSFX("activate");

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
    Player& player = (playerId == localPlayer.id) ? localPlayer : remotePlayer;

    auto card = player.getDeck().takeCardById(cardId);
    if (!card) {
        std::cerr << "Missing card id " << cardId << "\n";
        return false;
    }

    Audio::playSFX("draw");
    player.addCardToHand(std::move(card));

    int screenW = 0, screenH = 0;
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

    const std::vector<SDL_Rect> oldRects = cardRects;
    const std::size_t discardedIdx = static_cast<std::size_t>(
        std::distance(player->hand.begin(), it));

    if (animationsEnabled) {
        auto anim = DiscardAnimation::takePending(cardId);
        if (playerId == localPlayer.id) {
            if (!anim && discardedIdx < oldRects.size()) {
                anim = std::make_shared<DiscardAnimation>(oldRects[discardedIdx], cardId, 500U);
            }
        } else {
            int screenW, screenH;
            if (SDL_GetRendererOutputSize(renderer, &screenW, &screenH) != 0) return;
            SDL_Rect opponentDiscardRect = PlayingRenderUtil::computeOpponentDiscardRect(opponentSlots, screenW, screenH);
            anim = std::make_shared<DiscardAnimation>(opponentDiscardRect, cardId, 500U);
        }
        if (anim) animationQueue.enqueue(anim);
    }

    std::unique_ptr<Card> cardToDiscard = std::move(*it);
    std::string name = cardToDiscard->getName();
    player->hand.erase(it);
    // player->addMana(cardToDiscard->getManaValue());

    int boardIndex = (playerId == localPlayer.id) ? 0 : 1;
    Audio::playSFX("discard");
    board.addToDiscard(std::move(cardToDiscard), boardIndex);

    std::cout << "[Playing] " << playerId << " Discarded " << name << "\n";
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
    Audio::playSFX("augment");
    creature->augmentStats(powerDelta, toughnessDelta);
}

void Playing::destroyCreature(int playerId, int lane) {
    int boardIndex = (playerId == localPlayer.id) ? 0 : 1;
    if (lane < 0 || lane >= board.getLaneCount()) return;

    auto removeAndAnimate = [&](int targetBoardIndex, int targetLane) {
        std::unique_ptr<Card> card;
        Audio::playSFX("destroyed");
        if (!board.removeFromPlay(targetLane, targetBoardIndex, card)) {
            std::cout << "[destroyCreature] No card on lane " << targetLane
                      << " for player " << playerId << "\n";
            return;
        }
        if (animationsEnabled) {
            animationQueue.enqueue(std::make_shared<DeathAnimation>(
                targetLane, targetBoardIndex == 0, playSlots, opponentSlots, 260U
            ));
        }
    };

    const auto& targetZone = board.getZone(lane, boardIndex);
    if (!targetZone.has_value() || !targetZone.value()) return;

    const int opposingBoardIndex = (boardIndex == 0) ? 1 : 0;
    const auto& opposingZone = board.getZone(lane, opposingBoardIndex);
    const bool contestedLane = opposingZone.has_value() && static_cast<bool>(opposingZone.value());

    if (!contestedLane) {
        removeAndAnimate(boardIndex, lane);
        return;
    }

    const bool alreadyQueued = std::any_of(
        pendingDestroys.begin(), pendingDestroys.end(),
        [&](const PendingDestroyState& pending) {
            return pending.boardIndex == boardIndex && pending.lane == lane;
        }
    );
    if (alreadyQueued) return;

    if (!animationsEnabled) {
        removeAndAnimate(boardIndex, lane);
        return;
    }

    static constexpr Uint32 destroyDelayMs = 520U;
    pendingDestroys.push_back(PendingDestroyState{boardIndex, lane, SDL_GetTicks() + destroyDelayMs});
}

void Playing::processPendingDestroys(Uint32 now) {
    if (pendingDestroys.empty()) return;

    std::vector<PendingDestroyState> stillPending;
    stillPending.reserve(pendingDestroys.size());

    for (const PendingDestroyState& pending : pendingDestroys) {
        if (now < pending.executeAt) {
            stillPending.push_back(pending);
            continue;
        }
        if (animationsEnabled) {
            animationQueue.enqueue(std::make_shared<DeathAnimation>(
                pending.lane, pending.boardIndex == 0, playSlots, opponentSlots, 260U
            ));
        }
        std::unique_ptr<Card> card;
        Audio::playSFX("destroyed");
        if (!board.removeFromPlay(pending.lane, pending.boardIndex, card)) continue;
    }

    pendingDestroys.swap(stillPending);
}

void Playing::resolveLaneCombat(int playerAId, int playerBId, int lane, int powerA, int powerB) {
    (void)powerA;
    (void)powerB;

    if (lane < 0) return;
    Audio::playSFX("attack");
    if (!animationsEnabled) return;

    const int boardIndexA = (playerAId == localPlayer.id) ? 0 : 1;
    const int boardIndexB = (playerBId == localPlayer.id) ? 0 : 1;

    const std::vector<SDL_Rect>& slotsA = (boardIndexA == 0) ? playSlots : opponentSlots;
    const std::vector<SDL_Rect>& slotsB = (boardIndexB == 0) ? playSlots : opponentSlots;

    const std::size_t laneIndex = static_cast<std::size_t>(lane);
    if (laneIndex >= slotsA.size() || laneIndex >= slotsB.size()) return;

    const auto& creatureA = board.getZone(lane, boardIndexA);
    const auto& creatureB = board.getZone(lane, boardIndexB);
    if (!creatureA.has_value() || !creatureA.value() || !creatureB.has_value() || !creatureB.value()) return;

    auto attackGroup = std::make_shared<AnimationGroup>();
    attackGroup->add(std::make_shared<AttackAnimation>(lane, boardIndexA == 0, playSlots, opponentSlots, 420U));
    attackGroup->add(std::make_shared<AttackAnimation>(lane, boardIndexB == 0, playSlots, opponentSlots, 420U));
    animationQueue.enqueue(attackGroup);
}

void Playing::resolveDirectCombat(int playerId, int lane, int damage) {
    (void)damage;

    if (!renderer || lane < 0) return;

    const int boardIndex = (playerId == localPlayer.id) ? 0 : 1;
    const std::vector<SDL_Rect>& sourceSlots = (boardIndex == 0) ? playSlots : opponentSlots;

    const std::size_t laneIndex = static_cast<std::size_t>(lane);
    if (laneIndex >= sourceSlots.size()) return;

    int screenW = 0, screenH = 0;
    if (SDL_GetRendererOutputSize(renderer, &screenW, &screenH) != 0) return;

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

    Audio::playSFX("attack");
    if (animationsEnabled) {
        animationQueue.enqueue(std::make_shared<AttackAnimation>(
            lane, boardIndex == 0, playSlots, opponentSlots, 420U, &targetRect
        ));
    }
}

void Playing::augmentHP(int playerId, int delta) {
    Player& player = (playerId == localPlayer.id) ? localPlayer : remotePlayer;
    Audio::playSFX("damage");
    player.health += delta;
}

void Playing::augmentMana(int playerId, int delta) {
    Player& player = (playerId == localPlayer.id) ? localPlayer : remotePlayer;
    Audio::playSFX("discard");
    player.mana += delta;
}

// -------------------------
// Render
// -------------------------
void Playing::render(Game& game) {
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

    const int verticalOffset  = (screenH - static_cast<int>(850.0F * scale)) / 2;
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

    int handY = screenH - handCardHeight - handYOffset - verticalOffset;
    if (!cardRects.empty()) handY = cardRects.front().y;

    int totalSlotsWidth = slotCount * slotWidth + (slotCount - 1) * slotSpacing;
    int startX = (screenW - totalSlotsWidth) / 2;
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
        SDL_Rect localRect{ startX + i * (slotWidth + slotSpacing), slotY, slotWidth, slotHeight };
        playSlots.push_back(localRect);
        SDL_Rect oppRect = localRect;
        oppRect.y -= opponentOffset;
        opponentSlots.push_back(oppRect);
    }

    discardZone = PlayingLayoutUtil::computeDiscardRect(
        playSlots, cardWidth,
        static_cast<int>(Theme::Playing::CARD_HEIGHT * scale),
        deckGap, sideMargin
    );
}

void Playing::computeUiRects(int screenW, int screenH) {
    if (screenW <= 0 || screenH <= 0) {
        menuButton = pauseModal = resumeButton = pauseExitButton = SDL_Rect{0,0,0,0};
        exitModal = saveExitButton = noSaveExitButton = SDL_Rect{0,0,0,0};
        returnToTitleButton = requeueButton = SDL_Rect{0,0,0,0};
        return;
    }

    const float scale = std::min(
        static_cast<float>(screenW) / 1200.0F,
        static_cast<float>(screenH) / 850.0F);
    const int verticalOffset = (screenH - static_cast<int>(850.0F * scale)) / 2;
    const int margin = static_cast<int>(Theme::Playing::SCREEN_MARGIN * scale);

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

    saveExitButton   = SDL_Rect{(screenW - exitButtonW * 2 - exitButtonSpacing) / 2, exitFirstButtonY, exitButtonW, exitButtonH};
    noSaveExitButton = SDL_Rect{saveExitButton.x + exitButtonW + exitButtonSpacing, exitFirstButtonY, exitButtonW, exitButtonH};

    const int returnW = static_cast<int>(Theme::Playing::RETURN_BUTTON_WIDTH  * scale);
    const int returnH = static_cast<int>(Theme::Playing::RETURN_BUTTON_HEIGHT * scale);
    const int spacing = static_cast<int>(20 * scale);
    const int totalWidth = returnW * 2 + spacing;
    const int startX = (screenW - totalWidth) / 2;
    const int y = (screenH / 2) + static_cast<int>(24 * scale) + verticalOffset;

    returnToTitleButton = SDL_Rect{startX,                     y, returnW, returnH};
    requeueButton       = SDL_Rect{startX + returnW + spacing, y, returnW, returnH};
}

// -------------------------
// Accessors
// -------------------------
PlayingGameState Playing::getState() const {
    return state;
}

int Playing::getCoinReward() const {
    return coinReward;
}
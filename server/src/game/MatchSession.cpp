#include "game/MatchSession.hpp"
#include "net/PlayerConnection.hpp"
#include "objects/CreatureCard.h"
#include "objects/SpellCard.h"
#include "game/CombatEffects.hpp"
#include "utils/EnvUtil.hpp"
#include "utils/HttpUtil.hpp"
#include "utils/JsonUtil.hpp"

#include <iostream>
#include <algorithm>
#include <chrono>
#include <thread>
#include <mutex>
#include <queue>
#include <sstream>

/* TODO: Full State Snapshot
FULL_STATE {
  player0: {health: 90, mana: 3, hand: [1,2,5]},
  player1: {health: 80, mana: 2, hand: [7,8,9]},
  lanes: [[null,1,2,3,null],[4,5,null,6,7]],
  discard: [[8,9],[10,11]]
}
*/

MatchSession::MatchSession(
    std::shared_ptr<PlayerConnection> a,
    std::shared_ptr<PlayerConnection> b,
    const std::unordered_map<int, std::shared_ptr<ServerCard>>& catalog)
    : playerA(std::move(a)),
      playerB(std::move(b)),
      cardCatalog(catalog)
{
    players[0].id = playerA->getPlayerId();
    players[1].id = playerB->getPlayerId();
}

MatchSession::~MatchSession() {
    std::cout << "[MatchSession] Destroying..." << "\n";
    stop();
}

// --------------------------------------------------
// Start / Stop
// --------------------------------------------------
bool MatchSession::start() {
    if (running.load()) return false;

    auto weakSession = std::weak_ptr<MatchSession>(shared_from_this());
    // Set the Playing callback for Player A
    playerA->setMessageHandler(ConnectionState::Playing,
        [weakSession](const std::shared_ptr<PlayerConnection>& /*p*/, const std::string& msg) {
            if (auto session = weakSession.lock()) {
                // convert string back to vector<char> for handlePlayerMessage
                std::vector<char> raw(msg.begin(), msg.end());
                session->handlePlayerMessage(0, raw);
            }
        });

    // Set the Playing callback for Player B
    playerB->setMessageHandler(ConnectionState::Playing,
        [weakSession](const std::shared_ptr<PlayerConnection>& /*p*/, const std::string& msg) {
            if (auto session = weakSession.lock()) {
                std::vector<char> raw(msg.begin(), msg.end());
                session->handlePlayerMessage(1, raw);
            }
        });

    // Switch the players into Playing state
    playerA->state = ConnectionState::Playing;
    playerB->state = ConnectionState::Playing;
    
    running = true;

    try {
        gameThread = std::thread(&MatchSession::gameLoop, this);
    } catch (const std::exception& e) {
        std::cerr << "Failed to start match thread: " << e.what() << "\n";
        running = false;
        return false;
    }

    return true;
}

void MatchSession::stop() {
    std::cout << "[MatchSession] Executing stop()...\n";

    running = false;

    if (gameThread.joinable()) {
        if (std::this_thread::get_id() == gameThread.get_id()) {
            std::cout << "[MatchSession] Detaching Thread...\n";
            gameThread.detach();
        } else {
            std::cout << "[MatchSession] Joining Thread...\n";
            gameThread.join();
        }
    }
    std::cout << "[MatchSession] Stopped.\n";
}

// --------------------------------------------------
// Setup Phase
// --------------------------------------------------
bool MatchSession::loadDeckForPlayer(int playerId, ServerDeck& outDeck) {
    std::cout << "[DEBUG] Loading deck for Player " << playerId << "\n";

    const std::string cardsHost = EnvUtil::getServiceHost("CARDS_SERVICE", "127.0.0.1", "api.myapp.com");
    const int cardsPort = EnvUtil::getServicePort("CARDS_SERVICE", 8082, 443);

    std::string path = "/cards/decks/" + std::to_string(playerId);
    std::cout << "[DEBUG] GET " << path << " via " << cardsHost << ":" << cardsPort << "\n";

    int statusCode = -1;
    std::string responseBody;
    const bool ok = HttpUtil::sendHttp(cardsHost, cardsPort, "GET", path, "", statusCode, responseBody);

    if (!ok) {
        std::cerr << "[ERROR] Request failed. Error: Could not establish connection\n";
        return false;
    }

    std::cout << "[DEBUG] HTTP Status: " << statusCode << "\n";

    if (statusCode != 200) {
        std::cerr << "[ERROR] Unexpected HTTP status: " << statusCode << "\n";
        std::cerr << "[ERROR] Response body: " << responseBody << "\n";
        return false;
    }

    std::cout << "[DEBUG] Response body:\n" << responseBody << "\n";

    // Uncomment and use this when parsing JSON
    return parseDeckJson(responseBody, outDeck);

    // return true;
}

//map the JSON card IDs to create their Card obj version, then feed into deck
bool MatchSession::parseDeckJson(const std::string& jsonStr, ServerDeck& outDeck) {
    // Expect JSON like: {"uid":1,"cards":{"1":2,"5":1,...}}
    std::size_t pos = jsonStr.find("\"cards\"");
    if (pos == std::string::npos) {
        std::cerr << "[ERROR] 'cards' field not found in JSON\n";
        return false;
    }

    pos = jsonStr.find('{', pos);
    if (pos == std::string::npos) {
        std::cerr << "[ERROR] '{' not found after 'cards'\n";
        return false;
    }
    ++pos; // skip '{'

    while (pos < jsonStr.size()) {
        // Skip whitespace
        while (pos < jsonStr.size() && std::isspace(jsonStr[pos])) ++pos;
        if (pos >= jsonStr.size() || jsonStr[pos] == '}') break;

        if (jsonStr[pos] != '"') {
            std::cerr << "[WARN] Expected '\"' at position " << pos << "\n";
            return false;
        }
        ++pos;

        // Read card ID key
        std::size_t keyEnd = jsonStr.find('"', pos);
        if (keyEnd == std::string::npos) break;

        int cardId = std::stoi(jsonStr.substr(pos, keyEnd - pos));
        pos = keyEnd + 1;

        // Skip colon and whitespace
        while (pos < jsonStr.size() && (jsonStr[pos] == ':' || std::isspace(jsonStr[pos]))) ++pos;

        // Parse count
        int count = 0;
        if (!JsonUtil::parseJsonIntAt(jsonStr, pos, count)) {
            std::cerr << "[WARN] Failed to parse count for card " << cardId << "\n";
            continue;
        }

        // Skip comma / whitespace
        while (pos < jsonStr.size() && (jsonStr[pos] == ',' || std::isspace(jsonStr[pos]))) ++pos;

        // Find the card in the catalog
        if (cardCatalog.find(cardId) == cardCatalog.end()) {
            std::cerr << "[WARN] Card ID " << cardId << " not found in catalog\n";
            continue;
        }
        
        // Add 'count' clones of the card into the deck
        for (int i = 0; i < count; ++i) {
            outDeck.addCard(cardId); // each copy is independent
        }
    }

    return true;
}

//Sets up decks for both players at start of game (shuffle + draw 6)
void MatchSession::setupDecks() {
    std::cout << "Attempting deck setup for " << playerA.get()->getUsername() << "\n";
    loadDeckForPlayer(playerA.get()->getPlayerId(), players[0].deck);

    std::cout << "Attempting deck setup for " << playerB.get()->getUsername() << "\n";
    loadDeckForPlayer(playerB.get()->getPlayerId(), players[1].deck);

    std::cout << "Shuffling decks...\n";

    players[0].deck.shuffle();
    players[1].deck.shuffle();
}

void MatchSession::sendOpeningHands() {
    for (int i = 0; i < 6; ++i) {
        naturalDraw(0);
        naturalDraw(1);
    }
}


//Draw function: playerIndex player draws 1 card, other player will also receive instruction that their opponent drew 1 card

bool MatchSession::draw(int playerIndex) {
    auto& player = players[playerIndex];
    auto& deck = player.deck;

    auto cardIdOpt = deck.draw();
    if (!cardIdOpt) {
        augmentHP(playerIndex, -player.fatigueDamage);
        player.fatigueDamage+=1;
        return false;
    }

    int cardId = *cardIdOpt;

    //add card ID to player's hand
    player.hand.push_back(cardId);

    // auto& conn = (playerIndex == 0) ? playerA : playerB;
    //send msg to both players, informing that playerIndex player has drawn 1 card
    std::cout << player.id << ": DRAW " << player.id << " " << std::to_string(cardId) << "\n";
    playerA->send("DRAW " + std::to_string(player.id) + " " + std::to_string(cardId) + "\n");
    playerB->send("DRAW " + std::to_string(player.id) + " " + std::to_string(cardId) + "\n");

    return true;
}

bool MatchSession::naturalDraw(int playerIndex) {
    auto& player = players[playerIndex];
    int handSize = player.hand.size();
    if (handSize >= handLimit) {
        return false;
    }
    return draw(playerIndex);
}

// --------------------------------------------------
// Game Loop
// --------------------------------------------------
void MatchSession::gameLoop() {
    using namespace std::chrono;
    auto self = shared_from_this();
    auto lastDrawTime = steady_clock::now();
    auto lastAttackTime = steady_clock::now();

    std::cout << "[MatchSession] Game loop started\n";

    std::string msgA = "MATCH_START " + std::to_string(playerB->getPlayerId()) + "\n";
    std::string msgB = "MATCH_START " + std::to_string(playerA->getPlayerId()) + "\n";

    playerA->send(msgA);
    playerB->send(msgB);

    setupDecks();
    sendOpeningHands();

    while (running.load()) {

        if (!playerA->isAlive()) {
            handleDisconnect(playerA);
            break;
        }

        if (!playerB->isAlive()) {
            handleDisconnect(playerB);
            break;
        }

        processActions();

        auto now = steady_clock::now();

        if (duration_cast<seconds>(now - lastDrawTime).count() >= drawInterval) {
            naturalDraw(0);
            naturalDraw(1);
            lastDrawTime = now;
        }

        if (duration_cast<seconds>(now - lastAttackTime).count() >= attackInterval) {
            resolveAttackPhase();
            lastAttackTime = now;
        }

        std::this_thread::sleep_for(10ms);
    }

    if (onMatchEnd) onMatchEnd(self);
    std::cout << "[MatchSession] Exiting Match Game Loop...\n";
}

//This function handles player actions
/*  Summon Creature
    Cast Spell
    Discard Card
*/
void MatchSession::handlePlayerMessage(int playerIndex, const std::vector<char>& raw) {
    std::string msg(raw.begin(), raw.end());

    std::istringstream ss(msg);
    std::string cmd;
    ss >> cmd;

    PlayerAction action;
    action.playerIndex = playerIndex;
    action.type = cmd;

    int arg;
    while (ss >> arg) {
        action.args.push_back(arg);
    }

    {
        std::lock_guard<std::mutex> lock(actionMutex);
        actionQueue.push(std::move(action));
    }
    // TODO:
    // - Validate command
    // - Check legality
    // - Mutate authoritative state
    // - Broadcast result
}

void MatchSession::processActions() {
    if (!running.load()) return;
    std::queue<PlayerAction> localQueue;

    {
        std::lock_guard<std::mutex> lock(actionMutex);
        std::swap(localQueue, actionQueue);
    }

    while (!localQueue.empty()) {
        const PlayerAction& action = localQueue.front();

        if (action.type == "PLAY") {
            if (action.args.size() >= 2) {
                int cardId = action.args[0];
                int lane = action.args[1];
                std::optional<int> targetLane;
                std::optional<int> targetIndex;

                if (action.args.size() >= 4) {
                    targetLane = action.args[2];
                    targetIndex = action.args[3];
                }

                PlayerState& player = players[action.playerIndex];
                const ServerCard* card = getCard(cardId);

                if (!card) {
                    std::cerr << "Unknown card ID " << cardId << "\n";
                    localQueue.pop();
                    continue;
                }

                if (card->getType() == CardType::Creature) {
                    handleSummon(action.playerIndex, cardId, lane);
                } else if (card->getType() == CardType::Spell) {
                    handleSpell(action.playerIndex, cardId, lane, targetLane, targetIndex);
                }
            }
        }
        else if (action.type == "DISCARD") {
            if (!action.args.empty())
                handleDiscard(action.playerIndex, action.args[0]);
        }
        else if (action.type == "SURRENDER") {
            handleSurrender(action.playerIndex);
            return;
        }

        localQueue.pop();
    }
}

void MatchSession::handleSurrender(int surrenderingPlayerIndex) {
    if (!running.load()) return; // already ended

    std::shared_ptr<PlayerConnection> winner;
    std::shared_ptr<PlayerConnection> loser;

    if (surrenderingPlayerIndex == 0) {
        loser = playerA;
        winner = playerB;
    } else if (surrenderingPlayerIndex == 1) {
        loser = playerB;
        winner = playerA;
    } else {
        std::cerr << "[MatchSession] Invalid player index for surrender\n";
        return;
    }
    endMatch(winner, loser);
}

void MatchSession::handleSummon(int playerIndex, int cardId, int lane) {
    if (!running.load()) return;
    auto& player = players[playerIndex];

    if (lane < 0 || lane >= board.laneCount) return;

    const ServerCard* card = getCard(cardId);
    if (!card) return;
    std::string name = card->getName();

    if (card->getType() != CardType::Creature) return;

    if (board.lanes[playerIndex][lane].has_value()) return;

    if (player.mana < card->getManaCost()) {
        std::cout << "[MatchSession] Insufficient Mana: " << player.mana << " : " << card->getManaCost() << "\n";
        
        return;
    }

    // Deduct mana
    player.mana -= card->getManaCost();

    // Find and remove card from hand
    auto it = findCardInHand(player, cardId);
    if (it == player.hand.end()) {
        std::cerr << "[ERROR] Card ID " << cardId
                  << " not found in player " << getUsername(playerIndex) << "'s hand\n";
        return;
    }
    player.hand.erase(it);

    // Place card on board
    board.lanes[playerIndex][lane] = cardId;
    const int combatEffectMask = CombatEffects::getMaskFromCard(card);
    board.continuousEffects[playerIndex][lane] =
        (combatEffectMask == 0) ? std::nullopt : std::optional<int>(combatEffectMask);
    const int regenValue = CombatEffects::getRegenValueFromCard(card);
    board.regen[playerIndex][lane] = (regenValue > 0)
        ? std::optional<std::pair<int, int>>(std::make_pair(regenValue, 2))
        : std::nullopt;

    // Send confirmation message
    std::cout << "[MatchSession] " << getUsername(playerIndex) << " Summons " << name << "\n";
    std::ostringstream ss;
    ss << "PLAY " << player.id << " " << cardId << " " << lane << "\n";
    playerA->send(ss.str());
    playerB->send(ss.str());

    //Call effects, if any
    triggerCardEffects(playerIndex, cardId, std::nullopt, std::nullopt);

}

// targetIndex: 0(casting player), 1(casting player's opponent), -1(all or none)
// targetLane: board lane selected as target
void MatchSession::handleSpell(int playerIndex, int cardId, int lane, std::optional<int> targetLane, std::optional<int> targetIndex) {
    if (!running.load()) return;
    PlayerState& player = players[playerIndex];

    const ServerCard* card = getCard(cardId);
    if (!card) return;
    std::string name = card->getName();

    if (player.mana < card->getManaCost()) {
        std::cout << "[MatchSession] Insufficient Mana: " << player.mana << " : " << card->getManaCost() << "\n";
        return;
    }

    // Deduct mana
    player.mana -= card->getManaCost();

    std::optional<int> serverTargetIndex;
    //Assign targetIndex to appropriate target
    // if 0 == targetIndex -> target itself
    // if 1 == targetIndex -> target opponent
    if (targetIndex.has_value()) {
        //serverTargetIndex: the player affected by the spell, relative to server logic
        serverTargetIndex = (*targetIndex == 0) ? playerIndex : 1 - playerIndex; //if targetIndex is -1 it automatically assumes it is for opponent

        //if targetIndex <0, means it is either universal, 0 side, or 1 side.
        if (*targetIndex < 0) {
            switch (*targetIndex) {
                case -1:
                    serverTargetIndex = -1; //ALL
                    break;
                case -2:
                    serverTargetIndex = playerIndex; //Target Caster
                    break;
                case -3:
                    serverTargetIndex = 1 - playerIndex; //Target Caster's Opponent
                    break;
            }
        }

    }    
    
    // Remove card from hand after cast
    auto it = findCardInHand(player, cardId);
    if (it == player.hand.end()) {
        std::cerr << "[ERROR] Card ID " << cardId
                  << " not found in player " << playerIndex << "'s hand\n";
        return;
    }
    player.hand.erase(it);

    // board.lanes[playerIndex][lane] = cardId;
    std::cout << "[MatchSession] " << getUsername(playerIndex) << " Casts " << name << "\n";

        //Send confirmation message on successful summon
    std::string spellMsg = "PLAY " + std::to_string(player.id) + " "
                    + std::to_string(cardId) + " "
                    + std::to_string(lane) + " "
                    + (targetLane ? std::to_string(*targetLane) : "-1") + " "
                    + (targetIndex ? std::to_string(*targetIndex) : "-1") + "\n";
    std::cout << "[MatchSession] Sending Spell Command: " << spellMsg << "\n";

    playerA->send(spellMsg);
    playerB->send(spellMsg);

    // Call Effect
    triggerCardEffects(playerIndex, cardId, targetLane, serverTargetIndex);
}

void MatchSession::handleDiscard(int playerIndex, int cardId) {
    if (!running.load()) return;
    auto& player = players[playerIndex];

    const ServerCard* card = getCard(cardId);
    if (!card) return;

    // increment mana
    player.mana += card->getManaCost();

    std::cout << "[MatchSession] " << getUsername(playerIndex) << " Discards " << card->getName() << "\n";

    // add to discard
    board.discard[playerIndex].push_back(cardId);

    // remove from hand using helper
    auto it = findCardInHand(player, cardId);
    if (it != player.hand.end()) {
        player.hand.erase(it);
    } else {
        std::cerr << "[WARNING] Tried to discard cardId " << cardId
                  << " but it was not in player " << getUsername(playerIndex) << "'s hand\n";
    }

    // broadcast
    std::string msg = "DISCARD " + std::to_string(player.id)
                    + " " + std::to_string(cardId) + "\n";
    playerA->send(msg);
    playerB->send(msg);
}

// --------------------------------------------------
// Disconnect Handling
// --------------------------------------------------
void MatchSession::handleDisconnect(const std::shared_ptr<PlayerConnection>& player) {
    if (!running.load()) return;

    std::cout << "[MatchSession]: Player disconnected\n";

    std::shared_ptr<PlayerConnection> winner;
    std::shared_ptr<PlayerConnection> loser = player;

    if (player == playerA) {
        winner = playerB;
    } else if (player == playerB) {
        winner = playerA;
    } else {
        std::cerr << "[MatchSession] Unknown player in disconnect\n";
        return;
    }

    // Notify the remaining player, may not be needed?
    if (winner && winner->isAlive()) {
        winner->send("OPPONENT_DISCONNECTED\n");
    }

    // End match properly (this handles cleanup + callbacks)
    endMatch(winner, loser);
}

const ServerCard* MatchSession::getCard(int id) const {
    auto it = cardCatalog.find(id);
    if (it != cardCatalog.end()) return it->second.get();
    return nullptr;
}

int MatchSession::getLaneCount() const {
    return board.laneCount;
}

int MatchSession::getCreaturesOwned(int playerIndex) const {
    if (playerIndex < 0 || playerIndex >= 2) {
        std::cout << "[DEBUG] WRONG INDEX\n";
        return 0;} // safety check
    int count = 0;
    for (int i = 0; i < board.laneCount; i++) {
        if (board.lanes[playerIndex][i].has_value()) {
            std::cout << "[DEBUG] PROC\n";

            ++count;
        }
    }
    std::cout << "[DEBUG] COUNT: " << count << "\n";

    return count;
}


const std::string MatchSession::getUsername(int playerIndex) const {
    return (playerIndex == 0) ? playerA->getUsername() : playerB->getUsername();
}

const int MatchSession::getPlayerHealth(int playerIndex) const {
    return players[playerIndex].health;
}


std::vector<int>::iterator MatchSession::findCardInHand(PlayerState& player, int cardId) {
    return std::find(player.hand.begin(), player.hand.end(), cardId);
}

//BOARD ACTIONS
//AUGMENT <playerId> <lane> <powerDelta> <toughnessDelta>
void MatchSession::augmentCreature(int targetPlayerIndex, int lane, std::pair<int,int> augment) {
    // Safety checks
    if (!running.load()) return;
    if (targetPlayerIndex < 0 || targetPlayerIndex >= 2) {
        std::cout << "[augmentCreature] targetPlayerIndex: " << std::to_string(targetPlayerIndex) << "\n";
        return;
    }
    if (lane < 0 || lane >= board.laneCount) return;

    // Check if there's a creature on that lane
    auto& cardOpt = board.lanes[targetPlayerIndex][lane];
    if (!cardOpt.has_value()) {
        std::cout << "[MatchSession::augmentCreature] No Creature On Lane " << lane
                  << " for " << getUsername(targetPlayerIndex) << "\n";
        return;
    }

    //Card
    int cardId = *cardOpt;            // get the integer card ID
    const ServerCard* card = getCard(cardId); // retrieve the card object from the catalog
    if (!card) {
        std::cerr << "[augmentCreature] Card ID " << cardId << " not found in catalog\n";
        return;
    }
    // Cast to CreatureCard
    const CreatureCard* creature = dynamic_cast<const CreatureCard*>(card);
    if (!creature) {
        std::cerr << "[augmentCreature] Card ID " << cardId << " is not a creature\n";
        return;
    }

    // Reference to the existing augment pair
    auto& existingAugment = board.augments[targetPlayerIndex][lane];

    const int prevPowerAug = existingAugment.has_value() ? existingAugment->first : 0;
    const int prevToughnessAug = existingAugment.has_value() ? existingAugment->second : 0;

    int nextPowerAug = prevPowerAug + augment.first;
    int nextToughnessAug = prevToughnessAug + augment.second;

    // If value reaches 0, destroy and return
    if (nextToughnessAug + creature->getToughness() <= 0) {
        destroyCreature(targetPlayerIndex, lane);
        return;
    }

    if (nextPowerAug == 0 && nextToughnessAug == 0) {
        existingAugment = std::nullopt;
    } else {
        existingAugment = std::make_pair(nextPowerAug, nextToughnessAug);
    }

    const int appliedPowerDelta = nextPowerAug - prevPowerAug;
    const int appliedToughnessDelta = nextToughnessAug - prevToughnessAug;
    if (appliedPowerDelta == 0 && appliedToughnessDelta == 0) {
        return;
    }

    // broadcast
    std::string msg = "AUGMENT "
        + std::to_string(players[targetPlayerIndex].id) + " "
        + std::to_string(lane) + " "
        + std::to_string(appliedPowerDelta) + " "
        + std::to_string(appliedToughnessDelta) + "\n";

    playerA->send(msg);
    playerB->send(msg);

}

void MatchSession::setCreature(int targetPlayerIndex, int lane, std::pair<int,int> augment) {
    // Safety checks
    if (!running.load()) return;
    if (targetPlayerIndex < 0 || targetPlayerIndex >= 2) return;
    if (lane < 0 || lane >= board.laneCount) return;

    // Check if there's a creature on that lane
    auto& cardOpt = board.lanes[targetPlayerIndex][lane];
    if (!cardOpt.has_value()) {
        std::cout << "[MatchSession::augmentCreature] No Creature On Lane " << lane
                  << " for " << getUsername(targetPlayerIndex) << "\n";
        return;
    }

    //Card
    int cardId = *cardOpt;            // get the integer card ID
    const ServerCard* card = getCard(cardId); // retrieve the card object from the catalog
    if (!card) {
        std::cerr << "[setCreature] Card ID " << cardId << " not found in catalog\n";
        return;
    }
    // Cast to CreatureCard
    const CreatureCard* creature = dynamic_cast<const CreatureCard*>(card);
    if (!creature) {
        std::cerr << "[setCreature] Card ID " << cardId << " is not a creature\n";
        return;
    }
    int power = creature->getPower();
    int toughness = creature->getToughness();
    int totalPower = power;
    int totalToughness = toughness;
    int setPower = augment.first;
    int setToughness = std::min(augment.second, toughness);

    // Reference to the existing augment pair
    auto& existingAugment = board.augments[targetPlayerIndex][lane];
    if (existingAugment.has_value()) {
        totalPower += existingAugment->first;
        totalToughness += existingAugment->second;
    }

    augment.first -= power;
    augment.second = setToughness - toughness;
    if (augment.first == 0 && augment.second == 0) {
        existingAugment = std::nullopt;
    } else {
        existingAugment = augment;
    }
   
    std::string msg = "AUGMENT " + std::to_string(players[targetPlayerIndex].id) + " "
                    + std::to_string(lane) + " "
                    + std::to_string(setPower - totalPower) + " "
                    + std::to_string(setToughness - totalToughness) + "\n";
    playerA->send(msg);
    playerB->send(msg);
}

void MatchSession::triggerCardEffects(int playerIndex, int cardId, std::optional<int> targetLane, std::optional<int> serverTargetIndex, TriggerType triggerType) {
    if (!running.load()) return;

    auto effects = TriggerEffects::getCardEffects(cardId);
    if (!effects) return;
    
    for (const auto& entry : *effects) {
        if (entry.trigger != triggerType) continue;

        EffectFunc effect = TriggerEffects::getEffectById(entry.effectId);
        if (!effect) continue;
        // Use a local copy so mutations don't bleed into the next iteration
        std::optional<int> localTargetIndex = serverTargetIndex;

        int targetCardId = -1;
        if (localTargetIndex.has_value() && targetLane.has_value()) { //if there is a targeted zone
            int tPlayer = *localTargetIndex;
            int tLane   = *targetLane;
            if (tPlayer >= 0 && tPlayer < 2 && tLane >= 0 && tLane < board.laneCount) {
                if (board.lanes[tPlayer][tLane].has_value()) { //check if the card still exists
                    targetCardId = *board.lanes[tPlayer][tLane];
                } else { //There is no card found.
                    // Only skip if there's no explicit target override that would redirect it
                    if (!entry.target.has_value() || *entry.target == Target::Nil) continue;
                }
            }
        }
        if (!entry.condition(*this, targetCardId,
                             targetLane.value_or(-1),
                             localTargetIndex.value_or(-1))) {
            continue;
        }
        if (entry.target.has_value()) { //assign hardcoded target, if any.
            switch (*entry.target) {
                case Target::Self:     localTargetIndex = playerIndex;         break;
                case Target::Opponent: localTargetIndex = 1 - playerIndex;     break;
                case Target::Nil:                                               break;
            }
        }
        effect(*this, playerIndex, targetLane, localTargetIndex,
               entry.amount, entry.augment);
    }
}

void MatchSession::addCreatureEffect(int targetPlayerIndex, int lane, int effectBit) {
    if (!running.load()) return;
    if (targetPlayerIndex < 0 || targetPlayerIndex >= 2) return;
    if (lane < 0 || lane >= board.laneCount) return;
    if (!board.lanes[targetPlayerIndex][lane].has_value()) return;

    const int currentMask = board.continuousEffects[targetPlayerIndex][lane].value_or(0);
    const int updatedMask = currentMask | effectBit;
    board.continuousEffects[targetPlayerIndex][lane] = (updatedMask == 0) ? std::nullopt : std::optional<int>(updatedMask);

    std::string msg = "EFFECT_ADD " + std::to_string(players[targetPlayerIndex].id) + " "
                    + std::to_string(lane) + " " + std::to_string(effectBit) + "\n";
    playerA->send(msg);
    playerB->send(msg);
}

void MatchSession::removeCreatureEffect(int targetPlayerIndex, int lane, int effectBit) {
    if (!running.load()) return;
    if (targetPlayerIndex < 0 || targetPlayerIndex >= 2) return;
    if (lane < 0 || lane >= board.laneCount) return;
    if (!board.lanes[targetPlayerIndex][lane].has_value()) return;

    const int currentMask = board.continuousEffects[targetPlayerIndex][lane].value_or(0);
    const int updatedMask = currentMask & (~effectBit);
    board.continuousEffects[targetPlayerIndex][lane] = (updatedMask == 0) ? std::nullopt : std::optional<int>(updatedMask);

    std::string msg = "EFFECT_REMOVE " + std::to_string(players[targetPlayerIndex].id) + " "
                    + std::to_string(lane) + " " + std::to_string(effectBit) + "\n";
    playerA->send(msg);
    playerB->send(msg);
}

void MatchSession::setCreatureRegen(int targetPlayerIndex, int lane, int regenValue) {
    if (!running.load()) return;
    if (targetPlayerIndex < 0 || targetPlayerIndex >= 2) return;
    if (lane < 0 || lane >= board.laneCount) return;
    if (!board.lanes[targetPlayerIndex][lane].has_value()) return;
    if (regenValue <= 0) return;

    const int currentMask = board.continuousEffects[targetPlayerIndex][lane].value_or(0);
    const int updatedMask = currentMask | CombatEffects::kRegen;
    board.continuousEffects[targetPlayerIndex][lane] = std::optional<int>(updatedMask);

    board.regen[targetPlayerIndex][lane] = std::make_pair(regenValue, 2);

    std::string effectMsg = "EFFECT_ADD " + std::to_string(players[targetPlayerIndex].id) + " "
                         + std::to_string(lane) + " " + std::to_string(CombatEffects::kRegen) + "\n";
    playerA->send(effectMsg);
    playerB->send(effectMsg);

    std::string msg = "REGEN_SET " + std::to_string(players[targetPlayerIndex].id) + " "
                    + std::to_string(lane) + " " + std::to_string(regenValue) + "\n";
    playerA->send(msg);
    playerB->send(msg);
}

//removes creature from board, resets augments/effects vectors
//DESTROY <playerId> <lane>
void MatchSession::destroyCreature(int targetPlayerIndex, int lane) {
    // Safety checks
    if (!running.load()) return;
    if (targetPlayerIndex < 0 || targetPlayerIndex >= 2) return;
    if (lane < 0 || lane >= board.laneCount) return;

    auto& cardOpt = board.lanes[targetPlayerIndex][lane];
    if (!cardOpt.has_value()) {
        std::cout << "[MatchSession::destroyCreature] No Creature On Lane "
                  << lane << " for " << getUsername(targetPlayerIndex) << "\n";
        return;
    }

    int cardId = cardOpt.value();

    // Move to discard pile
    board.discard[targetPlayerIndex].push_back(cardId);

    // Clear board state
    board.lanes[targetPlayerIndex][lane] = std::nullopt;
    board.augments[targetPlayerIndex][lane] = std::nullopt;
    board.continuousEffects[targetPlayerIndex][lane] = std::nullopt;
    board.regen[targetPlayerIndex][lane] = std::nullopt;

    std::cout << "[MatchSession::destroyCreature] Destroyed creature "
              << cardId << " on lane " << lane
              << " for " << getUsername(targetPlayerIndex) << "\n";

    // broadcast
    std::string msg = "DESTROY " + std::to_string(players[targetPlayerIndex].id) + " "
                    + std::to_string(lane)+ "\n";
    playerA->send(msg);
    playerB->send(msg);
}

//raises or lowers player health 
//HP <playerId> <delta>
void MatchSession::augmentHP(int playerIndex, int amount){
    if (!running.load()) return;
    players[playerIndex].health += amount;
    std::string msg = "HP " + std::to_string(players[playerIndex].id) + " "
                    + std::to_string(amount)+ "\n";

    std::cout << "[augmentHP] " << msg;

    playerA->send(msg);
    playerB->send(msg);

    if (players[playerIndex].health <= 0) { //end match
            std::shared_ptr<PlayerConnection> loser = playerA;
            std::shared_ptr<PlayerConnection> winner = playerB;
        if (playerIndex == 1) {
            loser = playerB;
            winner = playerA;
        }
        endMatch(winner, loser);
    }
} 

void MatchSession::setHP(int playerIndex, int amount){
    if (!running.load()) return;
    int delta = players[playerIndex].health - amount;
    players[playerIndex].health = amount;

    std::string msg = "HP " + std::to_string(players[playerIndex].id) + " "
                    + std::to_string(-delta)+ "\n";
    playerA->send(msg);
    playerB->send(msg);
} 

void MatchSession::augmentMana(int playerIndex, int amount){
    if (!running.load()) return;
    players[playerIndex].mana += amount;
    std::string msg = "MANA " + std::to_string(players[playerIndex].id) + " "
                    + std::to_string(amount)+ "\n";
    playerA->send(msg);
    playerB->send(msg);
} 

//COMBAT FUNCTIONS GO HERE
int MatchSession::getCreaturePower(int playerIndex, int lane) {
    std::cout << "[getCreaturePower] running=" << running.load() << " player=" << playerIndex << " lane=" << lane << "\n";

    auto& cardOpt = board.lanes[playerIndex][lane];
    if (!cardOpt) return 0;

    const ServerCard* card = getCard(*cardOpt);
    if (!card) return 0;
    const CreatureCard* creature = dynamic_cast<const CreatureCard*>(card);
    if (!creature) {
        std::cerr << "Card is not a creature\n";
        return -1;
    }

    int power = creature->getPower();    

    auto& aug = board.augments[playerIndex][lane];
    if (aug) {
        power += aug->first;
        std::cout << "[getCreaturePower] Augment Power: " << std::to_string(aug->first) << "\n";

    }
    std::cout << "[getCreaturePower] Final Power: " << std::to_string(power) << "\n";

    return power;
}

int MatchSession::getCreatureToughness(int playerIndex, int lane) {
    auto& cardOpt = board.lanes[playerIndex][lane];
    if (!cardOpt) return 0;

    const ServerCard* card = getCard(*cardOpt);
    if (!card) return 0;
    const CreatureCard* creature = dynamic_cast<const CreatureCard*>(card);
    if (!creature) {
        std::cerr << "Card is not a creature\n";
        return -1;
    }
    int toughness = creature->getToughness();

    auto& aug = board.augments[playerIndex][lane];
    if (aug) toughness += aug->second;

    return toughness;
}

void MatchSession::resolveAttackPhase() {
    std::cout << "[MatchSession] Attack Phase Entered\n";

    for (int lane = 0; lane < board.laneCount; lane++) {
        if (!running.load()) break;
        resolveLaneCombat(lane);
    }

    // After combat, check deaths
    // for (int p = 0; p < 2; p++) {
    //     for (int lane = 0; lane < board.laneCount; lane++) {
    //         auto& cardOpt = board.lanes[p][lane];
    //         if (!cardOpt) continue;

    //         int toughness = getCreatureToughness(p, lane);

    //         if (toughness <= 0) {
    //             destroyCreature(p, lane);
    //         }
    //     }
    // }
}

// COMBAT <lane> <playerA's card power> <playerB's card power>
// DIRECT <attacker ID> <lane> <damage>
void MatchSession::resolveLaneCombat(int lane) {
    if (!running.load()) return;

    auto& cardA = board.lanes[0][lane];
    auto& cardB = board.lanes[1][lane];

    if (!cardA && !cardB) return;

    const bool startedWithA = cardA.has_value();
    const bool startedWithB = cardB.has_value();

    auto hasEffect = [this](int playerIndex, int laneIndex, int effectBit) {
        const auto& effectMaskOpt = board.continuousEffects[playerIndex][laneIndex];
        return CombatEffects::hasEffect(effectMaskOpt, effectBit);
    };

    const bool hasDoubleStrikeA = hasEffect(0, lane, CombatEffects::kDoubleStrike);
    const bool hasDoubleStrikeB = hasEffect(1, lane, CombatEffects::kDoubleStrike);
    const bool hasTrampleA = hasEffect(0, lane, CombatEffects::kTrample);
    const bool hasTrampleB = hasEffect(1, lane, CombatEffects::kTrample);
    const bool hasDeathTouchA = hasEffect(0, lane, CombatEffects::kDeathTouch);
    const bool hasDeathTouchB = hasEffect(1, lane, CombatEffects::kDeathTouch);
    const bool hadLifestealA = hasEffect(0, lane, CombatEffects::kLifesteal);
    const bool hadLifestealB = hasEffect(1, lane, CombatEffects::kLifesteal);

    // UPDATED: Direct damage now includes lifesteal
    auto sendDirectDamage = [this, lane, &hadLifestealA, &hadLifestealB](int attackerIndex, int damage) {
        if (!running.load()) return;
        if (damage <= 0) return;

        std::cout << "[Combat] Direct attack: "
                  << getUsername(attackerIndex) << " deals "
                  << damage << " to "
                  << getUsername(1 - attackerIndex) << "\n";

        std::string msg = "DIRECT "
            + std::to_string(players[attackerIndex].id) + " "
            + std::to_string(lane) + " "
            + std::to_string(damage) + "\n";
        playerA->send(msg);
        playerB->send(msg);

        // Deal damage
        augmentHP(1 - attackerIndex, -damage);

        // 🔥 Lifesteal heal
        if ((attackerIndex == 0 && hadLifestealA) ||
            (attackerIndex == 1 && hadLifestealB)) {
            augmentHP(attackerIndex, damage);
        }
    };

    auto resolveCombatOnce = [&](bool allowDoubleStrikePhase) {
        if (!running.load()) return;
        (void)allowDoubleStrikePhase;

        bool aAlive = board.lanes[0][lane].has_value();
        bool bAlive = board.lanes[1][lane].has_value();
        if (!aAlive && !bAlive) return;

        int powerA = aAlive ? getCreaturePower(0, lane) : 0;
        int powerB = bAlive ? getCreaturePower(1, lane) : 0;
        int toughnessA = aAlive ? getCreatureToughness(0, lane) : 0;
        int toughnessB = bAlive ? getCreatureToughness(1, lane) : 0;

        // Direct attacks
        if (aAlive && !bAlive && powerA > 0) {
            sendDirectDamage(0, powerA);
            return;
        }
        if (!aAlive && bAlive && powerB > 0) {
            sendDirectDamage(1, powerB);
            return;
        }

        std::cout << "[Combat] Lane " << lane
                  << " A(" << powerA << ") vs B(" << powerB << ")\n";

        int overflowA = 0;
        int overflowB = 0;

        if (aAlive && bAlive && hasTrampleA && powerA > 0) {
            int lethalToB = hasDeathTouchA ? 1 : toughnessB;
            overflowA = std::max(0, powerA - lethalToB);
        }
        if (aAlive && bAlive && hasTrampleB && powerB > 0) {
            int lethalToA = hasDeathTouchB ? 1 : toughnessA;
            overflowB = std::max(0, powerB - lethalToA);
        }

        // Broadcast combat
        std::ostringstream ss;
        ss << "COMBAT "
           << players[0].id << " "
           << players[1].id << " "
           << lane << " "
           << powerA << " "
           << powerB << "\n";
        playerA->send(ss.str());
        playerB->send(ss.str());

        // UPDATED: Apply damage + lifesteal
        if (aAlive && powerA > 0) {
            int damageDealt = hasDeathTouchA ? 1 : powerA;
            augmentCreature(1, lane, {0, -damageDealt});

            if (hadLifestealA) {
                augmentHP(0, damageDealt);
            }
        }

        if (bAlive && powerB > 0) {
            int damageDealt = hasDeathTouchB ? 1 : powerB;
            augmentCreature(0, lane, {0, -damageDealt});

            if (hadLifestealB) {
                augmentHP(1, damageDealt);
            }
        }

        // Deathtouch instant kill
        if (aAlive && bAlive && hasDeathTouchA && powerA > 0) {
            destroyCreature(1, lane);
        }
        if (aAlive && bAlive && hasDeathTouchB && powerB > 0) {
            destroyCreature(0, lane);
        }

        // OnKill triggers
        if (aAlive && !board.lanes[1][lane].has_value()) {
            if (board.lanes[0][lane].has_value()) {
                triggerCardEffects(0, *board.lanes[0][lane],
                                   lane, 0, TriggerType::OnKill);
            }
        }
        if (bAlive && !board.lanes[0][lane].has_value()) {
            if (board.lanes[1][lane].has_value()) {
                triggerCardEffects(1, *board.lanes[1][lane],
                                   lane, 1, TriggerType::OnKill);
            }
        }

        // Trample overflow → uses lifesteal automatically
        if (overflowA > 0) sendDirectDamage(0, overflowA);
        if (overflowB > 0) sendDirectDamage(1, overflowB);
    };

    // Regen tick
    auto tickRegen = [this, lane](int playerIndex) {
        auto& regenOpt = board.regen[playerIndex][lane];
        if (!regenOpt.has_value()) return;
        if (!board.lanes[playerIndex][lane].has_value()) {
            regenOpt = std::nullopt;
            return;
        }

        int regenValue = regenOpt->first;
        int combatsUntilTrigger = regenOpt->second - 1;

        if (combatsUntilTrigger <= 0) {
            const auto& augOpt = board.augments[playerIndex][lane];
            int toughnessAug = augOpt ? augOpt->second : 0;
            int missing = (toughnessAug < 0) ? -toughnessAug : 0;
            int heal = std::min(regenValue, missing);

            if (heal > 0) {
                augmentCreature(playerIndex, lane, {0, heal});
            }
            combatsUntilTrigger = 2;
        }

        regenOpt = std::make_pair(regenValue, combatsUntilTrigger);
    };

    if (startedWithA) tickRegen(0);
    if (startedWithB) tickRegen(1);

    // Double strike phase
    if (hasDoubleStrikeA || hasDoubleStrikeB) {
        resolveCombatOnce(true);
    }

    if (!running.load()) return;

    // Normal combat
    resolveCombatOnce(false);
}

void MatchSession::endMatch(std::shared_ptr<PlayerConnection> winner,
                            std::shared_ptr<PlayerConnection> loser) {
    // Update game state, send messages
    if (!running.exchange(false)) return; // guard double-calls
    if (winner) {
        std::string msg = "MATCH_WON " + std::to_string(coinReward) + "\n";
        winner->send(msg);
        if (!giveRewards(winner)) std::cout << "[DEBUG] Failed to give rewards\n";
    }

    if (loser) loser->send("MATCH_LOST\n");

    // running = false; // stop game loop

    // Set state AFTER sending messages, BEFORE onMatchEnd fires from gameLoop
    if (winner) winner->state.store(ConnectionState::Waiting);
    if (loser)  loser->state.store(ConnectionState::Waiting);
}

//Give rewards to winner of match (eg. coins)
bool MatchSession::giveRewards(std::shared_ptr<PlayerConnection> winner) {

    const std::string cardsHost = EnvUtil::getServiceHost("CARDS_SERVICE", "127.0.0.1", "api.myapp.com");
    const int cardsPort = EnvUtil::getServicePort("CARDS_SERVICE", 8082, 443);

    std::string path = "/cards/inventories/coins/add";
    const int userId = winner->getPlayerId();

    if (userId <= 0) return false;
    std::ostringstream payload;
    payload << "{\"uid\":" << userId << ",\"coins\":" << coinReward << "}";

    int statusCode = -1;
    std::string responseBody;
    if (!HttpUtil::sendHttp(cardsHost, cardsPort, "PUT", path, payload.str(), statusCode, responseBody)) {
        return false;
    }
    return statusCode >= 200 && statusCode < 300;

}

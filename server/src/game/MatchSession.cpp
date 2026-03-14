#include "game/MatchSession.hpp"
#include "net/PlayerConnection.hpp"
#include "objects/CreatureCard.h"
#include "objects/SpellCard.h"
#include "utils/EnvUtil.hpp"
#include "utils/HttpUtil.hpp"
#include "utils/JsonUtil.hpp"

#include <iostream>
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
    stop();
}

// --------------------------------------------------
// Start / Stop
// --------------------------------------------------
bool MatchSession::start() {
    if (running.load()) return false;

    auto weakSelf = std::weak_ptr<MatchSession>(shared_from_this());
    playerA->onMessageReceived = [weakSelf](const std::vector<char>& raw) {
        if (auto self = weakSelf.lock()) {
            self->handlePlayerMessage(0, raw);
        }
    };
    playerB->onMessageReceived = [weakSelf](const std::vector<char>& raw) {
        if (auto self = weakSelf.lock()) {
            self->handlePlayerMessage(1, raw);
        }
    };

    try {
        gameThread = std::thread(&MatchSession::gameLoop, this);
    } catch (const std::exception& e) {
        std::cerr << "Failed to start match thread: " << e.what() << "\n";
        return false;
    }

    running = true;
    return true;
}

void MatchSession::stop() {
    if (!running.exchange(false)) return;

    if (gameThread.joinable()) {
        gameThread.join();
    }
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
        drawAndSend(0);
        drawAndSend(1);
    }
}


//Draw function: playerIndex player draws 1 card, other player will also receive instruction that their opponent drew 1 card
bool MatchSession::drawAndSend(int playerIndex) {
    auto& player = players[playerIndex];
    auto& deck = player.deck;

    int handSize = player.hand.size();
    if (handSize >= handLimit) {
        return false;
    }

    auto cardIdOpt = deck.draw();
    if (!cardIdOpt) return false;

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

// --------------------------------------------------
// Game Loop
// --------------------------------------------------
void MatchSession::gameLoop() {
    using namespace std::chrono;

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

        if (!playerA->isAlive() || !playerB->isAlive()) {
            handleDisconnect();
            break;
        }

        processActions();

        auto now = steady_clock::now();

        if (duration_cast<seconds>(now - lastDrawTime).count() >= drawInterval) {
            drawAndSend(0);
            drawAndSend(1);
            lastDrawTime = now;
        }

        if (duration_cast<seconds>(now - lastAttackTime).count() >= attackInterval) {
            resolveAttackPhase();
            lastAttackTime = now;
        }

        std::this_thread::sleep_for(10ms);
    }

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
                std::optional<int> targetId; //id of targeteted lane 
                std::optional<int> targetIndex; //if opponent or not

                if (action.args.size() > 2) {
                    targetId = action.args[2];
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
                    handleSpell(action.playerIndex, cardId, lane, targetId, targetIndex);
                }
            }
        }
        else if (action.type == "DISCARD") {
            if (!action.args.empty())
                handleDiscard(action.playerIndex, action.args[0]);
        }

        localQueue.pop();
    }
}

void MatchSession::handleSummon(int playerIndex, int cardId, int lane) {
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

    // Send confirmation message
    std::cout << "[MatchSession] " << getUsername(playerIndex) << " Summons " << name << "\n";
    std::ostringstream ss;
    ss << "PLAY " << player.id << " " << cardId << " " << lane << "\n";
    playerA->send(ss.str());
    playerB->send(ss.str());
}

//targetIndex: 0(casting player), 1(casting player's opponent), -1(all or none)
//targetId: represents target zone
void MatchSession::handleSpell(int playerIndex, int cardId, int lane, std::optional<int> targetId, std::optional<int> targetIndex) {
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

    //Assign targetIndex to appropriate target
    // if 0 == targetIndex -> target itself
    // if 1 == targetIndex -> target opponent
    if (targetIndex.has_value()) {
        //serverTargetIndex: the player affected by the spell, relative to server logic
        int serverTargetIndex = (*targetIndex == 0) ? playerIndex : 1 - playerIndex;
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
                    + (targetId ? std::to_string(*targetId) : "-1") + " "
                    + (targetIndex ? std::to_string(*targetIndex) : "-1") + "\n";
    std::cout << "[MatchSession] Sending Spell Command: " << spellMsg << "\n";

    playerA->send(spellMsg);
    playerB->send(spellMsg);

    // Call Effect



}

void MatchSession::handleDiscard(int playerIndex, int cardId) {
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
void MatchSession::handleDisconnect() {
    std::cout << "[MatchSession]: Player disconnected\n";

    if (playerA->isAlive()) {
        playerA->send("OPPONENT_DISCONNECTED\n");
    }
    if (playerB->isAlive()) {
        playerB->send("OPPONENT_DISCONNECTED\n");
    }

    running = false;
}

const ServerCard* MatchSession::getCard(int id) const {
    auto it = cardCatalog.find(id);
    if (it != cardCatalog.end()) return it->second.get();
    return nullptr;
}

int MatchSession::getLaneCount() const {
    return board.laneCount;
}

const std::string MatchSession::getUsername(int playerIndex) const {
    return (playerIndex == 0) ? playerA->getUsername() : playerB->getUsername();
}

std::vector<int>::iterator MatchSession::findCardInHand(PlayerState& player, int cardId) {
    return std::find(player.hand.begin(), player.hand.end(), cardId);
}

//BOARD ACTIONS
//AUGMENT <playerId> <lane> <powerDelta> <toughnessDelta>
void MatchSession::augmentCreature(int targetPlayerIndex, int lane, std::pair<int,int> augment) {
    // Safety checks
    if (targetPlayerIndex < 0 || targetPlayerIndex >= 2) return;
    if (lane < 0 || lane >= board.laneCount) return;

    // Check if there's a creature on that lane
    auto& cardOpt = board.lanes[targetPlayerIndex][lane];
    if (!cardOpt.has_value()) {
        std::cout << "[MatchSession::augmentCreature] No Creature On Lane " << lane
                  << " for " << getUsername(targetPlayerIndex) << "\n";
        return;
    }

    // Reference to the existing augment pair
    auto& existingAugment = board.augments[targetPlayerIndex][lane];

    if (existingAugment.has_value()) {
        // Stack augments: add to existing values
        existingAugment->first  += augment.first;
        existingAugment->second += augment.second;
    } else {
        // No existing augment: set initial values
        existingAugment = augment;
    }

    // broadcast
    std::string msg = "AUGMENT "
        + std::to_string(players[targetPlayerIndex].id) + " "
        + std::to_string(lane) + " "
        + std::to_string(augment.first) + " "
        + std::to_string(augment.second) + "\n";

    playerA->send(msg);
    playerB->send(msg);

}

void MatchSession::setCreature(int targetPlayerIndex, int lane, std::pair<int,int> augment) {
    // Safety checks
    if (targetPlayerIndex < 0 || targetPlayerIndex >= 2) return;
    if (lane < 0 || lane >= board.laneCount) return;

    // Check if there's a creature on that lane
    auto& cardOpt = board.lanes[targetPlayerIndex][lane];
    if (!cardOpt.has_value()) {
        std::cout << "[MatchSession::augmentCreature] No Creature On Lane " << lane
                  << " for " << getUsername(targetPlayerIndex) << "\n";
        return;
    }

    // Reference to the existing augment pair
    auto& existingAugment = board.augments[targetPlayerIndex][lane];
    existingAugment = augment;
    
}

//removes creature from board, resets augments/effects vectors
//DESTROY <playerId> <lane>
void MatchSession::destroyCreature(int targetPlayerIndex, int lane) {
    // Safety checks
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
    players[playerIndex].health += amount;


} 

//COMBAT FUNCTIONS GO HERE
int MatchSession::getCreaturePower(int playerIndex, int lane) {
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
    if (aug) power += aug->first;

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
        resolveLaneCombat(lane);
    }

    // After combat, check deaths
    for (int p = 0; p < 2; p++) {
        for (int lane = 0; lane < board.laneCount; lane++) {
            auto& cardOpt = board.lanes[p][lane];
            if (!cardOpt) continue;

            int toughness = getCreatureToughness(p, lane);

            if (toughness <= 0) {
                destroyCreature(p, lane);
            }
        }
    }
}

// COMBAT <lane> <playerA's card power> <playerB's card power>
// DIRECT <attacker ID> <lane> <damage>
void MatchSession::resolveLaneCombat(int lane) {

    auto& cardA = board.lanes[0][lane];
    auto& cardB = board.lanes[1][lane];

    if (!cardA && !cardB) return;

    if (cardA && cardB) {

        int powerA = getCreaturePower(0, lane);
        int powerB = getCreaturePower(1, lane);

        std::cout << "[Combat] Lane " << lane
                  << " A(" << powerA << ") vs B(" << powerB << ")\n";

        augmentCreature(0, lane, {0, -powerB});
        augmentCreature(1, lane, {0, -powerA});

        std::ostringstream ss;
        ss << "COMBAT "
            << players[0].id << " "
            << players[1].id << " "
            << lane << " "
            << powerA << " "
            << powerB << "\n";
        playerA->send(ss.str());
        playerB->send(ss.str());
    } else {
        int attackerIndex = -1;
        if (cardA) attackerIndex = 0;
        else if (cardB) attackerIndex = 1;

        int power = getCreaturePower(attackerIndex, lane);

        std::cout << "[Combat] Direct attack: "
                  << getUsername(attackerIndex) << " deals "
                  << power << " to "
                  << getUsername(1 - attackerIndex) << "\n";

        std::string msg = "DIRECT " + std::to_string(players[attackerIndex].id) + " " + std::to_string(lane) + " " + std::to_string(power) + "\n";
        playerA->send(msg);
        playerB->send(msg);

        augmentHP(1 - attackerIndex, -power);

    }
}
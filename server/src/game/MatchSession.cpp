#include "game/MatchSession.hpp"
#include "net/PlayerConnection.hpp"
#include "objects/CreatureCard.h"
#include "objects/SpellCard.h"
#include "utils/JsonUtil.hpp"

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib/httplib.h"

#include <iostream>
#include <chrono>
#include <thread>
#include <mutex>
#include <queue>

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

    httplib::Client client("host.docker.internal", 8082);
    client.set_connection_timeout(3); // seconds
    client.set_read_timeout(5);       // seconds

    std::string path = "/cardbase/decks/" + std::to_string(playerId);
    std::cout << "[DEBUG] GET " << path << "\n";

    auto res = client.Get(path.c_str());

    if (!res) {
        std::cerr << "[ERROR] Request failed. Error: " << res.error() << "\n";
        return false;
    }

    std::cout << "[DEBUG] HTTP Status: " << res->status << "\n";

    if (res->status != 200) {
        std::cerr << "[ERROR] Unexpected HTTP status: " << res->status << "\n";
        std::cerr << "[ERROR] Response body: " << res->body << "\n";
        return false;
    }

    std::cout << "[DEBUG] Response body:\n" << res->body << "\n";

    // Uncomment and use this when parsing JSON
    return parseDeckJson(res->body, outDeck);

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
        
        return;}

    // Deduct mana
    player.mana -= card->getManaCost();

    // Find and remove card from hand
    auto it = findCardInHand(player, cardId);
    if (it == player.hand.end()) {
        std::cerr << "[ERROR] Card ID " << cardId
                  << " not found in player " << playerIndex << "'s hand\n";
        return;
    }
    player.hand.erase(it);

    // Place card on board
    board.lanes[playerIndex][lane] = cardId;

    // Send confirmation message
    std::cout << "[MatchSession] Summoning " << name << "\n";
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
    std::cout << "[MatchSession] Casting " << name << "\n";

        //Send confirmation message on successful summon
    std::string msg = "PLAY " + std::to_string(player.id) + " "
                    + std::to_string(cardId) + " "
                    + std::to_string(lane) + " "
                    + (targetId ? std::to_string(*targetId) : "-1") + " "
                    + (targetIndex ? std::to_string(*targetIndex) : "-1") + "\n";
    std::cout << "[MatchSession] Sending Spell Command: " << msg << "\n";

    playerA->send(msg);
    playerB->send(msg);

    // Call Effect

}

void MatchSession::handleDiscard(int playerIndex, int cardId) {
    auto& player = players[playerIndex];

    const ServerCard* card = getCard(cardId);
    if (!card) return;

    // increment mana
    player.mana += card->getManaCost();

    std::cout << "[MatchSession] Discarding " << card->getName() << "\n";

    // add to discard
    board.discard[playerIndex].push_back(cardId);

    // remove from hand using helper
    auto it = findCardInHand(player, cardId);
    if (it != player.hand.end()) {
        player.hand.erase(it);
    } else {
        std::cerr << "[WARNING] Tried to discard cardId " << cardId
                  << " but it was not in player " << playerIndex << "'s hand\n";
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

std::vector<int>::iterator MatchSession::findCardInHand(PlayerState& player, int cardId) {
    return std::find(player.hand.begin(), player.hand.end(), cardId);
}
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

// --------------------------------------------------
// Draw logic
// --------------------------------------------------
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

    player.hand.push_back(cardId);   // if you only need ID, keep this

    // auto& conn = (playerIndex == 0) ? playerA : playerB;
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
        // Disconnect check (optional)
        if (!playerA->isAlive() || !playerB->isAlive()) {
            handleDisconnect();
            break;
        }

        auto now = steady_clock::now();

        // Draw a card every 5 seconds
        if (duration_cast<seconds>(now - lastDrawTime).count() >= 5) {
            {
                std::lock_guard<std::mutex> lock(stateMutex);
                drawAndSend(0); // draw for player a
                drawAndSend(1); // draw for player b
            }
            lastDrawTime = now;
        }

        // Small sleep to avoid busy loop
        std::this_thread::sleep_for(10ms);
    }
    std::cout << "[MatchSession] Exiting Match Game Loop...\n";
}

void MatchSession::handlePlayerMessage(int playerIndex, const std::vector<char>& raw) {
    std::string msg(raw.begin(), raw.end());

    std::cout << "[Match] Player " << players[playerIndex].id << " -> " << msg;

    // TODO:
    // - Validate command
    // - Check legality
    // - Mutate authoritative state
    // - Broadcast result
}

// --------------------------------------------------
// Disconnect Handling
// --------------------------------------------------
void MatchSession::handleDisconnect() {
    std::cout << "MatchSession: player disconnected\n";

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
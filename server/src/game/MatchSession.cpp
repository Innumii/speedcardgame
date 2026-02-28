#include "game/MatchSession.hpp"
#include "net/PlayerConnection.hpp"
#include "objects/Deck.h"
#include "objects/CreatureCard.h"
#include "objects/SpellCard.h"
#include "httplib/httplib.h"
#include "utils/JsonUtil.hpp" // Make sure this has JSON parsing helpers

#include <iostream>
#include <chrono>
#include <thread>

MatchSession::MatchSession(std::shared_ptr<PlayerConnection> a,
                           std::shared_ptr<PlayerConnection> b,
                         const std::vector<std::unique_ptr<Card>>& cards)
    : playerA(std::move(a)), playerB(std::move(b)), allCards(cards) {}

MatchSession::~MatchSession() {
    stop();
}

// --------------------------------------------------
// Start / Stop
// --------------------------------------------------
bool MatchSession::start() {
    if (running.load()) return false;

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
bool MatchSession::loadDeckForPlayer(int playerId, Deck& outDeck) {
    // std::cout << "[DEBUG] Loading deck for Player " << playerId << "\n";

    httplib::Client client("host.docker.internal", 8082);
    client.set_connection_timeout(3); // seconds
    client.set_read_timeout(5);       // seconds

    std::string path = "/cardbase/decks/" + std::to_string(playerId);
    // std::cout << "[DEBUG] GET " << path << "\n";

    auto res = client.Get(path.c_str());

    if (!res) {
        std::cerr << "[ERROR] Request failed. Error: " << res.error() << "\n";
        return false;
    }

    // std::cout << "[DEBUG] HTTP Status: " << res->status << "\n";

    if (res->status != 200) {
        std::cerr << "[ERROR] Unexpected HTTP status: " << res->status << "\n";
        std::cerr << "[ERROR] Response body: " << res->body << "\n";
        return false;
    }

    // std::cout << "[DEBUG] Response body:\n" << res->body << "\n";

    // Uncomment and use this when parsing JSON
    return parseDeckJson(res->body, outDeck);

    // return true;
}

//map the JSON card IDs to create their Card obj version, then feed into deck
bool MatchSession::parseDeckJson(const std::string& jsonStr, Deck& outDeck) {
    // Find the "cards" array
    std::size_t pos = jsonStr.find("\"cards\"");
    if (pos == std::string::npos) {
        std::cerr << "[ERROR] 'cards' field not found in JSON\n";
        return false;
    }

    pos = jsonStr.find('[', pos);
    if (pos == std::string::npos) {
        std::cerr << "[ERROR] '[' not found after 'cards'\n";
        return false;
    }
    ++pos; // skip '['

    while (pos < jsonStr.size()) {
        // Skip whitespace
        while (pos < jsonStr.size() && std::isspace(static_cast<unsigned char>(jsonStr[pos]))) ++pos;

        if (pos >= jsonStr.size() || jsonStr[pos] == ']') break; // end of array

        int cid = 0;
        if (!JsonUtil::parseJsonIntAt(jsonStr, pos, cid)) {
            std::cerr << "[WARN] Failed to parse card ID at position " << pos << "\n";
            return false;
        }

        // Skip optional comma
        while (pos < jsonStr.size() && (jsonStr[pos] == ',' || std::isspace(static_cast<unsigned char>(jsonStr[pos])))) ++pos;

        // Find the card in the available cards pool
        auto it = std::find_if(allCards.begin(), allCards.end(),
            [cid](const std::unique_ptr<Card>& c) { return c->getId() == cid; });

        if (it == allCards.end()) {
            std::cerr << "[WARN] Card ID " << cid << " not found in allCards\n";
            continue; // skip invalid IDs
        }

        const Card* src = it->get();
        // Clone card into the deck
        if (src->getType() == CardType::Creature) {
            const CreatureCard* c = dynamic_cast<const CreatureCard*>(src);
            outDeck.addCard(std::make_unique<CreatureCard>(
                c->getName(), c->getText(), c->getManaValue(),
                c->getManaCost(), c->getPower(), c->getToughness(), c->getId()
            ));
        } else {
            const SpellCard* s = dynamic_cast<const SpellCard*>(src);
            outDeck.addCard(std::make_unique<SpellCard>(
                s->getName(), s->getText(), s->getManaValue(),
                s->getManaCost(), s->getId()
            ));
        }
    }

    return true;
}
void MatchSession::setupDecks() {
    // TODO: Replace with real decklists/seeding
    std::cout << "Attempting deck setup...\n";
    loadDeckForPlayer(playerA.get()->getPlayerId(), players[0].deck);
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

    if (deck.isEmpty())
        return false;

    int cardId = deck.draw().get()->getId();
    player.hand.push_back(cardId);

    auto& conn = (playerIndex == 0) ? playerA : playerB;

    conn->send("DRAW " + std::to_string(cardId) + "\n");
    return true;
}

// --------------------------------------------------
// Game Loop
// --------------------------------------------------
void MatchSession::gameLoop() {
    using namespace std::chrono_literals;

    std::cout << "Game loop started\n";

    playerA->send("MATCH_START\n");
    playerB->send("MATCH_START\n");

    setupDecks();
    sendOpeningHands();

    while (running.load()) {
        if (!playerA->isAlive() || !playerB->isAlive()) {
            handleDisconnect();
            break;
        }

        std::string msg;

        // Relay A → B (TEMPORARY until full authority)
        while (playerA->pollMessage(msg) || playerB->pollMessage(msg)) {
            if (playerA->pollMessage(msg)) {
                playerA->send(msg);
            }
            if (playerB->pollMessage(msg)) {
                playerB->send(msg);
            }
        }


        std::this_thread::sleep_for(1ms);
    }

    std::cout << "Game loop exiting\n";
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
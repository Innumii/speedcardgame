#include "game/MatchSession.hpp"
#include "net/PlayerConnection.hpp"

#include <iostream>
#include <chrono>
#include <thread>

MatchSession::MatchSession(std::shared_ptr<PlayerConnection> a,
                           std::shared_ptr<PlayerConnection> b)
    : playerA(std::move(a)), playerB(std::move(b)), board(5) {}

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
void MatchSession::setupDecks() {
    // TODO: Replace with real decklists/seeding
    // for (int i = 0; i < 30; ++i) {
    //     players[0].deck.addCardById(i);
    //     players[1].deck.addCardById(i);
    // }

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
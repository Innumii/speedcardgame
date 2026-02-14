#include "game/MatchSession.hpp"
#include "net/PlayerConnection.hpp"

#include <iostream>
#include <chrono>
#include <thread>

MatchSession::MatchSession(std::shared_ptr<PlayerConnection> a,
                            std::shared_ptr<PlayerConnection> b)
            :playerA(std::move(a)), playerB(std::move(b)) {}

MatchSession::~MatchSession() {
    stop(); // Ensure clean shutdown
}

bool MatchSession::start() {
    if (running.load()) return false;

    try {
        gameThread = std::thread(&MatchSession::gameLoop, this);
    } catch (const std::exception& e) {
        std::cerr << "Failed to start match thread: " << e.what() << "\n";
        return false;
    }
    std::cout << "MatchSession started between "
              << playerA->getSocket() << " and "
              << playerB->getSocket() << "\n";

    running = true;
    return true;
}

void MatchSession::stop() {
    if (!running.exchange(false)) return;

    if (gameThread.joinable()) {
        gameThread.join();
    }

    std::cout << "MatchSession stopped\n";
}

void MatchSession::gameLoop() {
    using namespace std::chrono_literals;
    std::cout << " Game loop started\n";

    playerA->send("MATCH_START\n");
    playerB->send("MATCH_START\n");

    while (running.load()) {
        //disconnect logic
        if (!playerA->isAlive() || !playerB->isAlive()) {
            handleDisconnect();
            break;
        }
        // --- Relay messages (simple authoritative server model) ---

        std::string msg;
        // Player A → B
        while (playerA->pollMessage(msg)) {
            playerB->send(msg);
        }

        // Player B → A
        while (playerB->pollMessage(msg)) {
            playerA->send(msg);
        }

        std::this_thread::sleep_for(1ms);

    }
    std::cout << "Game loop exiting\n";

}

void MatchSession::handleDisconnect() {
    std::cout << "MatchSession: player disconnected\n";

    // Notify remaining player
    if (playerA->isAlive()) {
        playerA->send("OPPONENT_DISCONNECTED\n");
    }
    if (playerB->isAlive()) {
        playerB->send("OPPONENT_DISCONNECTED\n");
    }

    running = false;
}
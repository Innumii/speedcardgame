#ifndef MATCHSESSION_HPP
#define MATCHSESSION_HPP

#include <memory>
#include <thread>
#include <atomic>
#include <vector>
#include <optional>

#include "objects/Deck.h"

class PlayerConnection;

// ----------------------------
// Lightweight server board
// ----------------------------
struct ServerBoard {
    int laneCount = 5;
    std::vector<std::optional<int>> lanes[2]; // card IDs
    std::vector<int> discard[2];

    ServerBoard(int lanesCount = 5) : laneCount(lanesCount) {
        lanes[0].resize(laneCount);
        lanes[1].resize(laneCount);
    }
};

// ----------------------------
// Player match state
// ----------------------------
struct PlayerState {
    Deck deck;
    std::vector<int> hand; // card IDs only
};

// ----------------------------
// MatchSession
// ----------------------------
class MatchSession {
public:
    MatchSession(std::shared_ptr<PlayerConnection> playerA,
                 std::shared_ptr<PlayerConnection> playerB);
    ~MatchSession();

    MatchSession(const MatchSession&) = delete;
    MatchSession& operator=(const MatchSession&) = delete;

    bool start();
    void stop();

    // Setup phase
    void setupDecks();
    void sendOpeningHands();
    bool drawAndSend(int playerIndex);

private:
    void gameLoop();
    void handleDisconnect();

    // Players
    std::shared_ptr<PlayerConnection> playerA;
    std::shared_ptr<PlayerConnection> playerB;

    std::thread gameThread;
    std::atomic<bool> running{false};

    // Authoritative state
    PlayerState players[2];   // 0 = A, 1 = B
    ServerBoard board;
};

#endif
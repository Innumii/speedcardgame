#ifndef MATCHSESSION_HPP
#define MATCHSESSION_HPP

#include <memory>
#include <thread>
#include <atomic>

class PlayerConnection;

class MatchSession {
public:
    MatchSession(std::shared_ptr<PlayerConnection> playerA,
                 std::shared_ptr<PlayerConnection> playerB);
    ~MatchSession();

    // Non-copyable (matches should never be copied)
    MatchSession(const MatchSession&) = delete;
    MatchSession& operator=(const MatchSession&) = delete;

    bool start();
    void stop();

private:
    void gameLoop();
    void handleDisconnect();
    std::shared_ptr<PlayerConnection> playerA;
    std::shared_ptr<PlayerConnection> playerB;

    std::thread gameThread;
    std::atomic<bool> running{false};
};

#endif
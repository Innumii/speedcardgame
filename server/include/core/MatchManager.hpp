#ifndef MATCHMANAGER_HPP
#define MATCHMANAGER_HPP

#include <memory>
#include <vector>
#include <mutex>
#include <functional>

#include "net/PlayerConnection.hpp"
#include "game/MatchSession.hpp"
#include "objects/ServerCard.h"
#include "core/Matchmaker.hpp"

class GameServer;
class MatchManager {
public:
    MatchManager(GameServer& server);

    // Called by Matchmaker when a pair is found
    void onPairFound(std::shared_ptr<PlayerConnection> a,
                     std::shared_ptr<PlayerConnection> b);

    // Called by network layer when client responds
    void onAccept(std::shared_ptr<PlayerConnection> player);
    // Optional: handle disconnects safely
    void onPlayerDisconnected(std::shared_ptr<PlayerConnection> player);

    void setMatchmaker(Matchmaker* mm) { matchmaker = mm; }


private:
    struct PendingMatch {
        std::shared_ptr<PlayerConnection> a;
        std::shared_ptr<PlayerConnection> b;
        bool aAccepted = false;
        bool bAccepted = false;
    };
    Matchmaker* matchmaker = nullptr; // non-owning
    GameServer& server; // non-owning reference to the authoritative server
    std::mutex mutex; //for pendingMatches and activeMatches

    std::vector<std::shared_ptr<PendingMatch>> pendingMatches;
    std::vector<std::shared_ptr<MatchSession>> activeMatches;

    std::shared_ptr<PendingMatch>
    findPending(const std::shared_ptr<PlayerConnection>& player);

    std::shared_ptr<MatchSession> startMatch(const std::shared_ptr<PendingMatch>& match);
    void removePending(const std::shared_ptr<PendingMatch>& match);

    // ---- Networking helpers ----
    void sendMatchFound(const std::shared_ptr<PlayerConnection>& player);
    void sendMatchCancelled(const std::shared_ptr<PlayerConnection>& player);
    // void sendMatchStart(const std::shared_ptr<PlayerConnection>& player);


};

#endif
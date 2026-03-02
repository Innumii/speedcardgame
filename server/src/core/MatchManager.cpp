#include "core/MatchManager.hpp"
#include <algorithm>
#include <iostream>
#include "objects/ServerCard.h"
#include "core/GameServer.hpp"


MatchManager::MatchManager(GameServer& gs) 
    : server(gs) 
{}

void MatchManager::onPairFound(std::shared_ptr<PlayerConnection> a,
                              std::shared_ptr<PlayerConnection> b)
{
    if (!a || !b) return;

    std::cout << "[MatchManager] Found a Pair!\n";
    std::cout << "[MatchManager] Creating Pending Match...\n";

    auto pending = std::make_shared<PendingMatch>();
    pending->a = a;
    pending->b = b;

    {
        std::lock_guard<std::mutex> lock(mutex);
        pendingMatches.push_back(pending);
    }

    std::cout << "Notifying players: " << a->getUsername() << " VS " << b->getUsername() << "\n";

    sendMatchFound(a);
    sendMatchFound(b);
}

void MatchManager::onAccept(std::shared_ptr<PlayerConnection> player)
{
    std::cout << "[MatchManager] " << player.get()->getUsername() << " accepted\n";
    auto match = findPending(player);
    if (!match) return;

    {
        std::lock_guard<std::mutex> lock(mutex);

        if (match->a == player) match->aAccepted = true;
        if (match->b == player) match->bAccepted = true;

        if (match->aAccepted && match->bAccepted) {
            startMatch(match);
        }
    }
}


//still have to account for active matches
//currently broken
void MatchManager::onPlayerDisconnected(std::shared_ptr<PlayerConnection> player)
{
    auto match = findPending(player);
    if (!match) return;

    std::shared_ptr<PlayerConnection> other;
    other = (match->a == player) ? match->b : match->a;

    {
        std::lock_guard<std::mutex> lock(mutex);
        removePending(match);
    }

    if (other) {
        // std::cout << "[MatchManager/before sendMatchCancelled] other.use_count() = " << other.use_count() << "\n";
        sendMatchCancelled(other);
        if (matchmaker) {
            // std::cout << "[MatchManager/before enqueuePlayer] other.use_count() = " << other.use_count() << "\n";
            matchmaker->enqueuePlayer(other);
            std::cout << "Requeued player " << other->getUsername() << " after opponent disconnected\n";
        }
    }
}

std::shared_ptr<MatchManager::PendingMatch>
MatchManager::findPending(const std::shared_ptr<PlayerConnection>& player)
{
    std::lock_guard<std::mutex> lock(mutex);

    for (auto& m : pendingMatches) {
        if (m->a == player || m->b == player) {
            return m;
        }
    }
    std::cout << "Nope\n";
    return nullptr;
}

void MatchManager::startMatch(const std::shared_ptr<PendingMatch>& match)
{
    //This is so fkin stupid
    //clone every card

    auto session = std::make_shared<MatchSession>(match->a, match->b, server.getAllCards());
    session->start();

    activeMatches.push_back(session);

    std::cout << "Match started: "
              << match->a->getUsername() << " vs "
              << match->b->getUsername() << "\n";

    // sendMatchStart(match->a);
    // sendMatchStart(match->b);

    removePending(match);
}

void MatchManager::removePending(const std::shared_ptr<PendingMatch>& match)
{
    pendingMatches.erase(
        std::remove(pendingMatches.begin(), pendingMatches.end(), match),
        pendingMatches.end()
    );

    std::cout << "[MatchManager] Pending Matches left: " << pendingMatches.size() << "\n";
}

//
// ---- Network helpers ----
// You can replace these with your actual packet protocol
//

void MatchManager::sendMatchFound(const std::shared_ptr<PlayerConnection>& player)
{
    if (!player) return;
    player->send("MATCH_FOUND\n");
    std::cout << "sent match found!\n";
}

void MatchManager::sendMatchCancelled(const std::shared_ptr<PlayerConnection>& player)
{
    if (!player) return;
    player->send("MATCH_CANCELLED\n");
}


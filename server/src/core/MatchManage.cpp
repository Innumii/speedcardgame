#include "core/MatchManager.hpp"
#include <algorithm>
#include <iostream>
#include <string>

namespace {
std::string sanitizeLineToken(std::string value) {
    for (char& ch : value) {
        if (ch == '\n' || ch == '\r' || ch == '|') {
            ch = ' ';
        }
    }
    return value;
}
}

MatchManager::MatchManager() {}

void MatchManager::onPairFound(std::shared_ptr<PlayerConnection> a,
                              std::shared_ptr<PlayerConnection> b)
{
    if (!a || !b) return;

    constexpr std::size_t startingHandSize = 6;
    constexpr std::size_t startingDeckSize = 34;

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
    sendOpponentInfo(a, b);
    sendOpponentInfo(b, a);
    sendOpponentCounts(a, startingHandSize, startingDeckSize);
    sendOpponentCounts(b, startingHandSize, startingDeckSize);
}

void MatchManager::onAccept(std::shared_ptr<PlayerConnection> player)
{
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

    {
        std::lock_guard<std::mutex> lock(mutex);
        other = (match->a == player) ? match->b : match->a;
        removePending(match);
    }

    if (other) {
        sendMatchCancelled(other);
        if (matchmaker) {
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
    auto session = std::make_shared<MatchSession>(match->a, match->b);
    session->start();

    activeMatches.push_back(session);

    std::cout << "Match started: "
              << match->a->getSocket() << " vs "
              << match->b->getSocket() << "\n";

    sendMatchStart(match->a);
    sendMatchStart(match->b);

    removePending(match);
}

void MatchManager::removePending(const std::shared_ptr<PendingMatch>& match)
{
    pendingMatches.erase(
        std::remove(pendingMatches.begin(), pendingMatches.end(), match),
        pendingMatches.end()
    );
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

void MatchManager::sendMatchStart(const std::shared_ptr<PlayerConnection>& player)
{
    if (!player) return;
    player->send("MATCH_START\n");
}

void MatchManager::sendOpponentInfo(const std::shared_ptr<PlayerConnection>& toPlayer,
                                    const std::shared_ptr<PlayerConnection>& opponent)
{
    if (!toPlayer || !opponent) return;

    const int opponentId = opponent->getPlayerId();
    const std::string opponentName = sanitizeLineToken(opponent->getUsername());
    toPlayer->send("OPPONENT_INFO|" + std::to_string(opponentId) + "|" + opponentName + "\n");
}

void MatchManager::sendOpponentCounts(const std::shared_ptr<PlayerConnection>& toPlayer,
                                      std::size_t handCount,
                                      std::size_t deckCount)
{
    if (!toPlayer) return;
    toPlayer->send("OPPONENT_COUNTS|" + std::to_string(handCount) + "|" + std::to_string(deckCount) + "\n");
}

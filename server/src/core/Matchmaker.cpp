#include "core/Matchmaker.hpp"
#include <algorithm>
#include <iostream>

//queue is checked when enqueueing a player, immediately try to empty out the queue
void Matchmaker::enqueuePlayer(const std::shared_ptr<PlayerConnection>& player) {
    if (!player) return;

    //lock the mutex so subsequent code below is protected
    std::lock_guard<std::mutex> lock(queueMutex);
    playerQueue.push(player);

    tryCreateMatch();

    //after scope ends, lock_guard automatically unlocks mutex
}

void Matchmaker::removePlayer(const std::shared_ptr<PlayerConnection>& player) {
    std::lock_guard<std::mutex> lock(queueMutex);

    //std::queue does not support removing arbitrarily, this is a workaround
    //change this later to a better implementation
    std::queue<std::shared_ptr<PlayerConnection>> tempQueue;

    while (!playerQueue.empty()) {
        auto p = playerQueue.front();
        playerQueue.pop();
        if (p != player) {
            tempQueue.push(p);
        }
    }

    std::swap(playerQueue, tempQueue);
    std::cout << player->getUsername() << " removed from Queue\n";
}

void Matchmaker::tryCreateMatch() {
    // Keep pairing players while we have at least 2 in queue
    while (playerQueue.size() >= 2) {
        auto playerA = playerQueue.front(); playerQueue.pop();
        auto playerB = playerQueue.front(); playerQueue.pop();

        if (!playerA || !playerB) continue; // safety check

        //create MatchSession
        // auto match = std::make_shared<MatchSession>(playerA, playerB);

        if (onMatchReady) {
            onMatchReady(playerA, playerB);
        }

        //start the match asynchronously
        // match->start();

        std::cout << "Match found between players "
                  << playerA->getUsername() << " and "
                  << playerB->getUsername() << "\n";
    }
}
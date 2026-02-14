#include "core/Matchmaker.hpp"
#include <algorithm>
#include <iostream>

void Matchmaker::enqueuePlayer(const std::shared_ptr<PlayerConnection>& player) {
    if (!player) return;

    //lock the mutex so subsequent code below is protected
    std::lock_guard<std::mutex> lock(queueMutex);
    playerQueue.push(player);

    tryCreateMatch();

    //after scope ends, lock)guard automatically unlocks mutex
}
#include "core/Board.hpp"
#include "objects/Card.h"
#include <iostream>

//:lanes(laneCount) executes before constructor body
//initialises each lane as a std::vector, and calls the vector constructor
Board::Board(int count):laneCount(count){
    for (int player = 0; player < 2; player++) {
        lanes[player].resize(laneCount);
    }

}

bool Board::isZoneEmpty(int lane, int playerId) const {
    if (playerId < 0 || playerId > 1 || lane < 0 || lane >= laneCount) return false;
    return !lanes[playerId][lane];
}

bool Board::addToPlay(int lane, int playerId, std::unique_ptr<Card> card) {
    if (!isZoneEmpty(lane, playerId) || !card) return false;
    if (playerId < 0 || playerId > 1 || lane < 0 || lane >= laneCount) return false;

    try {
        lanes[playerId][lane] = std::move(card);
    } catch (...) {
        std::cerr << "Failed to place card\n";
        return false;
    }
    std::cout << "[Board] Successfully added\n";

    return true;
}

void Board::displayPlay(int playerId) {
    if (playerId < 0 || playerId > 1) {
            std::cerr << "Invalid player ID\n";
            return;
    }

    const auto& pile = lanes[playerId];
    if (pile.empty()) {
        std::cout << "Player " << playerId << "'s play zone is empty.\n";
        return;
    }

    std::cout << "Player " << playerId << "'s play zone:\n";
    for (int i = 0; i < laneCount; i++) {
        if (pile[i] && *pile[i]) { //if optional has value + card ptr is not null
            std::cout << i << ": " << (*pile[i])->getName() << "\n";
        } else {
            std::cout << i << "\n";
        }
    }
}

const std::optional<std::unique_ptr<Card>>& Board::getZone(int lane, int playerId) const {
    if (playerId < 0 || playerId > 1 || lane < 0 || lane >= laneCount) {
        throw std::out_of_range("Invalid lane or playerId");
    }
    return lanes[playerId][lane];
}

int Board::getLaneCount() const {
    return laneCount;
}

bool Board::addToDiscard(std::unique_ptr<Card> card, int playerId) {
    if (!card) return false; //may not need
    if (playerId < 0 || playerId > 1) {
            std::cerr << "Invalid player ID\n";
            return false;
    }

    try {
        discard[playerId].push_back(std::move(card));  
        std::cout << "Discarded!\n";
    } catch (...) {
        std::cerr << "Failed to add to discard\n";
        return false;
    }
    
    return true;
}

void Board::displayDiscard(int playerId) {
    if (playerId < 0 || playerId > 1) {
            std::cerr << "Invalid player ID\n";
            return;
    }

    const auto& pile = discard[playerId];
    if (pile.empty()) {
        std::cout << "Player " << playerId << "'s discard pile is empty.\n";
        return;
    }

    std::cout << "Player " << playerId << "'s discard pile:\n";
    for (const auto& cardPtr : pile) {
        if (cardPtr) { // sanity check
            std::cout << " - " << cardPtr->getName() << "\n";
        }
    }
}
#include "core/Board.hpp"
#include "objects/Card.h"
#include <iostream>

//:lanes(laneCount) executes before constructor body
//initialises each lane as a std::vector, and calls the vector constructor
Board::Board(int laneCount):lanes(laneCount){

}

bool Board::isLaneEmpty(int lane) const {
    return !lanes[lane].has_value();
}

bool Board::placeCard(int lane, std::unique_ptr<Card> card) {
    if (!isLaneEmpty(lane) || !card) return false;

    try {
        lanes[lane] = std::move(card);
    } catch (...) {
        std::cerr << "Failed to place card\n";
        return false;
    }

    return true;
}

const std::optional<std::unique_ptr<Card>>& Board::getLane(int lane) const {
    return lanes[lane];
}

int Board::laneCount() const {
    return lanes.size();
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
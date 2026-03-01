#ifndef SERVERDECK_H
#define SERVERDECK_H

#include "objects/ServerCard.h"
#include <memory>
#include <vector>
#include <optional>
#include <algorithm>
#include <random>

class ServerDeck {
private:
    std::vector<std::shared_ptr<ServerCard>> cards;

public:
    ServerDeck() = default;

    // Add a card to the deck
    void addCard(std::shared_ptr<ServerCard> card);

    // Remove all cards
    void clear();

    // Shuffle the deck
    void shuffle();

    // Draw top card
    std::shared_ptr<ServerCard> draw();

    // Take a card by its ID (removes it from deck)
    std::shared_ptr<ServerCard> takeCardById(int cardId);

    // Query
    bool isEmpty() const;
    int size() const;
};

#endif
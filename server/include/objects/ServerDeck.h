#ifndef SERVERDECK_H
#define SERVERDECK_H

#include <vector>
#include <algorithm>
#include <random>
#include <optional>

class ServerDeck {
private:
    std::vector<int> cards;   // store card IDs only

public:
    ServerDeck() = default;

    // Add a card by ID
    void addCard(int cardId);

    // Remove all cards
    void clear();

    // Shuffle the deck
    void shuffle();

    // Draw top card (returns ID)
    std::optional<int> draw();

    // Take a card by its ID (removes it from deck)
    std::optional<int> takeCardById(int cardId);

    // Query
    bool isEmpty() const;
    int size() const;
};

#endif
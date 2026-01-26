#ifndef DECK_H
#define DECK_H

#include "Card.h"

#include <memory>
#include <vector>

class Deck {
private:
    std::vector<std::unique_ptr<Card>> cards;

public:
    void addCard(std::unique_ptr<Card> card);
    void shuffle();
    bool isEmpty() const;
    std::unique_ptr<Card> draw();
    int size() const;
};

#endif
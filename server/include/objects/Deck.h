#ifndef DECK_H
#define DECK_H

#include "Card.h"
#include <memory>
#include <vector>
#include <optional>

class Deck {
private:
    std::vector<std::unique_ptr<Card>> cards;

public:
    Deck() = default;
    void addCard(std::unique_ptr<Card> card);
    void clear();

    //Server side
    void shuffle();                         // server only
    std::unique_ptr<Card> draw();          // draw top

    //Client side
    std::unique_ptr<Card> takeCardById(int cardId);

    bool isEmpty() const;
    int size() const;

    //Deck resetting
    // std::vector<int> exportCardIds() const;
    // void importFromIds(const std::vector<int>& ids);
};

#endif
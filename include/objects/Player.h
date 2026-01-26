#ifndef PLAYER_H
#define PLAYER_H

#include <memory>
#include <vector>
#include "Card.h"
#include "Deck.h"

class Player {
public:
    int health = 100;
    int fatigueDamage = 1;
    std::vector<std::unique_ptr<Card>> hand;

    bool handFull() const;
    void drawCard(Deck& deck);
    bool isDead() const;
};

#endif

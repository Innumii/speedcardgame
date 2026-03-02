#ifndef PLAYER_H
#define PLAYER_H

#include <memory>
#include <vector>
#include "Card.h"
#include "Deck.h"

class Player {
public:
    int id = 1;
    int health = 100;
    int fatigueDamage = 1;
    int mana = 0;
    std::vector<std::unique_ptr<Card>> hand;
    Deck deck;
    int isOpponent = false;

    bool handFull() const;
    // void drawCard(Deck& deck);
    void addMana(int amount);
    bool isDead() const;
    Deck& getDeck();
    void setDeck(Deck newDeck);
    void addCardToHand(std::unique_ptr<Card> card);
    
    void setIsOpponent(bool opponentStatus) {
        isOpponent = opponentStatus;
    }
};

#endif
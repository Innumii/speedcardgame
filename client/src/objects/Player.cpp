#include "objects/Player.h"
#include <cerrno>

bool Player::handFull() const {
    return hand.size() >= 7;
}

bool Player::isDead() const {
    return health <= 0;
}

void Player::addMana(int amount) {
    if (amount <= 0) return;
    mana += amount;
}

Deck& Player::getDeck() {
    return deck;
}

void Player::setDeck(Deck newDeck) {
    this->deck = std::move(newDeck); // move the deck instead of copy
}

//take card from deck and add to hand
void Player::addCardToHand(std::unique_ptr<Card> card) {
    hand.push_back(std::move(card));
}
#include "objects/Player.h"

bool Player::handFull() const {
    return hand.size() >= 7;
}

void Player::drawCard(Deck& deck) {
    if (handFull()) return;

    if (!deck.isEmpty()) {
        hand.push_back(deck.draw());
        fatigueDamage = 1;
    } else {
        health -= fatigueDamage;
        fatigueDamage++;
    }
}

bool Player::isDead() const {
    return health <= 0;
}

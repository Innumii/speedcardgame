#include "objects/Player.h"

Player::Player()
    : id(0),
      health(100),
      mana(0),
      fatigueDamage(1),
      isOpponent(false),
      deckSize(0)
{
}

bool Player::handFull() const {
    return handInstanceIds.size() >= MAX_HAND_SIZE;
}

std::size_t Player::handSize() const {
    return handInstanceIds.size();
}

bool Player::isDead() const {
    return health <= 0;
}

void Player::setIsOpponent(bool opponentStatus) {
    isOpponent = opponentStatus;
}

void Player::setHand(const std::vector<int>& newHand) {
    handInstanceIds = newHand;
}

void Player::addCardToHand(int instanceId) {
    handInstanceIds.push_back(instanceId);
}

void Player::removeCardFromHand(std::size_t index) {
    if (index < handInstanceIds.size()) {
        handInstanceIds.erase(handInstanceIds.begin() + index);
    }
}

void Player::setDeckSize(int size) {
    deckSize = size;
}
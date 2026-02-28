#include "objects/CreatureCard.h"
#include <iostream>

CreatureCard::CreatureCard(const std::string& name,
                           const std::string& text,
                           int manaValue,
                           int manaCost,
                           int power,
                           int toughness,
                           int cardId)
    : Card(name, text, manaValue, manaCost, CardType::Creature, cardId),
      power(power),
      toughness(toughness) {}

int CreatureCard::getPower() const { return power; }
int CreatureCard::getToughness() const { return toughness; }

void CreatureCard::drawExtraInfo() const {
    std::cout << "  " << power << "/" << toughness << "\n";
}

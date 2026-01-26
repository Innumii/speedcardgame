#include "objects/CreatureCard.h"
#include <iostream>

CreatureCard::CreatureCard(std::string name,
                           std::string text,
                           int manaValue,
                           int manaCost,
                           int power,
                           int toughness)
    : Card(name, text, manaValue, manaCost, CardType::Creature),
      power(power),
      toughness(toughness) {}

int CreatureCard::getPower() const { return power; }
int CreatureCard::getToughness() const { return toughness; }

void CreatureCard::drawExtraInfo() const {
    std::cout << "  " << power << "/" << toughness << "\n";
}

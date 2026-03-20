#include "objects/CreatureCard.h"
#include <iostream>

CreatureCard::CreatureCard(const std::string& name,
                           const std::string& text,
                           int manaValue,
                           int manaCost,
                           int power,
                           int toughness,
                           int cardId,
                           Rarity rarity)
    : Card(name, text, manaValue, manaCost, CardType::Creature, cardId, rarity),
      power(power),
      basePower(power), 
      baseToughness(toughness),
      toughness(toughness) {}

int CreatureCard::getPower() const { return power; }
int CreatureCard::getToughness() const { return toughness; }
int CreatureCard::getBasePower() const { return basePower; }
int CreatureCard::getBaseToughness() const { return baseToughness; }

std::string CreatureCard::getText() const {
    std::string fullText = text;
    if (hasGrantedEffects()) {
        for (const std::string& effect : grantedEffects) {
            if (!fullText.empty() && fullText.back() != '\n') {
                fullText += "\n";
            }
            fullText += effect;
        }
    }
    return fullText;
}

void CreatureCard::drawExtraInfo() const {
    std::cout << "  " << power << "/" << toughness << "\n";
}
void CreatureCard::augmentStats(int powerDelta, int toughnessDelta) {
    power += powerDelta;
    toughness += toughnessDelta;
}
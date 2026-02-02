#include "objects/SpellCard.h"

SpellCard::SpellCard(std::string name,
                     std::string text,
                     int manaValue,
                     int manaCost)
    : Card(name, text, manaValue, manaCost, CardType::Spell) {}

void SpellCard::drawExtraInfo() const {
    // Spells have no extra info
}

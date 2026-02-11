#include "objects/SpellCard.h"

SpellCard::SpellCard(std::string name,
                     std::string text,
                     int manaValue,
                     int manaCost,
                     int cardId)
    : Card(name, text, manaValue, manaCost, CardType::Spell, cardId) {}

void SpellCard::drawExtraInfo() const {
    // Spells have no extra info
}

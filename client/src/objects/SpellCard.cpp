#include "objects/SpellCard.h"

SpellCard::SpellCard(const std::string& name,
                     const std::string& text,
                     int manaValue,
                     int manaCost,
                     int cardId,
                     Rarity rarity)
    : Card(name, text, manaValue, manaCost, CardType::Spell, cardId, rarity) {}

void SpellCard::drawExtraInfo() const {
    // Spells have no extra info
}

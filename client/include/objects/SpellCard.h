#ifndef SPELL_CARD_H
#define SPELL_CARD_H

#include "Card.h"

class SpellCard : public Card {
public:
    SpellCard(std::string name,
              std::string text,
              int manaValue,
              int manaCost,
              int cardId = -1);

    void drawExtraInfo() const override;
};

#endif

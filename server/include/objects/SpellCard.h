#ifndef SPELL_CARD_H
#define SPELL_CARD_H

#include "Card.h"

class SpellCard : public Card {
public:
    SpellCard(const std::string& name,
              const std::string& text,
              int manaValue,
              int manaCost,
              int cardId = -1);

    void drawExtraInfo() const override;
    std::unique_ptr<Card> clone() const override {
        return std::make_unique<SpellCard>(*this);
    }
};

#endif

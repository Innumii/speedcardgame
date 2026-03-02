#ifndef SPELL_CARD_H
#define SPELL_CARD_H

#include "objects/ServerCard.h"

class SpellCard : public ServerCard {
public:
    SpellCard(const std::string& name,
              const std::string& text,
              int manaValue,
              int manaCost,
              int cardId = -1);

    void drawExtraInfo() const override;
    std::shared_ptr<ServerCard> clone() const override {
        return std::make_shared<SpellCard>(*this);
    }
};

#endif

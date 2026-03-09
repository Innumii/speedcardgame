#ifndef CREATURE_CARD_H
#define CREATURE_CARD_H

#include "Card.h"
#include <memory>

class CreatureCard : public Card {
private:
    int power;
    int toughness;

public:
    CreatureCard(const std::string& name,
                 const std::string& text,
                 int manaValue,
                 int manaCost,
                 int power,
                 int toughness,
                 int cardId = -1);

    int getPower() const;
    int getToughness() const;

    void drawExtraInfo() const override;

    std::unique_ptr<Card> clone() const override {
        return std::make_unique<CreatureCard>(*this);
    }
};


#endif

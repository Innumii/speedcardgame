#ifndef CREATURE_CARD_H
#define CREATURE_CARD_H

#include "Card.h"

class CreatureCard : public Card {
private:
    int power;
    int toughness;

public:
    CreatureCard(std::string name,
                 std::string text,
                 int manaValue,
                 int manaCost,
                 int power,
                 int toughness);

    int getPower() const;
    int getToughness() const;

    void drawExtraInfo() const override;
};

#endif

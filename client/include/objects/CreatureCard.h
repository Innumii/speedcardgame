#ifndef CREATURE_CARD_H
#define CREATURE_CARD_H

#include "Card.h"
#include <memory>

class CreatureCard : public Card {
private:
    int power;
    int toughness;
    int basePower;
    int baseToughness;

public:
    CreatureCard(const std::string& name,
                 const std::string& text,
                 int manaValue,
                 int manaCost,
                 int power,
                 int toughness,
                 int cardId = -1,
                 Rarity rarity = Rarity::Common);

    int getPower() const;
    int getToughness() const;
    int getBasePower() const;
    int getBaseToughness() const;
    std::string getText() const override;
    
    void augmentStats(int powerDelta, int toughnessDelta);
    void drawExtraInfo() const override;

    std::unique_ptr<Card> clone() const override {
        return std::make_unique<CreatureCard>(*this);
    }
};


#endif

#ifndef CARD_H
#define CARD_H

#include <string>

enum class CardType {
    Creature,
    Spell
};

class Card {
protected:
    std::string name;
    std::string text;
    int manaValue;
    int manaCost;
    CardType type;

public:
    Card(std::string name,
         std::string text,
         int manaValue,
         int manaCost,
         CardType type);

    virtual ~Card() = default;

    // Getters
    std::string getName() const;
    std::string getText() const;
    int getManaCost() const;
    int getManaValue() const;
    CardType getType() const;

    // Polymorphic draw hook
    virtual void drawExtraInfo() const = 0;
};

#endif

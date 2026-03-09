#ifndef CARD_H
#define CARD_H

#include <string>
#include <memory>

enum class CardType {
    Creature,
    Spell
};

class Card {
protected:
    int id;
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
            CardType type,
            int cardId = -1);

    virtual ~Card() = default;
    virtual std::unique_ptr<Card> clone() const = 0;

    // Getters
    std::string getName() const;
    std::string getText() const;
    int getManaCost() const;
    int getManaValue() const;
    CardType getType() const;
    int getId() const;

    // Polymorphic draw hook
    virtual void drawExtraInfo() const = 0;
};

#endif

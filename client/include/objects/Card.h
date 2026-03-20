#ifndef CARD_H
#define CARD_H

#include <string>
#include <memory>
#include <vector>

enum class CardType {
    Creature,
    Spell
};

enum class Rarity {
    Common,
    Uncommon,
    Rare,
    VeryRare,
    SuperRare
};

class Card {
protected:
    int id;
    std::string name;
    std::string text;
    int manaValue;
    int manaCost;
    CardType type;
    Rarity rarity = Rarity::Common;
    std::vector<std::string> grantedEffects;

public:
    Card(std::string name,
         std::string text,
         int manaValue,
         int manaCost,
            CardType type,
            int cardId = -1,
            Rarity rarity = Rarity::Common);

    virtual ~Card() = default;
    virtual std::unique_ptr<Card> clone() const = 0;

    // Getters
    std::string getName() const;
    std::string getText() const;
    int getManaCost() const;
    int getManaValue() const;
    CardType getType() const;
    int getId() const;
    Rarity getRarity() const;
    bool hasGrantedEffects() const;
    void addGrantedEffect(const std::string& effectText);
    void removeGrantedEffect(const std::string& effectText);
    void clearGrantedEffects();

    // Polymorphic draw hook
    virtual void drawExtraInfo() const = 0;
};

#endif

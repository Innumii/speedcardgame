#ifndef SERVERCARD_H
#define SERVERCARD_H

#include <string>
#include <memory>

enum class CardType {
    Creature,
    Spell
};

class ServerCard {
protected:
    int id;
    std::string name;
    std::string text;
    int manaValue;
    int manaCost;
    CardType type;

public:
    ServerCard(std::string name,
         std::string text,
         int manaValue,
         int manaCost,
            CardType type,
            int cardId = -1);

    virtual ~ServerCard() = default;
    virtual std::shared_ptr<ServerCard> clone() const = 0;
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

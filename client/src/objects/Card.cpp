#include "objects/Card.h"

Card::Card(std::string name,
           std::string text,
           int manaValue,
           int manaCost,
           CardType type,
           int cardId,
           Rarity rarity)
    : id(cardId),
      name(name),
      text(text),
      manaValue(manaValue),
      manaCost(manaCost),
      type(type),
      rarity(rarity) {}

std::string Card::getName() const { return name; }
std::string Card::getText() const { return text; }
int Card::getManaCost() const { return manaCost; }
int Card::getManaValue() const { return manaValue; }
CardType Card::getType() const { return type; }
int Card::getId() const { return id; }
Rarity Card::getRarity() const { return rarity; }

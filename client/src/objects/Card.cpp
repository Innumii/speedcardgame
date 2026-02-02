#include "objects/Card.h"

Card::Card(std::string name,
           std::string text,
           int manaValue,
           int manaCost,
           CardType type)
    : name(name),
      text(text),
      manaValue(manaValue),
      manaCost(manaCost),
      type(type) {}

std::string Card::getName() const { return name; }
std::string Card::getText() const { return text; }
int Card::getManaCost() const { return manaCost; }
int Card::getManaValue() const { return manaValue; }
CardType Card::getType() const { return type; }

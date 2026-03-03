#include "objects/ServerCard.h"

ServerCard::ServerCard(std::string name,
           std::string text,
           int manaValue,
           int manaCost,
           CardType type,
           int cardId)
    : id(cardId),
      name(name),
      text(text),
      manaValue(manaValue),
      manaCost(manaCost),
      type(type) {}

std::string ServerCard::getName() const { return name; }
std::string ServerCard::getText() const { return text; }
int ServerCard::getManaCost() const { return manaCost; }
int ServerCard::getManaValue() const { return manaValue; }
CardType ServerCard::getType() const { return type; }
int ServerCard::getId() const { return id; }

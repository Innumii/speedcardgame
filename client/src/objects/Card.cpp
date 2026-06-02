#include "objects/Card.h"

#include <algorithm>

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
std::string Card::getText() const {
  if (grantedEffects.empty()) return text;

  std::string combined = text;
  if (!combined.empty()) combined += "\n";
  combined += "Gained: ";

  for (std::size_t i = 0; i < grantedEffects.size(); ++i) {
    if (i > 0) combined += ", ";
    combined += grantedEffects[i];
  }

  return combined;
}
int Card::getManaCost() const { return manaCost; }
int Card::getManaValue() const { return manaValue; }
CardType Card::getType() const { return type; }
int Card::getId() const { return id; }
Rarity Card::getRarity() const { return rarity; }

bool Card::hasGrantedEffects() const {
  return !grantedEffects.empty();
}

void Card::addGrantedEffect(const std::string& effectText) {
  if (effectText.empty()) return;
  auto it = std::find(grantedEffects.begin(), grantedEffects.end(), effectText);
  if (it == grantedEffects.end()) {
    grantedEffects.push_back(effectText);
  }
}

void Card::removeGrantedEffect(const std::string& effectText) {
  if (effectText.empty()) return;
  grantedEffects.erase(
    std::remove(grantedEffects.begin(), grantedEffects.end(), effectText),
    grantedEffects.end()
  );
}

void Card::clearGrantedEffects() {
  grantedEffects.clear();
}

bool Card::hasGrantedEffect(const std::string& effectText) const {
    return std::find(grantedEffects.begin(), grantedEffects.end(), effectText)
           != grantedEffects.end();
}

void Card::removeGrantedEffectsWithPrefix(const std::string& prefix) {
    grantedEffects.erase(
        std::remove_if(grantedEffects.begin(), grantedEffects.end(),
            [&prefix](const std::string& e) {
                return e.rfind(prefix, 0) == 0; // starts with prefix
            }),
        grantedEffects.end()
    );
}
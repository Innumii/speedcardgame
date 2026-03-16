#include "game/CombatEffects.hpp"

#include "objects/ServerCard.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace {

std::string toLowerCopy(const std::string& input) {
    std::string lowered = input;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return lowered;
}

} // namespace

namespace CombatEffects {

int getMaskFromCard(const ServerCard* card) {
    if (!card || card->getType() != CardType::Creature) return 0;

    int mask = 0;
    const std::string textLower = toLowerCopy(card->getText());

    if (textLower.find("double strike") != std::string::npos) {
        mask |= kDoubleStrike;
    }
    if (textLower.find("trample") != std::string::npos) {
        mask |= kTrample;
    }

    return mask;
}

bool hasEffect(const std::optional<int>& effectMaskOpt, int effectBit) {
    return effectMaskOpt.has_value() && ((*effectMaskOpt & effectBit) != 0);
}

} // namespace CombatEffects

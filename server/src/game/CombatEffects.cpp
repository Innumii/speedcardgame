#include "game/CombatEffects.hpp"

#include "objects/ServerCard.h"
#include "utils/StringUtil.hpp"

#include <algorithm>
#include <cctype>
#include <regex>
#include <string>

namespace CombatEffects {

int getMaskFromCard(const ServerCard* card) {
    if (!card || card->getType() != CardType::Creature) return 0;

    int mask = 0;
    const std::string textLower = StringUtil::toLower(card->getText());

    if (textLower.find("double strike") != std::string::npos) {
        mask |= kDoubleStrike;
    }
    if (textLower.find("trample") != std::string::npos) {
        mask |= kTrample;
    }
    if (textLower.find("deathtouch") != std::string::npos) {
        mask |= kDeathTouch;
    }
    if (textLower.find("regen") != std::string::npos) {
        mask |= kRegen;
    }
    if (textLower.find("lifesteal") != std::string::npos) {
        mask |= kLifesteal;
    }

    return mask;
}

int getRegenValueFromCard(const ServerCard* card) {
    if (!card || card->getType() != CardType::Creature) return 0;

    const std::string textLower = StringUtil::toLower(card->getText());
    static const std::regex regenPattern(R"(regen\s+(-?\d+))");
    std::smatch match;

    if (!std::regex_search(textLower, match, regenPattern) || match.size() < 2) {
        return 0;
    }

    try {
        return std::max(0, std::stoi(match[1].str()));
    } catch (...) {
        return 0;
    }
}

bool hasEffect(const std::optional<int>& effectMaskOpt, int effectBit) {
    return effectMaskOpt.has_value() && ((*effectMaskOpt & effectBit) != 0);
}

} // namespace CombatEffects

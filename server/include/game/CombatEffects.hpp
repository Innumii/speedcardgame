#pragma once

#include <optional>

class ServerCard;

namespace CombatEffects {

constexpr int kDoubleStrike = 1 << 0;
constexpr int kTrample = 1 << 1;
constexpr int kDeathTouch = 1 << 2;
constexpr int kRegen = 1 << 3;
constexpr int kLifesteal = 1 << 4;

int getMaskFromCard(const ServerCard* card);
int getRegenValueFromCard(const ServerCard* card);
bool hasEffect(const std::optional<int>& effectMaskOpt, int effectBit);

} // namespace CombatEffects

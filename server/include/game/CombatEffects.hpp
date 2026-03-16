#pragma once

#include <optional>

class ServerCard;

namespace CombatEffects {

constexpr int kDoubleStrike = 1 << 0;
constexpr int kTrample = 1 << 1;

int getMaskFromCard(const ServerCard* card);
bool hasEffect(const std::optional<int>& effectMaskOpt, int effectBit);

} // namespace CombatEffects

#pragma once

#include <functional>
#include <optional>
#include <unordered_map>

class MatchSession;

using SpellEffect = std::function<void(
    MatchSession&,
    int playerIndex,
    int lane,
    std::optional<int> targetId,
    std::optional<int> targetIndex)>; //0 for playerA, 1 for playerB

class SpellEffects {
public:
    static SpellEffect getEffect(int cardId);
};
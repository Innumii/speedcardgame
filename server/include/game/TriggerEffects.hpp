#pragma once
#include <functional>
#include <optional>
#include <unordered_map>
#include <vector>
class MatchSession;

using EffectFunc = std::function<void(
    MatchSession& session,
    int playerIndex,
    std::optional<int> targetLane,
    std::optional<int> targetIndex,
    std::optional<int> amount,
    std::optional<std::pair<int,int>> augment
)>;

enum class Target { Self, Opponent, Nil };
enum class TriggerType {
    OnCast,    // spells, default for all existing entries
    OnKill,    // creature killed an enemy in combat
    OnCombat,  // fires every combat phase
};

struct CardEffectEntry {
    int effectId;
    std::optional<int> amount;
    std::optional<std::pair<int,int>> augment;
    std::optional<Target> target;
    std::function<bool(const MatchSession&, int cardId, int targetLane, int targetIndex)> condition
        = [](const MatchSession&, int, int, int){ return true; };
    TriggerType trigger = TriggerType::OnCast; // <-- after condition, existing entries unaffected
};

class TriggerEffects {
public:
    static EffectFunc getEffectById(int effectId);
    static const std::vector<CardEffectEntry>* getCardEffects(int cardId);
private:
    static const std::vector<EffectFunc> effects;
    static const std::unordered_map<int, std::vector<CardEffectEntry>> cardToEffectsMap;
};
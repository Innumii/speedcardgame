#include "game/Effects.hpp"
#include "game/MatchSession.hpp"

//Logic
// Sequential list of effects (index = effectId)
const std::vector<EffectFunc> Effects::effects = {

    // 0: Augment creature
    [](MatchSession& session, int, std::optional<int> targetLane,
       std::optional<int> targetIndex, std::optional<int> amount,
       std::optional<std::pair<int,int>> augment) {
        if (!targetLane || !targetIndex || !augment) return;
        session.augmentCreature(*targetIndex, *targetLane, *augment);
    },

    // 1: Destroy creature
    [](MatchSession& session, int, std::optional<int> targetLane,
       std::optional<int> targetIndex, std::optional<int>, 
       std::optional<std::pair<int,int>>) {
        if (!targetLane || !targetIndex) return;
        // session.destroyCreature(*targetIndex, *targetLane);
    },

    // 2: Splash augment (augment all lanes for one side)
    [](MatchSession& session, int, std::optional<int>, 
       std::optional<int> targetIndex, std::optional<int>, 
       std::optional<std::pair<int,int>> augment) {
        if (!targetIndex || !augment) return;
        for (int i = 0; i < session.getLaneCount(); i++) {
            session.augmentCreature(*targetIndex, i, *augment);
        }
    },

    // 3: Augment player health
    [](MatchSession& session, int playerIndex, std::optional<int>,
       std::optional<int>, std::optional<int> amount, 
       std::optional<std::pair<int,int>>) {
        if (!amount) return;
        // session.augmentHP(playerIndex, *amount);
    },

    [](MatchSession& session, int, std::optional<int> targetLane,
       std::optional<int> targetIndex, std::optional<int> amount,
       std::optional<std::pair<int,int>> augment) {
        if (!targetLane || !targetIndex || !augment) return;
        session.setCreature(*targetIndex, *targetLane, *augment);
    },
};

EffectFunc Effects::getEffectById(int effectId) { //returns EffectFunc, but you still have to call it
    if (effectId < 0 || effectId >= static_cast<int>(effects.size()))
        return {}; // empty function if invalid
    return effects[effectId];
}

const std::unordered_map<int, std::vector<CardEffectEntry>> Effects::cardToEffectsMap = {
    {1, { CardEffectEntry{1, std::nullopt, std::make_pair(0,-3)} }},
    {20, { CardEffectEntry{1, std::nullopt, std::make_pair(0,3)} }},

    {23, { CardEffectEntry{1, std::nullopt, std::make_pair(2,1)}, 
            CardEffectEntry{4, 3} }}
};
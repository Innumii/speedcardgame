#include "game/SpellEffects.hpp"
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
        session.destroyCreature(*targetIndex, *targetLane);
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
        session.augmentHP(playerIndex, *amount);
    },

    //4
    [](MatchSession& session, int, std::optional<int> targetLane,
       std::optional<int> targetIndex, std::optional<int> amount,
       std::optional<std::pair<int,int>> augment) {
        if (!targetLane || !targetIndex || !augment) return;
        session.setCreature(*targetIndex, *targetLane, *augment);
    },
    //5: Mana augment
    [](MatchSession& session, int playerIndex, std::optional<int>,
       std::optional<int>, std::optional<int> amount, 
       std::optional<std::pair<int,int>>) {
        if (!amount) return;
        session.augmentMana(playerIndex, *amount);
    },
    // 6: set player health
    [](MatchSession& session, int playerIndex, std::optional<int>,
       std::optional<int>, std::optional<int> amount, 
       std::optional<std::pair<int,int>>) {
        if (!amount) return;
        session.setHP(playerIndex, *amount);
    },
};

EffectFunc Effects::getEffectById(int effectId) { //returns EffectFunc, but you still have to call it
    if (effectId < 0 || effectId >= static_cast<int>(effects.size()))
        return {}; // empty function if invalid
    return effects[effectId];
}

const std::vector<CardEffectEntry>* Effects::getCardEffects(int cardId) {
    auto it = cardToEffectsMap.find(cardId);
    if (it == cardToEffectsMap.end()) return nullptr;
    return &it->second;
}

const std::unordered_map<int, std::vector<CardEffectEntry>> Effects::cardToEffectsMap = {
    {1, {CardEffectEntry{1, std::nullopt, std::make_pair(0,-3)} }},
    {4, { CardEffectEntry{1, std::nullopt, std::make_pair(2,2)} }}, //add conditional augment
    {10, { CardEffectEntry{4, std::nullopt, std::make_pair(0,0)}}},
    {11, { CardEffectEntry{2, std::nullopt, std::make_pair(0,-1)}}},
    {13, { CardEffectEntry{1, std::nullopt, std::make_pair(1,1)}}}, //conditonal +5/-1
    {14, { CardEffectEntry{1, std::nullopt, std::make_pair(2,-1)}}},
    {15, { }}, //Double Strike
    {17, { CardEffectEntry{1, std::nullopt, std::make_pair(-1,-1)}}},
    {18, {}}, //Trample
    {20, { CardEffectEntry{1, std::nullopt, std::make_pair(0,3)} }},
    {23, { CardEffectEntry{3, -5, std::nullopt}, 
           CardEffectEntry{5, 5, std::nullopt} }},
    {25, {}}, // Conditional splash HP augment
    {26, {}}, // Conditional splash creature augment
    {29, { CardEffectEntry{6, 10, std::nullopt}}} // Deal difference as damage
};
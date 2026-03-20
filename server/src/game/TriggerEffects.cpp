#include "game/TriggerEffects.hpp"
#include "game/MatchSession.hpp"
#include <iostream>
#include "game/CombatEffects.hpp"

//Logic
// Sequential list of effects (index = effectId)
const std::vector<EffectFunc> TriggerEffects::effects = {

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
       std::optional<int> targetIndex, std::optional<int> amount, 
       std::optional<std::pair<int,int>>) {
        if (!amount) return;
        int index = playerIndex;
        if (targetIndex.has_value() && *targetIndex != -1) index = *targetIndex;
        session.augmentHP(index, *amount);
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
       std::optional<int> targetIndex, std::optional<int> amount, 
       std::optional<std::pair<int,int>>) {
        if (!amount) return;
        int index = playerIndex;
        if (targetIndex.has_value() && *targetIndex != -1) index = *targetIndex;
        session.augmentMana(index, *amount);
    },
    // 6: set player health
    [](MatchSession& session, int playerIndex, std::optional<int>,
       std::optional<int>, std::optional<int> amount, 
       std::optional<std::pair<int,int>>) {
        if (!amount) return;
        session.setHP(playerIndex, *amount);
    },

    // 7: Add evergreen combat effect bit to target creature
    [](MatchSession& session, int, std::optional<int> targetLane,
       std::optional<int> targetIndex, std::optional<int> amount,
       std::optional<std::pair<int,int>>) {
        if (!targetLane || !targetIndex || !amount) return;
        session.addCreatureEffect(*targetIndex, *targetLane, *amount);
    },

    // 8: Remove evergreen combat effect bit from target creature
    [](MatchSession& session, int, std::optional<int> targetLane,
       std::optional<int> targetIndex, std::optional<int> amount,
       std::optional<std::pair<int,int>>) {
        if (!targetLane || !targetIndex || !amount) return;
        session.removeCreatureEffect(*targetIndex, *targetLane, *amount);
    },

    // 9: Set regen value on target creature (triggers every 2 combats)
    [](MatchSession& session, int, std::optional<int> targetLane,
       std::optional<int> targetIndex, std::optional<int> amount,
       std::optional<std::pair<int,int>>) {
        if (!targetLane || !targetIndex || !amount) return;
        session.setCreatureRegen(*targetIndex, *targetLane, *amount);
    },

    // 10: augment HP stack from board count 
    [](MatchSession& session, int playerIndex, std::optional<int>,
       std::optional<int> targetIndex, std::optional<int> amount, 
       std::optional<std::pair<int,int>>) {
        if (!amount)  {
            // std::cout << "[DEBUG] NO AMOUNT\n";
            return;}
        int index = playerIndex;
        if (targetIndex.has_value() && *targetIndex != -1) index = *targetIndex; 
        int creaturesOwned = session.getCreaturesOwned(index);
        if (creaturesOwned <= 0) {
            // std::cout << "[DEBUG] NO CREATURES\n";
            return;
        }
        int totalAmount = creaturesOwned * (*amount);
        session.augmentHP(index, totalAmount);
    },
};

EffectFunc TriggerEffects::getEffectById(int effectId) { //returns EffectFunc, but you still have to call it
    if (effectId < 0 || effectId >= static_cast<int>(effects.size()))
        return {}; // empty function if invalid
    return effects[effectId];
}

const std::vector<CardEffectEntry>* TriggerEffects::getCardEffects(int cardId) {
    auto it = cardToEffectsMap.find(cardId);
    if (it == cardToEffectsMap.end()) return nullptr;
    return &it->second;
}

const std::unordered_map<int, std::vector<CardEffectEntry>> TriggerEffects::cardToEffectsMap = {

    //Fireball
    {1, {CardEffectEntry{0, std::nullopt, std::make_pair(0,-3)} }}, //done

    //Jo
    {4, { CardEffectEntry{0, std::nullopt, std::make_pair(4,4), Target::Nil, //done
        [](const MatchSession& session, int cardId, int, int) {
        return session.getCard(cardId)->getName() == "Go";}},
        CardEffectEntry{0, std::nullopt, std::make_pair(2,2), Target::Nil,
        [](const MatchSession& session, int cardId, int, int) {
        return session.getCard(cardId)->getName() != "Go";}} 
        }
    }, 

    //Flashbang
    {10, { CardEffectEntry{4, std::nullopt, std::make_pair(0,1)}}}, //done 

    //Fan The Hammer
    {11, { CardEffectEntry{2, std::nullopt, std::make_pair(0,-1), Target::Opponent}}}, //done

    //With This Sacred Treasure
    {13, { CardEffectEntry{0, std::nullopt, std::make_pair(5,-1), Target::Nil, //done
            [](const MatchSession& session, int cardId, int, int) {
            return session.getCard(cardId)->getName() == "Potential Man";}},
            CardEffectEntry{0, std::nullopt, std::make_pair(1,1), Target::Nil,
            [](const MatchSession& session, int cardId, int, int) {
            return session.getCard(cardId)->getName() != "Potential Man";}}
            }
    }, //conditonal +5/-1

    //Black Flash
    {14, { CardEffectEntry{0, std::nullopt, std::make_pair(2,-1)}}}, //done

    //Do it Again
    {15, { CardEffectEntry{7, CombatEffects::kDoubleStrike, std::nullopt} }}, // Give Double Strike

    //Knockout
    {17, { CardEffectEntry{0, std::nullopt, std::make_pair(-1,-1)}}}, //done

    //The Pass
    {18, { CardEffectEntry{7, CombatEffects::kTrample, std::nullopt} }}, // Give Trample

    //Mr President Get Down
    {20, { CardEffectEntry{0, std::nullopt, std::make_pair(0,3)} }}, //done

    //Grass Aint Green
    {22, { CardEffectEntry{10, -1, std::nullopt, Target::Opponent}}}, // NOT WORKING YET

    //Equivalent Exchange
    {23, { CardEffectEntry{3, -5, std::nullopt}, //done
           CardEffectEntry{5, 5, std::nullopt} }}, 

    //Holy Blessing
    {25, { CardEffectEntry{10, 2, std::nullopt, Target::Self}}},

    //Aura
    {26, {CardEffectEntry{2, std::nullopt, std::make_pair(2,2)}}}, //done

    //Final Gambit
    {29, { CardEffectEntry{6, 10, std::nullopt, Target::Self}}}, // NOT DONE

    //Cheese Touch
    {32, { CardEffectEntry{7, CombatEffects::kDeathTouch, std::nullopt} }}
};
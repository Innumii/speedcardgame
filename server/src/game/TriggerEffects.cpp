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

    // 2: Splash creature augment (augment all lanes for one side)
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

    //4: Set Creature Power/Toughness
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

    // 11: Final Gambit Unique Effect
    [](MatchSession& session, int playerIndex, std::optional<int>,
    std::optional<int> targetIndex, std::optional<int> amount,
    std::optional<std::pair<int,int>> augment) {

        if (!amount) return;
        int self = playerIndex;
        if (targetIndex.has_value() && *targetIndex != -1) {
            self = *targetIndex;
        }
        int opponent = 1 - self;
        int targetHP = *amount;
        int delta = session.getPlayerHealth(self) - targetHP;
        session.setHP(self, targetHP);
        if (delta != 0) {
            session.augmentHP(opponent, -delta);
        }
    },

    //12: Draw power
    [](MatchSession& session, int playerIndex, std::optional<int>,
       std::optional<int>, std::optional<int> amount,
       std::optional<std::pair<int,int>>) {
        if ( !amount) return;
        for (int i = 0; i < *amount; i++) session.draw(playerIndex);
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

//---------------------------------------------------------- SPELLS ----------------------------------------------------------

    //Fireball
    {1, { CardEffectEntry{
        .effectId = 0,
        .augment  = std::make_pair(0,-3)
    }}},

    //Jo
    {4, { CardEffectEntry{
            .effectId  = 0,
            .augment   = std::make_pair(4,4),
            .target    = Target::Nil,
            .condition = [](const MatchSession& session, int cardId, int, int) {
                return session.getCard(cardId)->getName() == "Go";
            }
          },
          CardEffectEntry{
            .effectId  = 0,
            .augment   = std::make_pair(2,2),
            .target    = Target::Nil,
            .condition = [](const MatchSession& session, int cardId, int, int) {
                return session.getCard(cardId)->getName() != "Go";
            }
          }
    }},

    //Flashbang
    {10, { CardEffectEntry{
        .effectId = 4,
        .augment  = std::make_pair(0,1)
    }}},

    //Fan The Hammer
    {11, { CardEffectEntry{
        .effectId = 2,
        .augment  = std::make_pair(0,-1),
        .target   = Target::Opponent
    }}},

    //With This Sacred Treasure
    {13, { CardEffectEntry{
            .effectId  = 0,
            .augment   = std::make_pair(5,-1),
            .target    = Target::Nil,
            .condition = [](const MatchSession& session, int cardId, int, int) {
                return session.getCard(cardId)->getName() == "Potential Man";
            }
          },
          CardEffectEntry{
            .effectId  = 0,
            .augment   = std::make_pair(1,1),
            .target    = Target::Nil,
            .condition = [](const MatchSession& session, int cardId, int, int) {
                return session.getCard(cardId)->getName() != "Potential Man";
            }
          }
    }},

    //Black Flash
    {14, { CardEffectEntry{
        .effectId = 0,
        .augment  = std::make_pair(2,-1)
    }}},

    //Do it Again
    {15, { CardEffectEntry{
        .effectId = 7,
        .amount   = CombatEffects::kDoubleStrike
    }}},

    //Knockout
    {17, { CardEffectEntry{
        .effectId = 0,
        .augment  = std::make_pair(-1,-1)
    }}},

    //The Pass
    {18, { CardEffectEntry{
        .effectId = 7,
        .amount   = CombatEffects::kTrample
    }}},

    //Mr President Get Down
    {20, { CardEffectEntry{
        .effectId = 0,
        .augment  = std::make_pair(0,3)
    }}},

    //Grass Aint Green
    {22, { CardEffectEntry{
        .effectId = 10,
        .amount   = -1,
        .target   = Target::Opponent
    }}},

    //Equivalent Exchange
    {23, { CardEffectEntry{
            .effectId = 3,
            .amount   = -5
          },
          CardEffectEntry{
            .effectId = 5,
            .amount   = 5
          }
    }},

    //Holy Blessing
    {25, { CardEffectEntry{
        .effectId = 10,
        .amount   = 2,
        .target   = Target::Self
    }}},

    //Aura
    {26, { CardEffectEntry{
        .effectId = 2,
        .augment  = std::make_pair(2,2)
    }}},

    //Final Gambit
    {29, { CardEffectEntry{
        .effectId = 11,
        .amount   = 10,
        .target   = Target::Self
    }}},

    //Cheese Touch
    {32, { CardEffectEntry{
        .effectId = 7,
        .amount   = CombatEffects::kDeathTouch
    }}},

    //Pot of Greed
    {36, { CardEffectEntry{
        .effectId = 12,
        .amount   = 2
    }}},

    //Devil's Lettuce
    {37, { CardEffectEntry{
            .effectId = 9,
            .amount   = 3
          }
        }},

    //Mossad Mayhem
    {38, { CardEffectEntry{
            .effectId = 3,
            .amount   = -10,
            .target = Target::Self
          },
           CardEffectEntry{
            .effectId = 2,
            .augment  = std::make_pair(0,-5),
            .target = Target::Opponent
          },
        }},

//---------------------------------------------------------- CREATURES ----------------------------------------------------------

    //Kirby
    {5, { CardEffectEntry{
        .effectId = 0,
        .augment  = std::make_pair(1,1),
        .trigger  = TriggerType::OnKill
    }}},

    //Jew
    {7, { CardEffectEntry{
            .effectId = 5,
            .amount   = -2,
            .target   = Target::Opponent
          },
          CardEffectEntry{
            .effectId = 5,
            .amount   = 2,
            .target   = Target::Self
          }
    }},

    //Bushcamper
    {24, { CardEffectEntry{
        .effectId = 3,
        .amount   = -2,
        .target   = Target::Opponent
    }}},

    //Gun
    {27, { CardEffectEntry{
        .effectId = 3,
        .amount   = -1,
        .target   = Target::Opponent,
        .trigger  = TriggerType::OnKill
    }}},

    //Ay Lmao
    {28, { CardEffectEntry{
        .effectId = 3,
        .amount   = -5,
        .target   = Target::Self
    }}},

};
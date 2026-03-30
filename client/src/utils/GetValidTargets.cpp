#include "utils/GetValidTargets.hpp"
#include "utils/StringUtil.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>

/*
    Get all possible targets a spell can have

    Options:
        1. hand attack (opponent cards in hand)
        2. creatures on the board (both sides)
            - opponent creatures only
            - self creatures only
            - all creatures on the board
        3. players directly (if spell allows)

    returns a vector of integers representing valid target indices:
        - For hand targets: 0 to opponent's hand size - 1
        - For board targets: 
            - 100 + lane index for local player's creatures
            - 200 + lane index for opponent's creatures
        - For player targets: -1 for local player, -2 for opponent player
*/
std::optional<std::vector<int>> getValidTargets(const Playing& playing, const Card& card, int sourceLane) {
    (void)sourceLane;

    std::vector<int> validTargets;

    std::string cardText = StringUtil::toLower(card.getText());

    constexpr int localBoardIndex = 0;
    constexpr int opponentBoardIndex = 1;

    // Determine targeting options based on card text
    /* -1: target all opponent cards
       -2: target all local cards
       -3: target all cards on board
    */
    bool isAllOpponent = cardText.find("all opponent") != std::string::npos; //let server resolve
    bool isAllSelf = cardText.find("all your") != std::string::npos; //let server resolve
    bool isAll = cardText.find("all") != std::string::npos; //let server resolve

    bool canTargetAll = cardText.find("any target") != std::string::npos;
    bool canTargetPlayers = cardText.find("target player") != std::string::npos;
    bool canTargetCreatures = cardText.find("target creature") != std::string::npos;
    bool canTargetOpponentCreatures = cardText.find("target creature an opponent controls") != std::string::npos;
    bool canTargetSelfCreatures = cardText.find("target creature you control") != std::string::npos;
    bool canTargetHand = cardText.find("target card in opponent's hand") != std::string::npos;
    bool hasTargeting = canTargetAll || canTargetCreatures || canTargetHand || canTargetOpponentCreatures || canTargetPlayers || canTargetSelfCreatures;

    if (isAllOpponent) {
        validTargets.push_back(903);
        return validTargets;
    } else if (isAllSelf) {
        validTargets.push_back(902);
        return validTargets;
    } else if (isAll) {
        validTargets.push_back(901);
        return validTargets;    
    }

    if (!hasTargeting) {
        return std::nullopt;
    }
    
    // Check hand targets
    if (canTargetHand) {
        int opponentHandSize = static_cast<int>(playing.remotePlayer.hand.size());
        for (int i = 0; i < opponentHandSize; ++i) {
            validTargets.push_back(i); // hand targets: 0 to opponent hand size - 1
        }
    }

    // "any target" means any target on the board or players, but not hand cards
    if (canTargetAll) {
        canTargetPlayers = true;
        canTargetCreatures = true;
        canTargetOpponentCreatures = true;
        canTargetSelfCreatures = true;
    }

    // Check board targets
    if (canTargetCreatures || canTargetOpponentCreatures || canTargetSelfCreatures) {
        int laneCount = playing.board.getLaneCount();
        for (int lane = 0; lane < laneCount; ++lane) {
            const auto& localCardOpt = playing.board.getZone(lane, localBoardIndex);
            const auto& opponentCardOpt = playing.board.getZone(lane, opponentBoardIndex);

            if ((canTargetCreatures || canTargetSelfCreatures) && localCardOpt && *localCardOpt) {
                validTargets.push_back(100 + lane); // self creature targets: 100 + lane index
            }
            if ((canTargetCreatures || canTargetOpponentCreatures) && opponentCardOpt && *opponentCardOpt) {
                validTargets.push_back(200 + lane); // opponent creature targets: 200 + lane index
            }
        }
    }

    // Check player targets
    if (canTargetPlayers) {
        validTargets.push_back(-1); // local player target
        validTargets.push_back(-2); // opponent player target
    }

    return validTargets;
}
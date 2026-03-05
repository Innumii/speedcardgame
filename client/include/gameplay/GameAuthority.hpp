#pragma once
#include <optional>
//base class
class GameAuthority {
public:
    //virtual means can be overridden
    virtual ~GameAuthority() = default;

    virtual void playCard(int cardId, int lane, std::optional<int> targetId, std::optional<int> targetOpponent) = 0;
    virtual void discardCard(int cardId) = 0;
};

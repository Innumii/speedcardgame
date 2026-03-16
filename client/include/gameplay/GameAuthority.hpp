#pragma once
#include <optional>
//base class
class GameAuthority {
public:
    //virtual means can be overridden
    virtual ~GameAuthority() = default;

    virtual void playCard(int cardId, int lane, std::optional<int> targetLane, std::optional<int> targetIndex) = 0;
    virtual void discardCard(int cardId) = 0;
};

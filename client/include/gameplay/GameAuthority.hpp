#pragma once

//base class
class GameAuthority {
public:
    //virtual means can be overridden
    virtual ~GameAuthority() = default;

    virtual void playCard(int handIndex, int lane, std::optional<int> targetId) = 0;
    virtual void discardCard(int handIndex) = 0;
};

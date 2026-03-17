#pragma once

#include "gameplay/GameAuthority.hpp"
#include <optional>

class NetworkClient;

class LocalAuthority : public GameAuthority {
public:
    explicit LocalAuthority(NetworkClient* net);

    ~LocalAuthority() override = default;

    // Play card from hand
    void playCard(int cardId, int lane, std::optional<int> targetLane, std::optional<int> targetIndex) override;

    // Discard card from hand
    void discardCard(int cardId) override;
    void surrender() override;


private:
    NetworkClient* network; // non-owning pointer
};

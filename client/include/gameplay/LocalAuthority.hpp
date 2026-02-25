#pragma once

#include "gameplay/GameAuthority.hpp"
#include <optional>

class NetworkClient;

class LocalAuthority : public GameAuthority {
public:
    explicit LocalAuthority(NetworkClient* net);

    ~LocalAuthority() override = default;

    // Play card from hand
    void playCard(int handIndex, int lane, std::optional<int> targetId) override;

    // Discard card from hand
    void discardCard(int handIndex) override;

private:
    NetworkClient* network; // non-owning pointer
};

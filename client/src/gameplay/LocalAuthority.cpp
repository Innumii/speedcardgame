//This class is used for handling sending of user game actions towards the server.

#include "gameplay/LocalAuthority.hpp"
#include "core/NetworkClient.hpp"

LocalAuthority::LocalAuthority(NetworkClient* net)
    : network(net)
{
}

void LocalAuthority::playCard(int handIndex, int lane, std::optional<int> targetId) {
    if (!network) return;

    // Single-phase: send everything at once
    network->sendPlayCard(handIndex, lane, targetId);
}

void LocalAuthority::discardCard(int handIndex) {
    if (!network) return;

    network->sendDiscardCard(handIndex);
}

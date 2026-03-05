//This class is used for handling sending of user game actions towards the server.

#include "gameplay/LocalAuthority.hpp"
#include "core/NetworkClient.hpp"

LocalAuthority::LocalAuthority(NetworkClient* net)
    : network(net)
{
}

void LocalAuthority::playCard(int cardId, int lane, std::optional<int> targetId, std::optional<int> targetOpponent) {
    if (!network) return;

    // Single-phase: send everything at once
    network->sendPlayCard(cardId, lane, targetId, targetOpponent);
}

void LocalAuthority::discardCard(int cardId) {
    if (!network) return;

    network->sendDiscardCard(cardId);
}

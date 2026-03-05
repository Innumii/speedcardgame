#include "objects/Inventory.hpp"

#include "core/Game.hpp"
#include "objects/Deck.h"
#include "utils/EnvUtil.hpp"
#include "utils/HttpUtil.hpp"
#include "utils/JsonUtil.hpp"

#include <algorithm>
#include <string>
#include <unordered_map>

namespace {
    bool extractCardsObjectForUser(const std::string& json, int userId, std::string& outCards) {
        std::size_t pos = 0;
        while (pos < json.size()) {
            if (json[pos] != '{') {
                ++pos;
                continue;
            }

            std::size_t objEnd = 0;
            if (!JsonUtil::findMatchingBrace(json, pos, objEnd)) {
                return false;
            }

            const std::string obj = json.substr(pos, objEnd - pos + 1);
            int uid = -1;
            JsonUtil::readJsonIntField(obj, "uid", uid);
            if (uid == userId) {
                const std::string needle = "\"cards\"";
                std::size_t cardsPos = obj.find(needle);
                if (cardsPos == std::string::npos) return false;
                cardsPos = obj.find('{', cardsPos + needle.size());
                if (cardsPos == std::string::npos) return false;
                std::size_t cardsEnd = 0;
                if (!JsonUtil::findMatchingBrace(obj, cardsPos, cardsEnd)) return false;
                if (cardsEnd <= cardsPos + 1) {
                    outCards.clear();
                    return true;
                }
                outCards = obj.substr(cardsPos + 1, cardsEnd - cardsPos - 1);
                return true;
            }

            pos = objEnd + 1;
        }

        return false;
    }
}

Inventory::Inventory(int cardCount) : copiesByCardIndex(cardCount, 0) {}

int Inventory::getCardCount(int cardIndex) const {
    return Inventory::getCardCount(copiesByCardIndex, cardIndex);
}

void Inventory::setCardCount(int cardIndex, int count) {
    if (cardIndex < 0 || cardIndex >= static_cast<int>(copiesByCardIndex.size())) {
        return;
    }
    copiesByCardIndex[cardIndex] = count;
}

int Inventory::getCardCount(const std::vector<int>& copiesByCardIndex, int cardIndex) {
    if (cardIndex < 0 || cardIndex >= static_cast<int>(copiesByCardIndex.size())) {
        return 0;
    }
    return copiesByCardIndex[cardIndex];
}

int Inventory::getRemainingCount(
    const std::vector<int>& inventoryCopies,
    bool inventoryLoaded,
    const std::vector<int>& deckCopies,
    int cardIndex,
    int maxDeckCopies
) {
    if (cardIndex < 0 || cardIndex >= static_cast<int>(deckCopies.size())) return 0;

    if (!inventoryLoaded || inventoryCopies.size() != deckCopies.size()) {
        return maxDeckCopies - deckCopies[cardIndex];
    }

    const int remaining = getCardCount(inventoryCopies, cardIndex) - deckCopies[cardIndex];
    return remaining > 0 ? remaining : 0;
}

bool Inventory::loadInventoryCopiesFromService(
    const Game& game,
    const std::vector<std::unique_ptr<Card>>& availableCards,
    std::vector<int>& outInventoryCopies,
    int maxCopies
) {
    if (availableCards.empty()) return false;

    const std::string host = EnvUtil::getServiceHost("CARDS_SERVICE", "127.0.0.1", "api.myapp.com");
    const int port = EnvUtil::getServicePort("CARDS_SERVICE", 8082, 443);
    const std::string path = "/cards/inventories";
    const int userId = EnvUtil::getEnvIntOrDefault("CARDS_SERVICE_UID", game.getPlayerId());

    int statusCode = -1;
    std::string responseBody;
    if (!HttpUtil::sendHttp(host, port, "GET", path, "", statusCode, responseBody)) {
        outInventoryCopies.assign(availableCards.size(), 0);
        return false;
    }

    std::string cardsJson;
    if (!extractCardsObjectForUser(responseBody, userId, cardsJson)) {
        outInventoryCopies.assign(availableCards.size(), 0);
        return false;
    }

    std::vector<std::pair<int, int>> cardCounts;
    if (!Deck::parseCardCounts(cardsJson, cardCounts)) {
        outInventoryCopies.assign(availableCards.size(), 0);
        return false;
    }

    std::unordered_map<int, std::size_t> cardIndexById;
    cardIndexById.reserve(availableCards.size());
    for (std::size_t i = 0; i < availableCards.size(); ++i) {
        cardIndexById.emplace(availableCards[i]->getId(), i);
    }

    outInventoryCopies.assign(availableCards.size(), 0);
    for (const auto& pair : cardCounts) {
        const int cardId = pair.first;
        const int copies = pair.second;
        if (copies <= 0) continue;

        auto it = cardIndexById.find(cardId);
        if (it == cardIndexById.end()) continue;
        outInventoryCopies[it->second] = std::min(copies, maxCopies);
    }

    return true;
}

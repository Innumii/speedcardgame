#include "objects/Inventory.hpp"

#include "core/Game.hpp"
#include "objects/Deck.h"
#include "utils/LoadAvailableCards.hpp"
#include "utils/EnvUtil.hpp"
#include "utils/HttpUtil.hpp"
#include "utils/JsonUtil.hpp"

#include <algorithm>
#include <sstream>
#include <string>
#include <unordered_map>

namespace {
    std::vector<int>& cachedInventoryCopiesStorage() {
        static std::vector<int> copies;
        return copies;
    }

    int& cachedInventoryCoinsStorage() {
        static int coins = 0;
        return coins;
    }

    bool& cachedInventoryLoadedFlag() {
        static bool loaded = false;
        return loaded;
    }

    bool loadInventoryAndCoinsFromServiceImpl(
        const Game& game,
        const std::vector<std::unique_ptr<Card>>& availableCards,
        std::vector<int>& outInventoryCopies,
        int& outCoins,
        int maxCopies
    ) {
        if (availableCards.empty()) return false;

        const std::string host = EnvUtil::getCardsServiceHost();
        const int port = EnvUtil::getCardsServicePort();
        const int userId = game.getPlayerId();
        if (userId <= 0) {
            outInventoryCopies.assign(availableCards.size(), 0);
            return false;
        }
        const std::string path = "/cards/inventories/" + std::to_string(userId);

        int statusCode = -1;
        std::string responseBody;
        if (!HttpUtil::sendHttp(host, port, "GET", path, "", statusCode, responseBody)) {
            outInventoryCopies.assign(availableCards.size(), 0);
            return false;
        }
        if (statusCode < 200 || statusCode >= 300) {
            outInventoryCopies.assign(availableCards.size(), 0);
            return false;
        }

        if (!JsonUtil::readJsonIntField(responseBody, "coins", outCoins)) {
            outInventoryCopies.assign(availableCards.size(), 0);
            return false;
        }

        std::string cardsObject;
        if (!JsonUtil::extractJsonObject(responseBody, "cards", cardsObject)) {
            outInventoryCopies.assign(availableCards.size(), 0);
            return false;
        }

        std::string cardsJson;
        if (cardsObject.size() >= 2 && cardsObject.front() == '{' && cardsObject.back() == '}') {
            cardsJson = cardsObject.substr(1, cardsObject.size() - 2);
        } else {
            cardsJson = cardsObject;
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

bool Inventory::loadInventoryAndCoinsFromService(const Game& game) {
    const auto& availableCards = LoadAvailableCardsUtil::getAvailableCards();
    std::vector<int> loadedCopies;
    int loadedCoins = 0;

    const bool loaded = loadInventoryAndCoinsFromServiceImpl(
        game,
        availableCards,
        loadedCopies,
        loadedCoins,
        Deck::getDeckCopiesLimit()
    );

    if (!loaded) {
        cachedInventoryCopiesStorage().assign(availableCards.size(), 0);
        cachedInventoryCoinsStorage() = 0;
        cachedInventoryLoadedFlag() = false;
        return false;
    }

    cachedInventoryCopiesStorage() = std::move(loadedCopies);
    cachedInventoryCoinsStorage() = loadedCoins;
    cachedInventoryLoadedFlag() = true;
    return true;
}

bool Inventory::ensureInventoryAndCoinsLoaded(
    const Game& game,
    const std::vector<std::unique_ptr<Card>>& availableCards,
    int maxCopies
) {
    if (cachedInventoryLoadedFlag() && cachedInventoryCopiesStorage().size() == availableCards.size()) {
        return true;
    }

    std::vector<int> loadedCopies;
    int loadedCoins = 0;
    const bool loaded = loadInventoryAndCoinsFromServiceImpl(
        game,
        availableCards,
        loadedCopies,
        loadedCoins,
        maxCopies
    );

    if (!loaded) {
        cachedInventoryCopiesStorage().assign(availableCards.size(), 0);
        cachedInventoryCoinsStorage() = 0;
        cachedInventoryLoadedFlag() = false;
        return false;
    }

    cachedInventoryCopiesStorage() = std::move(loadedCopies);
    cachedInventoryCoinsStorage() = loadedCoins;
    cachedInventoryLoadedFlag() = true;
    return true;
}

bool Inventory::isInventoryCacheLoaded() {
    return cachedInventoryLoadedFlag();
}

const std::vector<int>& Inventory::getCachedInventoryCopies() {
    return cachedInventoryCopiesStorage();
}

std::vector<int>& Inventory::getCachedInventoryCopiesMutable() {
    return cachedInventoryCopiesStorage();
}

int Inventory::getCachedCoins() {
    return cachedInventoryCoinsStorage();
}

void Inventory::setCachedCoins(int coins) {
    cachedInventoryCoinsStorage() = coins > 0 ? coins : 0;
}

bool Inventory::updateCoinsOnService(const Game& game, int coins) {
    const std::string host = EnvUtil::getCardsServiceHost();
    const int port = EnvUtil::getCardsServicePort();
    const std::string path = "/cards/inventories/coins";
    const int userId = game.getPlayerId();
    if (userId <= 0) {
        return false;
    }

    std::ostringstream payload;
    payload << "{\"uid\":" << userId << ",\"coins\":" << coins << "}";

    int statusCode = -1;
    std::string responseBody;
    if (!HttpUtil::sendHttp(host, port, "PUT", path, payload.str(), statusCode, responseBody)) {
        return false;
    }

    return statusCode >= 200 && statusCode < 300;
}
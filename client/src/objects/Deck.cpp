#include "objects/Deck.h"
#include "core/Game.hpp"
#include "objects/CreatureCard.h"
#include "objects/SpellCard.h"
#include "utils/EnvUtil.hpp"
#include "utils/HttpUtil.hpp"
#include "utils/JsonUtil.hpp"

#include <algorithm>
#include <cctype>
#include <random>
#include <iostream>
#include <numeric>
#include <sstream>
#include <unordered_map>
#include <utils/SessionUtil.hpp>

void Deck::addCard(std::unique_ptr<Card> card) {
    cards.push_back(std::move(card));
}

std::unique_ptr<Card> Deck::draw() {
    auto card = std::move(cards.back());
    cards.pop_back();
    return card;
}


void Deck::shuffle() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::shuffle(cards.begin(), cards.end(), gen);
}

bool Deck::isEmpty() const {
    return cards.empty();
}

int Deck::size() const {
    return cards.size(); 
}

std::unique_ptr<Card> Deck::takeCardById(int cardId) {
    for (auto it = cards.begin(); it != cards.end(); ++it) {
        if ((*it)->getId() == cardId) {
            std::unique_ptr<Card> card = std::move(*it);
            cards.erase(it);
            return card;
        }
    }

    return nullptr;
}

void Deck::toString() const {
    std::cout << "Deck contains " << cards.size() << " cards:\n";
    for (const auto& c : cards) {
        if (c) std::cout << "  Card id: " << c->getId() << "\n";
    }
}

bool Deck::loadDeckCopiesFromService(
    const Game& game,
    const std::vector<std::unique_ptr<Card>>& availableCards,
    std::vector<int>& outDeckCopies,
    int maxCopies
) {
    if (availableCards.empty()) return false;

    const std::string host = EnvUtil::getCardsServiceHost();
    const int port = EnvUtil::getCardsServicePort();
    const std::string path = "/cards/decks";
    const int userId = game.getPlayerId();

    int statusCode = -1;
    std::string responseBody;
    if (!HttpUtil::sendHttp(host, port, "GET", path, "", statusCode, responseBody, SessionUtil::get())) {
        outDeckCopies.assign(availableCards.size(), 0);
        return false;
    }

    std::string cardsJson;
    if (!JsonUtil::extractCardsObjectForUser(responseBody, userId, cardsJson)) {
        outDeckCopies.assign(availableCards.size(), 0);
        return false;
    }

    std::vector<std::pair<int, int>> cardCounts;
    if (!parseCardCounts(cardsJson, cardCounts)) {
        outDeckCopies.assign(availableCards.size(), 0);
        return false;
    }

    std::unordered_map<int, std::size_t> cardIndexById;
    cardIndexById.reserve(availableCards.size());
    for (std::size_t i = 0; i < availableCards.size(); ++i) {
        cardIndexById.emplace(availableCards[i]->getId(), i);
    }

    outDeckCopies.assign(availableCards.size(), 0);
    for (const auto& pair : cardCounts) {
        auto it = cardIndexById.find(pair.first);
        if (it != cardIndexById.end()) {
            outDeckCopies[it->second] = std::min(pair.second, maxCopies);
        }
    }

    return true;
}

int Deck::getDeckSizeLimit() {
    const int configured = EnvUtil::getEnvIntOrDefault("DECK_SIZE", 30);
    return configured > 0 ? configured : 30;
}

int Deck::getDeckCopiesLimit() {
    const int configured = EnvUtil::getEnvIntOrDefault("DECK_COPIES", 4);
    return configured > 0 ? configured : 4;
}

int Deck::getDeckCardCount(const std::vector<int>& deckCopies) {
    return std::accumulate(deckCopies.begin(), deckCopies.end(), 0);
}

bool Deck::hasFullDeck(const std::vector<int>& deckCopies) {
    return getDeckCardCount(deckCopies) >= getDeckSizeLimit();
}

bool Deck::addCopy(
    std::vector<int>& deckCopies,
    int cardIndex,
    int maxDeckCopies,
    int deckSizeLimit,
    int remainingCount
) {
    if (cardIndex < 0 || cardIndex >= static_cast<int>(deckCopies.size())) return false;
    if (deckCopies[cardIndex] >= maxDeckCopies) return false;
    if (getDeckCardCount(deckCopies) >= deckSizeLimit) return false;
    if (remainingCount <= 0) return false;

    deckCopies[cardIndex] += 1;
    return true;
}

bool Deck::removeCopy(std::vector<int>& deckCopies, int cardIndex) {
    if (cardIndex < 0 || cardIndex >= static_cast<int>(deckCopies.size())) return false;
    if (deckCopies[cardIndex] <= 0) return false;

    deckCopies[cardIndex] -= 1;
    return true;
}

bool Deck::saveDeckCopiesToService(
    const Game& game,
    const std::vector<std::unique_ptr<Card>>& availableCards,
    const std::vector<int>& deckCopies
) {
    if (!hasFullDeck(deckCopies)) {
        return false;
    }

    const std::string host = EnvUtil::getCardsServiceHost();
    const int port = EnvUtil::getCardsServicePort();
    const std::string path = "/cards/decks";
    const int userId = game.getPlayerId();

    std::ostringstream payload;
    payload << "{\"uid\":" << userId << ",\"cards\":{";
    bool first = true;
    const std::size_t count = std::min(availableCards.size(), deckCopies.size());
    for (std::size_t i = 0; i < count; ++i) {
        const int copies = deckCopies[i];
        if (copies <= 0) continue;

        const int cardId = availableCards[i] ? availableCards[i]->getId() : -1;
        if (cardId < 0) continue;

        if (!first) payload << ',';
        payload << "\"" << cardId << "\":" << copies;
        first = false;
    }
    payload << "}}";

    int statusCode = -1;
    std::string responseBody;
    return HttpUtil::sendHttp(host, port, "POST", path, payload.str(), statusCode, responseBody, SessionUtil::get());
}

bool Deck::parseCardCounts(const std::string& json, std::vector<std::pair<int, int>>& out) {
    out.clear();

    std::size_t pos = 0;
    while (pos < json.size()) {
        while (pos < json.size()) {
            const unsigned char c = static_cast<unsigned char>(json[pos]);
            if (std::isspace(c) || json[pos] == ',' || json[pos] == '{' || json[pos] == '}') {
                ++pos;
                continue;
            }
            break;
        }

        if (pos >= json.size()) {
            break;
        }

        std::string key;
        if (!JsonUtil::parseJsonQuotedStringAt(json, pos, key)) {
            int numericKey = -1;
            if (!JsonUtil::parseJsonIntAt(json, pos, numericKey)) {
                return false;
            }
            key = std::to_string(numericKey);
        }

        while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) {
            ++pos;
        }
        if (pos >= json.size() || json[pos] != ':') {
            return false;
        }
        ++pos;

        int count = 0;
        if (!JsonUtil::parseJsonIntAt(json, pos, count)) {
            return false;
        }

        try {
            const int cardId = std::stoi(key);
            out.emplace_back(cardId, count);
        } catch (...) {
            return false;
        }
    }

    return true;
}

Deck Deck::buildFromCopies(
    const std::vector<std::unique_ptr<Card>>& availableCards,
    const std::vector<int>& deckCopies
) {
    Deck deck;
    const std::size_t count = std::min(availableCards.size(), deckCopies.size());

    for (std::size_t i = 0; i < count; ++i) {
        int copies = deckCopies[i];
        if (copies <= 0) continue;

        const Card* base = availableCards[i].get();
        if (!base) continue;

        for (int copy = 0; copy < copies; ++copy) {
            deck.addCard(base->clone()); // Use clone() here
        }
    }

    return deck;
}

Deck Deck::clone() const {
    Deck newDeck;
    for (const auto& card : cards) {
        newDeck.addCard(card->clone());
    }
    return newDeck; // uses move semantics
}
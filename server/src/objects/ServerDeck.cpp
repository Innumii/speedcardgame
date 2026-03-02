#include "objects/ServerDeck.h"

void ServerDeck::addCard(int cardId) {
    cards.push_back(cardId);
}

void ServerDeck::clear() {
    cards.clear();
}

void ServerDeck::shuffle() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::shuffle(cards.begin(), cards.end(), gen);
}

std::optional<int> ServerDeck::draw() {
    if (cards.empty()) return std::nullopt;

    int top = cards.back();
    cards.pop_back();
    return top;
}

std::optional<int> ServerDeck::takeCardById(int cardId) {
    auto it = std::find(cards.begin(), cards.end(), cardId);
    if (it == cards.end()) return std::nullopt;

    int id = *it;
    cards.erase(it);
    return id;
}

bool ServerDeck::isEmpty() const {
    return cards.empty();
}

int ServerDeck::size() const {
    return static_cast<int>(cards.size());
}
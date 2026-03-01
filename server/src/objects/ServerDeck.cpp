#include "objects/ServerDeck.h"
#include <algorithm>
#include <random>

void ServerDeck::addCard(std::shared_ptr<ServerCard> card) {
    cards.push_back(std::move(card));
}

void ServerDeck::clear() {
    cards.clear();
}

void ServerDeck::shuffle() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::shuffle(cards.begin(), cards.end(), gen);
}

std::shared_ptr<ServerCard> ServerDeck::draw() {
    if (cards.empty()) return nullptr;
    auto top = cards.back();
    cards.pop_back();
    return top;
}

std::shared_ptr<ServerCard> ServerDeck::takeCardById(int cardId) {
    auto it = std::find_if(cards.begin(), cards.end(),
        [cardId](const std::shared_ptr<ServerCard>& c){ return c->getId() == cardId; });

    if (it == cards.end()) return nullptr;

    auto card = *it;
    cards.erase(it);
    return card;
}

bool ServerDeck::isEmpty() const {
    return cards.empty();
}

int ServerDeck::size() const {
    return static_cast<int>(cards.size());
}
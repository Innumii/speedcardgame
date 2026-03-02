#include "objects/Deck.h"
#include <algorithm>
#include <random>
#include <iostream>

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
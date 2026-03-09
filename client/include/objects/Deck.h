#ifndef DECK_H
#define DECK_H

#include "Card.h"
#include <memory>
#include <vector>
#include <optional>
#include <string>

class Game;

class Deck {
private:
    std::vector<std::unique_ptr<Card>> cards;
    int MaxDeckCopies = 4;
    int DeckSizeLimit = 30;

public:
    static int getDeckSizeLimit();
    static int getDeckCopiesLimit();

    Deck() = default;
    void addCard(std::unique_ptr<Card> card);
    void clear();

    //Server side
    void shuffle();                         // server only
    std::unique_ptr<Card> draw();          // draw top

    //Client side
    std::unique_ptr<Card> takeCardById(int cardId);

    bool isEmpty() const;
    int size() const;

    //Print cards in deck
    void toString() const;

    static int getDeckCardCount(const std::vector<int>& deckCopies);
    static bool hasFullDeck(const std::vector<int>& deckCopies);
    static bool parseCardCounts(const std::string& json, std::vector<std::pair<int, int>>& out);
    static bool addCopy(
        std::vector<int>& deckCopies,
        int cardIndex,
        int maxDeckCopies,
        int deckSizeLimit,
        int remainingCount
    );
    static bool removeCopy(std::vector<int>& deckCopies, int cardIndex);

    static bool loadDeckCopiesFromService(
        const Game& game,
        const std::vector<std::unique_ptr<Card>>& availableCards,
        std::vector<int>& outDeckCopies,
        int maxCopies
    );

    static bool saveDeckCopiesToService(
        const Game& game,
        const std::vector<std::unique_ptr<Card>>& availableCards,
        const std::vector<int>& deckCopies
    );

    static Deck buildFromCopies(
        const std::vector<std::unique_ptr<Card>>& availableCards,
        const std::vector<int>& deckCopies
    );


    //Deck resetting
    // std::vector<int> exportCardIds() const;
    // void importFromIds(const std::vector<int>& ids);
};

#endif
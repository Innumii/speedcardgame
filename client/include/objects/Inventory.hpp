#ifndef INVENTORY_HPP
#define INVENTORY_HPP

#include <memory>
#include <vector>

class Card;
class Game;

class Inventory {
	std::vector<int> copiesByCardIndex;
    

public:
    explicit Inventory(int cardCount = 0);
    int getCardCount(int cardIndex) const;
    void setCardCount(int cardIndex, int count);

    static int getCardCount(const std::vector<int>& copiesByCardIndex, int cardIndex);
    static int getRemainingCount(
        const std::vector<int>& inventoryCopies,
        bool inventoryLoaded,
        const std::vector<int>& deckCopies,
        int cardIndex,
        int maxDeckCopies
    );

    static bool loadInventoryAndCoinsFromService(const Game& game);

    static bool ensureInventoryAndCoinsLoaded(
        const Game& game,
        const std::vector<std::unique_ptr<Card>>& availableCards,
        int maxCopies
    );

    static bool isInventoryCacheLoaded();
    static const std::vector<int>& getCachedInventoryCopies();
    static std::vector<int>& getCachedInventoryCopiesMutable();
    static int getCachedCoins();
    static void setCachedCoins(int coins);

    static bool updateCoinsOnService(const Game& game, int coins);
};

#endif
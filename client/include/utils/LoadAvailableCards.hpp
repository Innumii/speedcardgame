#ifndef LOAD_AVAILABLE_CARDS_HPP
#define LOAD_AVAILABLE_CARDS_HPP

#include <memory>
#include <vector>

class Card;

namespace LoadAvailableCardsUtil {
    bool loadFromService(std::vector<std::unique_ptr<Card>>& outCards);
    bool loadFromCsv(std::vector<std::unique_ptr<Card>>& outCards);
    bool ensureAvailableCardsLoaded();
    bool areAvailableCardsLoaded();
    std::vector<std::unique_ptr<Card>>& getAvailableCards();
    int getNumberOfAvailableCards();
}

#endif
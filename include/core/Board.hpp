#ifndef BOARD_HPP
#define BOARD_HPP
#include <memory>
#include <optional>
#include <vector>
class Card;

class Board { //stores data on what cards are on the board
public:
    explicit Board(int laneCount = 5);
    bool isLaneEmpty(int lane) const;

    //transfers ownership of card to Board object
    bool placeCard(int lane, std::unique_ptr<Card> card);

    //return reference to Card object, if card is present on the lane
    const std::optional<std::unique_ptr<Card>>& getLane(int lane) const;
    int laneCount() const;

    //Discard Zone interaction (0 for local, 1 for remote)
    bool addToDiscard(std::unique_ptr<Card> card, int playerId);
    std::vector<std::unique_ptr<Card>>& getDiscard(int playerId);
    void displayDiscard(int playerId);


private:
    std::vector<std::optional<std::unique_ptr<Card>>> lanes;

    //Discard Zones
    std::vector<std::unique_ptr<Card>> discard[2];

};

#endif
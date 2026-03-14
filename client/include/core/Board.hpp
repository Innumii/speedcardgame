#ifndef BOARD_HPP
#define BOARD_HPP
#include <memory>
#include <optional>
#include <vector>
class Card;

class Board { //stores data on what cards are on the board
public:
    explicit Board(int count = 5);
    bool isZoneEmpty(int lane, int playerId) const;

    //transfers ownership of card to Board object
    bool addToPlay(int lane, int playerId, std::unique_ptr<Card> card);
    bool removeFromPlay(int lane, int playerId, std::unique_ptr<Card>& outCard);
    void displayPlay(int playerId);

    //return reference to Card object, if card is present on the zone
    const std::optional<std::unique_ptr<Card>>& getZone(int lane, int playerId) const;
    int getLaneCount() const;

    //Discard Zone interaction (id 0 for local, 1 for remote)
    bool addToDiscard(std::unique_ptr<Card> card, int playerId);
    std::vector<std::unique_ptr<Card>>& getDiscard(int playerId);
    void displayDiscard(int playerId);


private:
    int laneCount;
    std::vector<std::optional<std::unique_ptr<Card>>> lanes[2];

    //Discard Zones
    std::vector<std::unique_ptr<Card>> discard[2];

};

#endif
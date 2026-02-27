#ifndef PLAYER_H
#define PLAYER_H

#include <vector>
#include <cstddef>

class Player {
public:
    Player();

    int id;
    int health;
    int mana;
    int fatigueDamage;
    bool isOpponent;

    // Runtime card instance IDs (authoritative server assigns these)
    std::vector<int> handInstanceIds;
    int deckSize;

    // ---- State Queries ----
    bool handFull() const;
    std::size_t handSize() const;
    bool isDead() const;

    void setIsOpponent(bool opponentStatus);
    void setHand(const std::vector<int>& newHand);
    void addCardToHand(int instanceId);
    void removeCardFromHand(std::size_t index);
    void setDeckSize(int size);

private:
    static constexpr std::size_t MAX_HAND_SIZE = 7;
};

#endif
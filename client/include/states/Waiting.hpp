#ifndef WAITING_HPP
#define WAITING_HPP
#include <SDL2/SDL.h>
#include <string>

class Game;

class Waiting {
public:
    Waiting() = default;

    void handleEvents(Game& game, const SDL_Event& event);
    void update(Game& game);
    void render(const Game& game);

private:
    std::string recvBuffer; // handles partial TCP messages
    bool matchFound = false;
    bool waitingForOpponent = false;
    bool accepted = false;
    bool declined = false;

    SDL_Rect acceptRect{300, 450, 280, 75};
    SDL_Rect declineRect{430, 250, 150, 50};
    bool acceptHovered = false;
    bool declineHovered = false;
    bool acceptPressed = false;
    bool declinePressed = false;
};

#endif
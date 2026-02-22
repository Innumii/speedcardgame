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
    void render(Game& game);

private:
    std::string recvBuffer; // handles partial TCP messages
};

#endif
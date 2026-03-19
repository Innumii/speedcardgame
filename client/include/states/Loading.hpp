#ifndef LOADING_HPP
#define LOADING_HPP

#include <SDL2/SDL.h>
#include <cstddef>
#include <string>

class Game;

class Loading {
public:
    void enter(Game& game);
    void exit(Game& game);
    void handleEvents(Game& game, const SDL_Event& event);
    void update(Game& game);
    void render(const Game& game);

private:
    bool cardsPrepared{false};
    bool deckPrepared{false};
    std::size_t retryRounds{0};
    std::size_t currentCardIndex{0};
    std::size_t totalCards{0};
    std::string statusMessage{"Loading cards..."};
};

#endif
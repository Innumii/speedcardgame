#ifndef LOADING_HPP
#define LOADING_HPP

#include "StateInterface.hpp"
#include <SDL2/SDL.h>
#include <cstddef>
#include <string>

class Game;

class Loading : public StateInterface {
public:
    void enter(Game& game) override;
    void exit(Game& game) override;
    void handleEvents(Game& game, const SDL_Event& event) override;
    void update(Game& game) override;
    void render(Game& game) override;

private:
    int maxRetryRounds{3};
    bool cardsPrepared{false};
    bool deckPrepared{false};
    std::size_t retryRounds{0};
    std::size_t currentCardIndex{0};
    std::size_t totalCards{0};
    std::string statusMessage{"Loading cards..."};
};

#endif
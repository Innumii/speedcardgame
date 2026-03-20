#ifndef WAITING_HPP
#define WAITING_HPP
#include <SDL2/SDL.h>
#include <string>
#include <vector>

#include "render/Theme.hpp"

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

    SDL_Rect acceptRect{
        Theme::Waiting::ACCEPT_BUTTON_INITIAL_X,
        Theme::Waiting::ACCEPT_BUTTON_INITIAL_Y,
        Theme::Waiting::ACCEPT_BUTTON_WIDTH,
        Theme::Waiting::ACCEPT_BUTTON_HEIGHT
    };
    SDL_Rect declineRect{
        Theme::Waiting::DECLINE_BUTTON_INITIAL_X,
        Theme::Waiting::DECLINE_BUTTON_INITIAL_Y,
        Theme::Waiting::DECLINE_BUTTON_WIDTH,
        Theme::Waiting::DECLINE_BUTTON_HEIGHT
    };
    bool acceptHovered = false;
    bool declineHovered = false;
    bool acceptPressed = false;
    bool declinePressed = false;

    std::vector<int> opponentDeckCopies;
};

#endif
#ifndef STATE_INTERFACE_HPP
#define STATE_INTERFACE_HPP

#include <SDL2/SDL.h>

class Game;

class StateInterface {
public:
    virtual ~StateInterface() = default;

    virtual void enter(Game& game) { (void)game; }
    virtual void exit(Game& game) { (void)game; }
    virtual void handleEvents(Game& game, const SDL_Event& event) { (void)game; (void)event; }
    virtual void update(Game& game) { (void)game; }
    virtual void render(Game& game) { (void)game; }
};

#endif
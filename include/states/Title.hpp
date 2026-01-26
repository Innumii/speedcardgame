#ifndef TITLE_HPP
#define TITLE_HPP

#include <SDL2/SDL.h>
#include "core/GameState.hpp" //need this as we plan to use specific GameState stuff with this class

class Game; //use this over #include to prevent circular dependency and tight coupling
//since you're not grabbing anything specific from Game you can just declare

class Title {
public:
    void handleEvents(Game& game, const SDL_Event& event);
    void update(Game& game);
    void render(Game& game);
private:
    SDL_Rect startButton{300,150,200,80};
    SDL_Rect quitButton{300,250,200,80};
};

#endif
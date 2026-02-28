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
    void render(const Game& game);
private:
    void updateLayout(SDL_Renderer* renderer);
    SDL_Rect startButton{300,150,280,75};
    SDL_Rect quitButton{300,250,280,75};
    SDL_Rect BuildDeckButton{300,350,280,75};
    SDL_Rect OpenPacksButton{300, 450, 280, 75};
    SDL_Rect ConnectButton{300, 550, 280, 75};
    SDL_Rect titleBanner{180, 40, 600, 100};

    // animation state
    Uint32 animStartTick  {0};
    bool   animInitialized{false};
    int    hoveredButton  {-1};
};

#endif
#ifndef TITLE_HPP
#define TITLE_HPP

#include <SDL2/SDL.h>
#include "core/GameState.hpp" //need this as we plan to use specific GameState stuff with this class
#include "render/Theme.hpp"

class Game; //use this over #include to prevent circular dependency and tight coupling
//since you're not grabbing anything specific from Game you can just declare

class Title {
public:
    void handleEvents(Game& game, const SDL_Event& event);
    void update(Game& game);
    void render(const Game& game);
private:
    void updateLayout(SDL_Renderer* renderer);
    SDL_Rect startButton{
        Theme::Title::START_BUTTON_INITIAL_X,
        Theme::Title::START_BUTTON_INITIAL_Y,
        Theme::Title::MAIN_BUTTON_WIDTH,
        Theme::Title::MAIN_BUTTON_HEIGHT
    };
    SDL_Rect BuildDeckButton{
        Theme::Title::START_BUTTON_INITIAL_X,
        Theme::Title::BUILD_BUTTON_INITIAL_Y,
        Theme::Title::MAIN_BUTTON_WIDTH,
        Theme::Title::MAIN_BUTTON_HEIGHT
    };
    SDL_Rect OpenPacksButton{
        Theme::Title::START_BUTTON_INITIAL_X,
        Theme::Title::OPEN_PACKS_BUTTON_INITIAL_Y,
        Theme::Title::MAIN_BUTTON_WIDTH,
        Theme::Title::MAIN_BUTTON_HEIGHT
    };
    SDL_Rect logoutButton{
        Theme::Title::LOGOUT_BUTTON_INITIAL_X,
        Theme::Title::LOGOUT_BUTTON_INITIAL_Y,
        Theme::Title::SMALL_BUTTON_WIDTH,
        Theme::Title::SMALL_BUTTON_HEIGHT
    };
    SDL_Rect quitButton{
        Theme::Title::QUIT_BUTTON_INITIAL_X,
        Theme::Title::QUIT_BUTTON_INITIAL_Y,
        Theme::Title::SMALL_BUTTON_WIDTH,
        Theme::Title::SMALL_BUTTON_HEIGHT
    };
    SDL_Rect titleBanner{
        Theme::Title::BANNER_INITIAL_X,
        Theme::Title::BANNER_INITIAL_Y,
        Theme::Title::BANNER_WIDTH,
        Theme::Title::BANNER_HEIGHT
    };

    // animation state
    Uint32 animStartTick  {0};
    bool   animInitialized{false};
    bool layoutInitialized{false};

    int    hoveredButton  {-1};
};

#endif
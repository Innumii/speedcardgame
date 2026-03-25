#ifndef TITLE_HPP
#define TITLE_HPP

#include <SDL2/SDL.h>
#include <array>
#include <string>
#include "core/GameState.hpp"
#include "render/Theme.hpp"
#include "render/RenderText.hpp"
class Game;

class Title {
public:
    void handleEvents(Game& game, const SDL_Event& event);
    void update(Game& game);
    void render(const Game& game);
    ~Title();

private:
    void updateLayout(SDL_Renderer* renderer);

    // ── button rects ─────────────────────────────────────────────
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
    SDL_Rect ShopButton{
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

    Uint32 animStartTick  {0};
    bool   animInitialized{false};

    int hoveredButton{-1};

    struct CachedButton {
        SDL_Texture* texture = nullptr;
        int w = 0;
        int h = 0;
        std::string label;
        bool hovered = false;
        float lastScale = 0.0f;
    };

    std::array<CachedButton, 6> cachedButtons;

    SDL_Texture* buildButtonTexture(SDL_Renderer* renderer,
                                    const SDL_Rect& rect,
                                    const std::string& text,
                                    SDL_Color fill,
                                    bool hovered,
                                    const RenderText::FontSet& fonts);
};

#endif
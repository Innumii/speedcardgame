#ifndef PLAYING_HPP
#define PLAYING_HPP

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "../core/GameState.hpp" //need this as we plan to use specific GameState stuff with this class
#include "objects/Player.h"
#include "objects/Deck.h"

class Game; // forward declaration to avoid circular include

class Playing {
private:
    Player player;
    Deck deck;
    int drawIntervalSeconds;
    Uint32 lastDrawTick{0};
    bool running{false};

    struct DragState {
        bool active{false};
        std::size_t index{0};
        int offsetX{0};
        int offsetY{0};
        int x{0};
        int y{0};
    };

    using FontPtr = std::unique_ptr<TTF_Font, decltype(&TTF_CloseFont)>;

    SDL_Renderer* renderer{nullptr}; // non-owning, provided by Game
    FontPtr fontLarge{nullptr, TTF_CloseFont};
    FontPtr fontSmall{nullptr, TTF_CloseFont};
    std::vector<SDL_Rect> cardRects;
    DragState drag;

    void drawText(const std::string& text, TTF_Font* font, SDL_Color color, int x, int y);
    void drawWrappedText(const std::string& text, TTF_Font* font, SDL_Color color, int x, int y, std::size_t maxLineLen);
    std::vector<SDL_Rect> computeCardLayout(std::size_t count, int screenW, int screenH) const;

public:
    Playing(int drawIntervalSeconds = 3);
    ~Playing();

    Playing(const Playing&) = delete;
    Playing& operator=(const Playing&) = delete;
    Playing(Playing&&) noexcept = default;
    Playing& operator=(Playing&&) noexcept = default;
    void setup(Game& game);
    void handleEvent(Game& game, const SDL_Event& event);
    void run();
    void update(Game& game);
    void render(Game&);
};


#endif

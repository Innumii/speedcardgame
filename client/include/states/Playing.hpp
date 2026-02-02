#ifndef PLAYING_HPP
#define PLAYING_HPP

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "core/GameState.hpp" //need this as we plan to use specific GameState stuff with this class
#include "objects/Player.h"
#include "objects/Deck.h"
#include "core/Board.hpp"

class Game; // forward declaration to avoid circular include

class Playing {
    friend class RenderPlaying;
private:
    Player player;
    Deck deck;
    Board board;

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
    std::vector<SDL_Rect> playSlots;
    SDL_Rect discardZone{0, 0, 0, 0};
    DragState drag;
    std::size_t hoverIndex{static_cast<std::size_t>(-1)};
    Uint32 hoverStartTick{0};

    bool pointInRect(const SDL_Rect& rect, int x, int y);
    std::vector<SDL_Rect> computeCardLayout(std::size_t count, int screenW, int screenH) const;
    void computeZones(int screenW, int screenH);

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

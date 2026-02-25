#ifndef PLAYING_HPP
#define PLAYING_HPP

#include <SDL2/SDL.h>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "core/GameState.hpp"
#include "objects/Deck.h"
#include "objects/Player.h"
#include "core/Board.hpp"
#include "animation/animationQueue.hpp"
#include "render/RenderText.hpp"
#include "gameplay/GameAuthority.hpp" // added for authority pattern

class Game; // forward declaration to avoid circular include
class Card;

class Playing {
    friend class RenderPlaying;

private:
    // --- Game logic handled via authority ---
    std::unique_ptr<GameAuthority> authority; 

    // --- Local state for rendering/UI ---
    Deck deck;                   // deck of the local player
    SDL_Renderer* renderer{nullptr}; 
    RenderText::FontSet fonts{};
    std::vector<SDL_Rect> cardRects;
    std::vector<SDL_Rect> playSlots;
    SDL_Rect discardZone{0, 0, 0, 0};
    SDL_Rect menuButton{0, 0, 0, 0};
    SDL_Rect exitGameButton{0, 0, 0, 0};
    SDL_Rect returnToTitleButton{0, 0, 0, 0};
    AnimationQueue animationQueue;

    // --- Drag state for cards ---
    struct DragState {
        bool active{false};
        std::size_t index{0};
        int offsetX{0};
        int offsetY{0};
        int x{0};
        int y{0};
    };
    DragState drag;

    // --- Hover / UI ---
    std::size_t hoverIndex{static_cast<std::size_t>(-1)};
    Uint32 hoverStartTick{0};
    bool menuOpen{false};
    bool surrendered{false};

    // --- Spell targeting ---
    struct PendingSpellTargetState {
        bool active{false};
        std::unique_ptr<Card> spell;
    };
    PendingSpellTargetState pendingSpellTarget;

    // --- Timing ---
    int drawIntervalSeconds;
    Uint32 lastDrawTick{0};
    bool running{false};

    // --- Private helper methods ---
    bool pointInRect(const SDL_Rect& rect, int x, int y);
    bool isTargetedSpell(const Card& card) const;
    bool resolvePendingSpellTargetAt(int x, int y);
    std::vector<SDL_Rect> computeCardLayout(std::size_t count, int screenW, int screenH) const;
    void computeZones(int screenW, int screenH);
    void computeUiRects(int screenW, int screenH);
    SDL_Rect computeSelfDeckRect(int screenW, int screenH) const;
    bool tryDrawCardWithAnimation(Uint32 now);

public:
    explicit Playing(int drawIntervalSeconds = 3);
    ~Playing();

    // --- Disable copy ---
    Playing(const Playing&) = delete;
    Playing& operator=(const Playing&) = delete;
    // --- Allow move ---
    Playing(Playing&&) noexcept = default;
    Playing& operator=(Playing&&) noexcept = default;

    // --- Setup ---
    void setup(const Game& game);
    void setDeck(Deck newDeck);
    void setAuthority(std::unique_ptr<GameAuthority> auth); // new: assign LocalAuthority or ServerAuthority

    // --- Main loop / events ---
    void handleEvents(Game& game, const SDL_Event& event);
    void run();
    void update(Game& game);
    void render(const Game&);
};

#endif

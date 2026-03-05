#ifndef PLAYING_HPP
#define PLAYING_HPP

#include <SDL2/SDL.h>
#include <cstddef>
#include <memory>
#include <vector>

#include "core/GameState.hpp"
#include "objects/Player.h"
#include "core/Board.hpp"
#include "render/RenderText.hpp"
#include "animation/animationQueue.hpp"
#include "gameplay/GameAuthority.hpp"

class Game;

class Playing {
    friend class RenderPlaying;

public:
    Playing() = default;
    ~Playing();

    Playing(const Playing&) = delete;
    Playing& operator=(const Playing&) = delete;
    Playing(Playing&&) noexcept = default;
    Playing& operator=(Playing&&) noexcept = default;

    void setup(Game& game);
    void handleEvents(Game& game, const SDL_Event& event);
    void run();
    void update(Game& game);
    void render(const Game& game);

    void setupPlayers(Player&& local, Player&& remote);

private:
    // -------------------------
    // Game state
    // -------------------------
    Player localPlayer;
    Player remotePlayer;
    Board board;

    bool running{false};

    std::string recvBuffer; // handles partial TCP messages


    // -------------------------
    // Dragging cards
    // -------------------------
    struct DragState {
        bool active{false};
        std::size_t index{0}; // index into localPlayer.hand
        int offsetX{0};
        int offsetY{0};
        int x{0};
        int y{0};
    } drag;

    // -------------------------
    // Rendering
    // -------------------------
    SDL_Renderer* renderer{nullptr}; // non-owning
    RenderText::FontSet fonts{};
    std::vector<SDL_Rect> cardRects;
    std::vector<SDL_Rect> playSlots;
    std::vector<SDL_Rect> opponentSlots;
    SDL_Rect discardZone{0, 0, 0, 0};
    SDL_Rect menuButton{0, 0, 0, 0};
    SDL_Rect exitGameButton{0, 0, 0, 0};
    SDL_Rect returnToTitleButton{0, 0, 0, 0};
    std::size_t hoverIndex{static_cast<std::size_t>(-1)};
    Uint32 hoverStartTick{0};
    bool menuOpen{false};
    bool surrendered{false};
    AnimationQueue animationQueue;

    // -------------------------
    // Pending action (targeting system)
    // -------------------------
    struct PendingActionState {
        bool active{false};
        std::size_t handIndex{static_cast<std::size_t>(-1)};
        int sourceLane{-1}; // lane where the spell was dropped
        int targetOpponent{-1};

        void clear() {
            active = false;
            handIndex = static_cast<std::size_t>(-1);
            sourceLane = -1;
        }
    };
    PendingActionState pendingAction;

    // -------------------------
    // Authority (network bridge)
    // -------------------------
    std::unique_ptr<GameAuthority> authority;

    // -------------------------
    // Helpers
    // -------------------------
    bool pointInRect(const SDL_Rect& rect, int x, int y);
    bool resolvePendingActionAt(int x, int y);

    std::vector<SDL_Rect> computeCardLayout(std::size_t count, int screenW, int screenH) const;
    void computeZones(int screenW, int screenH);
    void computeUiRects(int screenW, int screenH);
    SDL_Rect computeSelfDeckRect(int screenW, int screenH) const;

    //Game related
    bool handleServerMessage(const std::string& msg);
    bool drawCard(int playerId, int cardId);
    void discardCard(int playerId, int cardId);
    void playCreature(int playerId, std::unique_ptr<Card> card, int lane);
    // void playSpell(int playerId, std::unique_ptr<Card> card, int sourceLane, int targetLane, int targetOpponent);
};

#endif
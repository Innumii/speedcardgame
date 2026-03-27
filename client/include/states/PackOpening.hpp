#ifndef PACKOPENING_HPP
#define PACKOPENING_HPP

#include <SDL2/SDL.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "render/Theme.hpp"
#include "core/GameState.hpp"
class Game;
class Card;

class PackOpening {
public:
    void enter(Game& game);
    void handleEvents(Game& game, const SDL_Event& event);
    void update(Game& game);
    void render(Game& game);
    void exit(Game& game);
    void leaveState(Game& game, GameState next);

    void tryFlush(Game& game);
    int getPendingInventoryOps() const;

private:
    struct OpenedCardResult {
        int cardIndex{-1};
        bool refunded{false};
        int resultingCopies{0};
    };

    bool applyInventoryDelta(const Game& game, const std::unordered_map<int, int>& deltaByCardId);

    void updateLayout(SDL_Renderer* renderer);
    void openPack(Game& game);

    std::vector<OpenedCardResult> lastOpenedCards;

    SDL_Rect backButton{
        Theme::PackOpening::BACK_BUTTON_INITIAL_X,
        Theme::PackOpening::BACK_BUTTON_INITIAL_Y,
        Theme::PackOpening::BACK_BUTTON_WIDTH,
        Theme::PackOpening::BACK_BUTTON_HEIGHT
    };
    SDL_Rect openPackButton{
        Theme::PackOpening::OPEN_BUTTON_INITIAL_X,
        Theme::PackOpening::OPEN_BUTTON_INITIAL_Y,
        Theme::PackOpening::OPEN_BUTTON_WIDTH,
        Theme::PackOpening::OPEN_BUTTON_HEIGHT
    };
    SDL_Rect shopButton{
        Theme::PackOpening::OPEN_BUTTON_INITIAL_X,
        Theme::PackOpening::OPEN_BUTTON_INITIAL_Y,
        Theme::PackOpening::OPEN_BUTTON_WIDTH,
        Theme::PackOpening::OPEN_BUTTON_HEIGHT
    };

    std::string statusMessage;
    int lastRefundCoins{0};
    bool backHovered{false};
    bool openHovered{false};
    bool shopHovered{false};
    int hoveredOpenedCard{-1};

    // ── Slide-in animation ───────────────────────────────────────────────────
    // revealStartTick is the moment the pack was opened.
    // cardSlideInTicks[i] holds the *scheduled* SDL tick at which card i's
    // slide begins (= revealStartTick + i * SlideInDelayMs).
    Uint32              revealStartTick{0};
    std::vector<Uint32> cardSlideInTicks;

    // ── Flip animation (click to reveal) ────────────────────────────────────
    std::vector<bool>   cardFlipped;    // has the user clicked this card
    std::vector<Uint32> cardFlipTicks;  // tick when flip animation started (0 = not yet)

    // ── Hover effect ─────────────────────────────────────────────────────────
    std::vector<Uint32> cardHoverStartTicks;
    std::vector<bool>   cardHoverActive;

    // ── Pack constants ───────────────────────────────────────────────────────
    static constexpr int PackSize            = 5;
    static constexpr int MaxCardCopies       = 4;
    static constexpr int RefundCoinsPerExtra = 10;
    static constexpr int PackCostCoins       = 100;

    // ── Animation tuning ─────────────────────────────────────────────────────
    static constexpr int SlideInDelayMs    = 150;  // ms stagger between each card's slide start
    static constexpr int SlideInDurationMs = 420;  // ms for each card to reach its target position
    static constexpr int FlipDurationMs    = 150;  // ms for the flip-reveal animation
    static constexpr int StaggerOffsetY    = 0;   // px of vertical stagger (up for even, down for odd)

    // ── Deferred service flush ────────────────────────────────────────────────
    int pendingCoinDelta = 0;
    std::unordered_map<int, int> pendingInventoryDelta;

    Uint32 lastFlushTick{0};
    static constexpr Uint32 FlushIntervalMs         = 20000;
    static constexpr int    CoinFlushThreshold      = 200;
    static constexpr int    InventoryFlushThreshold = 10;
};

#endif
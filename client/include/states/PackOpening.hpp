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

    // Reveal animation
    int revealedCount{0};
    Uint32 revealStartTick{0};

    static constexpr int PackSize = 5;
    static constexpr int MaxCardCopies = 4;
    static constexpr int RefundCoinsPerExtra = 10;
    static constexpr int PackCostCoins = 100;
    static constexpr int CardRevealIntervalMs = 350;

    int pendingCoinDelta = 0;
    std::unordered_map<int, int> pendingInventoryDelta;

    // ── Flush control ─────────────────────────────────────────────
    Uint32 lastFlushTick{0};

    static constexpr Uint32 FlushIntervalMs = 5000;      // 5 seconds
    static constexpr int CoinFlushThreshold = 200;       // flush if >= 200 coins changed
    static constexpr int InventoryFlushThreshold = 10;   // flush if >= 10 cards added

};

#endif

#ifndef PAYMENT_HPP
#define PAYMENT_HPP

#include "core/GameState.hpp"
#include "objects/ShopPackage.hpp"

#include <SDL2/SDL.h>
#include <string>
#include <vector>

class Game;

class Payment {
public:
    void enter(Game& game);
    void handleEvents(Game& game, const SDL_Event& event);
    void update(Game& game);
    void render(Game& game);

private:
    // ── Two-step flow ─────────────────────────────────────────────────────────
    enum class Step { SelectPackage, Checkout };
    Step currentStep = Step::SelectPackage;

    // slide: 0.0 = packages fully visible, 1.0 = form fully visible
    float slideProgress = 0.0f;
    bool  slideForward  = false; // true while animating toward form
    bool  slideBack     = false; // true while animating back to packages
    static constexpr float SLIDE_SPEED = 3.5f; // units per second (reaches 1.0 in ~0.29s)
    Uint32 lastTick = 0;

    // ── Package selection ─────────────────────────────────────────────────────
    int hoveredPackage  = -1;
    int selectedPackage = -1;

    // ── UI state ──────────────────────────────────────────────────────────────
    std::string statusMessage;

    bool backTitleHovered = false;
    bool backPacksHovered = false;
    bool refreshHovered   = false;
    bool buyNowHovered    = false;
    bool changePackHovered = false;
    bool payHovered       = false;
    bool payPressed       = false;

    bool awaitingCoinUpdate = false;
    int checkoutStartCoins = 0;
    Uint32 checkoutPollStartTick = 0;
    Uint32 checkoutLastPollTick = 0;
    static constexpr Uint32 CHECKOUT_POLL_INTERVAL_MS = 2000;
    static constexpr Uint32 CHECKOUT_POLL_TIMEOUT_MS = 120000;

    // ── Layout rects ──────────────────────────────────────────────────────────
    SDL_Rect backTitleButton  = {36,  122, 110, 34};
    SDL_Rect backPacksButton  = {158, 122, 160, 34};
    SDL_Rect refreshButton    = {330, 122, 90,  34};

    // Package panel
    std::vector<SDL_Rect> packageButtons;
    SDL_Rect buyNowButton = {};

    // Checkout panel
    SDL_Rect orderSummaryRect  = {};
    SDL_Rect payButtonRect     = {};
    SDL_Rect changePackButton  = {};

    // ── Helpers ───────────────────────────────────────────────────────────────
    void updateLayout(SDL_Renderer* renderer);

    // Render the two sliding panels (packages / form).
    // panelOffsetX is the LEFT edge of the "packages" panel in screen space.
    // The "form" panel sits immediately to its right (+screenW).
    void renderPackagesPanel(Game& game, int panelX);
    void renderFormPanel(Game& game, int panelX);
    void renderOrderSummary(SDL_Renderer* renderer, const ShopPackage::Package& pkg,
                            int panelX, const SDL_Rect& rect, const void* uiFonts);

    bool refreshPackages();
    bool requestCheckoutSession(Game& game, const ShopPackage::Package& package);
    bool tryLoadLatestCoins(Game& game, int& outCoins) const;

    static std::string formatMoney(int amountCents, const std::string& currencyCode);
};

#endif
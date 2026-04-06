#include "states/Payment.hpp"

#include "core/Game.hpp"
#include "render/RenderButton.hpp"
#include "render/RenderText.hpp"
#include "render/Theme.hpp"
#include "utils/EnvUtil.hpp"
#include "utils/HttpUtil.hpp"
#include "utils/JsonUtil.hpp"
#include "utils/RenderUtil.hpp"
#include "utils/StringUtil.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <map>
#include <sstream>

#include <SDL2/SDL_image.h>

#if defined(_WIN32)
#include <windows.h>
#include <shellapi.h>
#endif

// ─────────────────────────────────────────────────────────────────────────────
// Anonymous helpers (unchanged from original)
// ─────────────────────────────────────────────────────────────────────────────
namespace {
    std::map<std::string, SDL_Texture*> coinPackTextureCache;

    std::string getTierImagePath(const ShopPackage::Package& package) {
        const std::string key = StringUtil::toLower(package.id + " " + package.name);
        if (key.find("small")  != std::string::npos) return "assets/images/smallCoins.png";
        if (key.find("medium") != std::string::npos) return "assets/images/mediumCoins.png";
        if (key.find("large")  != std::string::npos) return "assets/images/largeCoins.png";
        return "assets/images/smallCoins.png";
    }

    SDL_Texture* getTierTexture(SDL_Renderer* renderer, const ShopPackage::Package& package) {
        if (!renderer) return nullptr;
        const std::string path = getTierImagePath(package);
        const auto existing = coinPackTextureCache.find(path);
        if (existing != coinPackTextureCache.end()) return existing->second;

        SDL_Surface* surface = IMG_Load(path.c_str());
        if (!surface) { coinPackTextureCache[path] = nullptr; return nullptr; }

        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_FreeSurface(surface);
        coinPackTextureCache[path] = texture;
        return texture;
    }

    std::string escapeForShellDoubleQuotes(const std::string& value) {
        std::string out;
        out.reserve(value.size());
        for (char c : value) {
            if (c == '"') out += '\\';
            out += c;
        }
        return out;
    }

    bool openExternalURL(const std::string& url) {
        if (url.empty()) return false;

#if defined(_WIN32)
        const auto result = reinterpret_cast<intptr_t>(
            ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL)
        );
        return result > 32;
#elif defined(__APPLE__)
        const std::string command = "open \"" + escapeForShellDoubleQuotes(url) + "\"";
        return std::system(command.c_str()) == 0;
#else
        const std::string command = "xdg-open \"" + escapeForShellDoubleQuotes(url) + "\"";
        return std::system(command.c_str()) == 0;
#endif
    }

    // Simple ease-out cubic: maps t in [0,1] → [0,1] with deceleration.
    float easeOut(float t) {
        const float s = 1.0f - t;
        return 1.0f - s * s * s;
    }
} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// enter
// ─────────────────────────────────────────────────────────────────────────────
void Payment::enter(Game& game) {
    updateLayout(game.getRenderer());

    currentStep      = Step::SelectPackage;
    slideProgress    = 0.0f;
    slideForward     = false;
    slideBack        = false;
    lastTick         = SDL_GetTicks();

    hoveredPackage   = -1;
    selectedPackage  = -1;

    backTitleHovered  = false;
    backPacksHovered  = false;
    refreshHovered    = false;
    buyNowHovered     = false;
    changePackHovered = false;
    payHovered        = false;
    payPressed        = false;
    awaitingCoinUpdate = false;
    checkoutSessionId.clear();
    checkoutStartCoins = game.getPackRefundCoins();
    checkoutPollStartTick = 0;
    checkoutLastPollTick = 0;

    statusMessage.clear();

    SDL_StartTextInput();

    if (ShopPackage::getNumberOfPackages() == 0) {
        if (!refreshPackages()) {
            statusMessage = "There was an error loading packages.";
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// handleEvents
// ─────────────────────────────────────────────────────────────────────────────
void Payment::handleEvents(Game& game, const SDL_Event& event) {
    updateLayout(game.getRenderer());

    if (event.type == SDL_QUIT) {
        SDL_StopTextInput();
        game.setNextState(GameState::Quit);
        return;
    }

    if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
        // Escape from form → go back to package selection; from packages → go to title
        if (currentStep == Step::Checkout) {
            slideBack    = true;
            slideForward = false;
            statusMessage.clear();
        } else {
            SDL_StopTextInput();
            game.setNextState(GameState::Title);
        }
        return;
    }

    // ── Compute the current panel X offset so hit-testing works during animation ──
    int screenW = Theme::SCREEN_DEFAULT_WIDTH;
    int screenH = Theme::SCREEN_DEFAULT_HEIGHT;
    SDL_GetRendererOutputSize(game.getRenderer(), &screenW, &screenH);

    const float easedProgress = easeOut(slideProgress);
    // packagesPanelX: starts at 0, slides to -screenW
    const int packagesPanelX = static_cast<int>(-easedProgress * screenW);
    // formPanelX: starts at +screenW, slides to 0
    const int formPanelX = packagesPanelX + screenW;

    if (event.type == SDL_MOUSEMOTION) {
        const int x = event.motion.x;
        const int y = event.motion.y;

        // Top-bar buttons are always hit-testable at their fixed positions
        backTitleHovered = RenderUtil::pointInRect(backTitleButton, x, y);
        backPacksHovered = RenderUtil::pointInRect(backPacksButton, x, y);
        refreshHovered   = RenderUtil::pointInRect(refreshButton,   x, y);

        // Package panel hovers (offset by panel position)
        hoveredPackage = -1;
        buyNowHovered  = false;
        if (formPanelX > 0) { // packages panel still at least partly visible
            for (int i = 0; i < static_cast<int>(packageButtons.size()); ++i) {
                SDL_Rect shifted = packageButtons[i];
                shifted.x += packagesPanelX;
                if (RenderUtil::pointInRect(shifted, x, y)) { hoveredPackage = i; break; }
            }
            SDL_Rect shiftedBuy = buyNowButton;
            shiftedBuy.x += packagesPanelX;
            buyNowHovered = RenderUtil::pointInRect(shiftedBuy, x, y);
        }

        // Form panel hovers
        changePackHovered = false;
        payHovered        = false;
        if (formPanelX < screenW) { // form panel at least partly visible
            SDL_Rect shiftedChange = changePackButton;
            shiftedChange.x += formPanelX;
            changePackHovered = RenderUtil::pointInRect(shiftedChange, x, y);

            SDL_Rect shiftedPay = payButtonRect;
            shiftedPay.x += formPanelX;
            payHovered = RenderUtil::pointInRect(shiftedPay, x, y);
        }
        return;
    }

    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        const int x = event.button.x;
        const int y = event.button.y;

        // Top-bar buttons always active
        if (RenderUtil::pointInRect(backTitleButton, x, y)) {
            SDL_StopTextInput();
            game.setNextState(GameState::Title);
            return;
        }
        if (RenderUtil::pointInRect(backPacksButton, x, y)) {
            SDL_StopTextInput();
            game.setNextState(GameState::PackOpening);
            return;
        }
        if (RenderUtil::pointInRect(refreshButton, x, y)) {
            statusMessage = refreshPackages() ? "" : "There was an error loading packages.";
            return;
        }

        // ── Package panel clicks (only when not fully slid away) ──────────────
        if (formPanelX > 0) {
            const auto& packages = ShopPackage::getPackages();
            for (int i = 0; i < static_cast<int>(packageButtons.size()) &&
                            i < static_cast<int>(packages.size()); ++i) {
                SDL_Rect shifted = packageButtons[i];
                shifted.x += packagesPanelX;
                if (RenderUtil::pointInRect(shifted, x, y)) {
                    selectedPackage = i;
                    statusMessage.clear();
                    return;
                }
            }

            // "Buy Now" button
            SDL_Rect shiftedBuy = buyNowButton;
            shiftedBuy.x += packagesPanelX;
            if (RenderUtil::pointInRect(shiftedBuy, x, y) && selectedPackage >= 0) {
                slideForward  = true;
                slideBack     = false;
                statusMessage.clear();
                return;
            }
        }

        // ── Form panel clicks (only when at least partly visible) ─────────────
        if (formPanelX < screenW) {
            // "← Change package" link
            SDL_Rect shiftedChange = changePackButton;
            shiftedChange.x += formPanelX;
            if (RenderUtil::pointInRect(shiftedChange, x, y)) {
                slideBack    = true;
                slideForward = false;
                statusMessage.clear();
                return;
            }

            // Pay button
            SDL_Rect shiftedPay = payButtonRect;
            shiftedPay.x += formPanelX;
            if (RenderUtil::pointInRect(shiftedPay, x, y)) {
                payPressed = true;
                return;
            }
        }
    }

    if (event.type == SDL_KEYDOWN) {
        if (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER) {
            if (currentStep == Step::Checkout) payPressed = true;
            return;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// update
// ─────────────────────────────────────────────────────────────────────────────
void Payment::update(Game& game) {
    updateLayout(game.getRenderer());

    // ── Advance slide animation ───────────────────────────────────────────────
    const Uint32 now = SDL_GetTicks();
    const float dt = static_cast<float>(now - lastTick) / 1000.0f;
    lastTick = now;

    if (slideForward) {
        slideProgress += SLIDE_SPEED * dt;
        if (slideProgress >= 1.0f) {
            slideProgress = 1.0f;
            slideForward  = false;
            currentStep   = Step::Checkout;
        }
    } else if (slideBack) {
        slideProgress -= SLIDE_SPEED * dt;
        if (slideProgress <= 0.0f) {
            slideProgress = 0.0f;
            slideBack     = false;
            currentStep   = Step::SelectPackage;
        }
    }

    if (awaitingCoinUpdate) {
        const Uint32 nowTicks = SDL_GetTicks();
        if ((nowTicks - checkoutLastPollTick) >= CHECKOUT_POLL_INTERVAL_MS) {
            checkoutLastPollTick = nowTicks;

            if (!checkoutSessionId.empty()) {
                const std::string host = EnvUtil::getCardsServiceHost();
                const int port = EnvUtil::getCardsServicePort();
                const std::string statusPath = "/cards/payments/checkout-status?session_id=" + checkoutSessionId;

                int statusCode = -1;
                std::string statusBody;
                (void)HttpUtil::sendHttp(host, port, "GET", statusPath, "", statusCode, statusBody);
            }

            int latestCoins = game.getPackRefundCoins();
            if (tryLoadLatestCoins(game, latestCoins) && latestCoins > checkoutStartCoins) {
                game.setPackRefundCoins(latestCoins);
                awaitingCoinUpdate = false;
                checkoutSessionId.clear();
                statusMessage = "Payment confirmed. Coins updated.";
            }
        }

        if ((nowTicks - checkoutPollStartTick) >= CHECKOUT_POLL_TIMEOUT_MS) {
            awaitingCoinUpdate = false;
            checkoutSessionId.clear();
            statusMessage = "Payment submitted. Coin update is taking longer than expected.";
        }
    }

    // ── Process payment ───────────────────────────────────────────────────────
    if (!payPressed) return;
    payPressed = false;

    const auto& packages = ShopPackage::getPackages();
    if (selectedPackage < 0 || selectedPackage >= static_cast<int>(packages.size())) {
        statusMessage = "There was an error processing payment.";
        return;
    }
    requestCheckoutSession(game, packages[selectedPackage]);
}

// ─────────────────────────────────────────────────────────────────────────────
// render
// ─────────────────────────────────────────────────────────────────────────────
void Payment::render(Game& game) {
    SDL_Renderer* renderer = game.getRenderer();
    if (!renderer) return;

    updateLayout(renderer);

    const auto& titleFonts = game.getTitleFonts();
    const auto& uiFonts    = game.getUIFonts();

    int screenW = Theme::SCREEN_DEFAULT_WIDTH;
    int screenH = Theme::SCREEN_DEFAULT_HEIGHT;
    SDL_GetRendererOutputSize(renderer, &screenW, &screenH);

    // Background
    SDL_SetRenderDrawColor(renderer, Theme::BG.r, Theme::BG.g, Theme::BG.b, 255);
    SDL_RenderClear(renderer);

    // ── Fixed top-bar (always at screen position) ─────────────────────────────
    RenderText::drawText(renderer, "Coin Shop", titleFonts.large, Theme::BANNER_TEXT, 36, 26);

    const std::string subtitle = "Choose a bundle, then complete secure payment in your browser.";
    RenderText::drawText(renderer, subtitle, uiFonts.small, Theme::TEXT_MUTED, 36, 88);

    const std::string coinText = "Current coins: " + std::to_string(game.getPackRefundCoins());
    int coinW = 0, coinH = 0;
    RenderText::measureText(uiFonts.large, coinText, coinW, coinH);
    RenderText::drawText(renderer, coinText, uiFonts.large, Theme::BANNER_BORDER, screenW - coinW - 24, 32);

    RenderButton::drawButton(renderer, backTitleButton, "Back to Title", uiFonts.small,
                             Theme::BTN_QUIT, Theme::BTN_BORDER, Theme::BTN_TEXT, backTitleHovered);
    RenderButton::drawButton(renderer, backPacksButton, "Open Packs", uiFonts.small,
                             Theme::BTN_PRIMARY, Theme::BTN_BORDER, Theme::BTN_TEXT, backPacksHovered);
    RenderButton::drawButton(renderer, refreshButton, "Refresh", uiFonts.small,
                             Theme::BTN_SECONDARY, Theme::BTN_BORDER, Theme::BTN_TEXT, refreshHovered);

    // ── Set up a clip rect so panels don't bleed into the top-bar ────────────
    const int contentTop = 166; // just below the nav buttons
    SDL_Rect clipRect = {0, contentTop, screenW, screenH - contentTop};
    SDL_RenderSetClipRect(renderer, &clipRect);

    // ── Compute panel positions ───────────────────────────────────────────────
    const float easedProgress  = easeOut(slideProgress);
    const int packagesPanelX   = static_cast<int>(-easedProgress * static_cast<float>(screenW));
    const int formPanelX       = packagesPanelX + screenW;

    // Draw both panels; the clip rect hides whatever is off-screen.
    renderPackagesPanel(game, packagesPanelX);
    renderFormPanel(game, formPanelX);

    // Restore clip
    SDL_RenderSetClipRect(renderer, nullptr);

    // ── Status message (below the content area, always visible) ──────────────
    if (!statusMessage.empty()) {
        RenderText::drawText(renderer, statusMessage, uiFonts.small, Theme::TEXT_IVORY, 36, screenH - 36);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// renderPackagesPanel
// ─────────────────────────────────────────────────────────────────────────────
void Payment::renderPackagesPanel(Game& game, int panelX) {
    SDL_Renderer* renderer = game.getRenderer();
    const auto& uiFonts    = game.getUIFonts();

    const auto& packages = ShopPackage::getPackages();

    if (packages.empty()) {
        int screenW = Theme::SCREEN_DEFAULT_WIDTH;
        int screenH = Theme::SCREEN_DEFAULT_HEIGHT;
        SDL_GetRendererOutputSize(renderer, &screenW, &screenH);

        const std::string emptyText = "No packages available.";
        int emptyW = 0, emptyH = 0;
        RenderText::measureText(uiFonts.large, emptyText, emptyW, emptyH);
        RenderText::drawText(renderer, emptyText, uiFonts.large, Theme::ERROR_RED,
                             panelX + (screenW - emptyW) / 2, screenH / 2 - emptyH / 2);
    }

    for (int i = 0; i < static_cast<int>(packageButtons.size()) &&
                    i < static_cast<int>(packages.size()); ++i) {
        const ShopPackage::Package& package = packages[i];

        SDL_Rect button = packageButtons[i];
        button.x += panelX;

        const bool hovered  = (hoveredPackage == i);
        const bool selected = (selectedPackage == i);

        SDL_Color fill   = hovered ? Theme::BTN_PRIMARY : SDL_Color{35, 38, 56, 255};
        SDL_Color border = selected ? Theme::SUCCESS_GREEN
                                    : (hovered ? Theme::BANNER_BORDER : SDL_Color{90, 100, 130, 255});
        RenderUtil::drawRoundedRect(renderer, button, 16, fill, border);

        SDL_Rect imageRect{button.x + 14, button.y + 12, 168, button.h - 24};
        if (SDL_Texture* texture = getTierTexture(renderer, package)) {
            SDL_RenderCopy(renderer, texture, nullptr, &imageRect);
        } else {
            SDL_SetRenderDrawColor(renderer, 52, 56, 78, 255);
            SDL_RenderFillRect(renderer, &imageRect);
            SDL_SetRenderDrawColor(renderer, 120, 130, 165, 255);
            SDL_RenderDrawRect(renderer, &imageRect);
        }

        const int textLeft = imageRect.x + imageRect.w + 18;
        const std::string title = package.name;
        const std::string coins = std::to_string(package.coins) + " coins";
        std::string price = formatMoney(package.amountCents, package.currency);
        if (package.discountPercent > 0)
            price += "   (" + std::to_string(package.discountPercent) + "% OFF)";

        RenderText::drawText(renderer, title, uiFonts.large, Theme::BANNER_TEXT,  textLeft, button.y + 14);
        RenderText::drawText(renderer, coins, uiFonts.large, Theme::SUCCESS_GREEN, textLeft, button.y + 46);
        RenderText::drawText(renderer, price, uiFonts.small, Theme::TEXT_IVORY,    textLeft, button.y + 76);
    }

    // ── "Buy Now" button ──────────────────────────────────────────────────────
    SDL_Rect shiftedBuy = buyNowButton;
    shiftedBuy.x += panelX;

    const bool canBuy = (selectedPackage >= 0);
    RenderButton::drawButton(renderer, shiftedBuy,
                             canBuy ? "Buy Now" : "Select a Package",
                             uiFonts.large,
                             canBuy ? Theme::BTN_START    : Theme::BTN_DISABLED,
                             canBuy ? Theme::BTN_BORDER   : Theme::BTN_DISABLED_BORDER,
                             canBuy ? Theme::BTN_TEXT     : Theme::BTN_DISABLED_TEXT,
                             canBuy && buyNowHovered);
}

// ─────────────────────────────────────────────────────────────────────────────
// renderFormPanel
// ─────────────────────────────────────────────────────────────────────────────
void Payment::renderFormPanel(Game& game, int panelX) {
    SDL_Renderer* renderer = game.getRenderer();
    const auto& uiFonts    = game.getUIFonts();

    const auto& packages = ShopPackage::getPackages();

    // ── "← Change package" back link ─────────────────────────────────────────
    SDL_Rect shiftedChange = changePackButton;
    shiftedChange.x += panelX;
    RenderButton::drawButton(renderer, shiftedChange, "Change Package", uiFonts.small,
                             Theme::BTN_SECONDARY, Theme::BTN_BORDER, Theme::BTN_TEXT, changePackHovered);

    // ── Order summary strip ───────────────────────────────────────────────────
    if (selectedPackage >= 0 && selectedPackage < static_cast<int>(packages.size())) {
        const ShopPackage::Package& pkg = packages[selectedPackage];
        SDL_Rect summaryRect = orderSummaryRect;
        summaryRect.x += panelX;

        // Summary card background
        const SDL_Color summaryFill   = {28, 32, 50, 255};
        const SDL_Color summaryBorder = {60, 160, 100, 180};
        RenderUtil::drawRoundedRect(renderer, summaryRect, 10, summaryFill, summaryBorder);

        // Coin image thumbnail
        SDL_Rect thumbRect{summaryRect.x + 12, summaryRect.y + 10, 56, summaryRect.h - 20};
        if (SDL_Texture* tex = getTierTexture(renderer, pkg)) {
            SDL_RenderCopy(renderer, tex, nullptr, &thumbRect);
        }

        // Package info
        const int infoX = thumbRect.x + thumbRect.w + 14;
        const std::string title = pkg.name;
        const std::string coins = std::to_string(pkg.coins) + " coins";
        RenderText::drawText(renderer, title, uiFonts.large, Theme::BANNER_TEXT,   infoX, summaryRect.y + 12);
        RenderText::drawText(renderer, coins, uiFonts.large, Theme::SUCCESS_GREEN, infoX, summaryRect.y + 42);

        // Price (right-aligned)
        const std::string price = formatMoney(pkg.amountCents, pkg.currency);
        int priceW = 0, priceH = 0;
        RenderText::measureText(uiFonts.large, price, priceW, priceH);
        RenderText::drawText(renderer, price, uiFonts.large, Theme::TEXT_IVORY,
                             summaryRect.x + summaryRect.w - priceW - 16,
                             summaryRect.y + (summaryRect.h - priceH) / 2);
    }

    SDL_Rect helperRect = {
        orderSummaryRect.x + panelX,
        orderSummaryRect.y + orderSummaryRect.h + 24,
        orderSummaryRect.w,
        82
    };
    RenderUtil::drawRoundedRect(renderer, helperRect, 10, SDL_Color{26, 30, 46, 255}, SDL_Color{70, 80, 114, 180});
    RenderText::drawText(renderer,
                         "You will be redirected to Stripe to enter card details.",
                         uiFonts.small,
                         Theme::TEXT_IVORY,
                         helperRect.x + 14,
                         helperRect.y + 16);
    RenderText::drawText(renderer,
                         "Coins are granted after Stripe webhook confirmation.",
                         uiFonts.small,
                         Theme::TEXT_MUTED,
                         helperRect.x + 14,
                         helperRect.y + 44);

    // ── Pay button ────────────────────────────────────────────────────────────
    SDL_Rect shiftedPay = payButtonRect;
    shiftedPay.x += panelX;

    const bool canPay = (selectedPackage >= 0);
    RenderButton::drawButton(renderer, shiftedPay,
                             canPay ? "Open Stripe Checkout" : "Select Package",
                             uiFonts.large,
                             canPay ? Theme::BTN_START        : Theme::BTN_DISABLED,
                             canPay ? Theme::BTN_BORDER       : Theme::BTN_DISABLED_BORDER,
                             canPay ? Theme::BTN_TEXT         : Theme::BTN_DISABLED_TEXT,
                             canPay && payHovered);
}

// ─────────────────────────────────────────────────────────────────────────────
// updateLayout
// ─────────────────────────────────────────────────────────────────────────────
void Payment::updateLayout(SDL_Renderer* renderer) {
    int screenW = Theme::SCREEN_DEFAULT_WIDTH;
    int screenH = Theme::SCREEN_DEFAULT_HEIGHT;
    if (renderer) SDL_GetRendererOutputSize(renderer, &screenW, &screenH);

    const int topButtonY = 122;
    const int smallGap   = 12;
    const int leftX      = 36;

    backTitleButton = {leftX, topButtonY, 110, 34};
    backPacksButton = {backTitleButton.x + backTitleButton.w + smallGap, topButtonY, 160, 34};
    refreshButton   = {backPacksButton.x + backPacksButton.w + smallGap, topButtonY,  90, 34};

    // ── Package panel layout (positions are panel-relative; panelX added at render time) ──
    packageButtons.clear();
    const int buttonW    = std::max(420, screenW - 120);
    const int buttonH    = 128;
    const int gap        = 16;
    const int x          = (screenW - buttonW) / 2;
    const int maxVisible = std::min(3, ShopPackage::getNumberOfPackages());

    const int contentTop    = 176;
    // Leave room for the Buy Now button at the bottom
    const int buyNowH       = 50;
    const int buyNowMarginB = 20;
    const int cardsBottom   = screenH - buyNowH - buyNowMarginB - 28;

    const int totalCardsH = (maxVisible * buttonH) + (std::max(0, maxVisible - 1) * gap);
    int startY = contentTop + std::max(0, (cardsBottom - contentTop - totalCardsH) / 2);
    if (startY < contentTop) startY = contentTop;

    for (int i = 0; i < maxVisible; ++i) {
        packageButtons.push_back({x, startY + i * (buttonH + gap), buttonW, buttonH});
    }

    // Buy Now sits below the cards, right-aligned
    buyNowButton = {x + buttonW - 240, cardsBottom + buyNowMarginB, 240, buyNowH};

    // ── Checkout panel layout (also panel-relative) ───────────────────────────
    const int formLeft  = 36;
    const int formWidth = std::max(420, screenW - 72);

    // "← Change package" button near the top of the form area
    changePackButton = {formLeft, contentTop, 180, 34};

    // Order summary strip just below the back button
    const int summaryTop = contentTop + 34 + 14;
    orderSummaryRect = {formLeft, summaryTop, formWidth, 80};

    const int actionY = summaryTop + 80 + 132;
    payButtonRect   = {formLeft + formWidth - 320, actionY, 320, 50};
}

// ─────────────────────────────────────────────────────────────────────────────
// refreshPackages / requestCheckoutSession / formatMoney
// ─────────────────────────────────────────────────────────────────────────────
bool Payment::refreshPackages() {
    return ShopPackage::loadPackagesFromService();
}

bool Payment::requestCheckoutSession(Game& game, const ShopPackage::Package& package) {
    const int uid = game.getPlayerId();
    if (uid <= 0) {
        statusMessage = "There was an error processing payment.";
        return false;
    }

    const std::string host = EnvUtil::getCardsServiceHost();
    const int         port = EnvUtil::getCardsServicePort();
    const std::string path = "/cards/payments/checkout-session";

    std::ostringstream callbackSuccess;
    callbackSuccess << "https://" << host << ":" << port << "/cards/payments/checkout-complete?status=success";
    std::ostringstream callbackCancel;
    callbackCancel << "https://" << host << ":" << port << "/cards/payments/checkout-complete?status=cancel";

    std::ostringstream payload;
    payload << "{"
            << "\"uid\":"          << uid                                        << ","
            << "\"package_id\":\""  << JsonUtil::escapeJsonString(package.id)    << "\","
            << "\"success_url\":\"" << JsonUtil::escapeJsonString(callbackSuccess.str()) << "\","
            << "\"cancel_url\":\""  << JsonUtil::escapeJsonString(callbackCancel.str()) << "\""
            << "}";

    int         statusCode  = -1;
    std::string responseBody;
    if (!HttpUtil::sendHttp(host, port, "POST", path, payload.str(), statusCode, responseBody)) {
        statusMessage = "There was an error processing payment.";
        return false;
    }
    if (statusCode < 200 || statusCode >= 300) {
        statusMessage = "There was an error processing payment.";
        return false;
    }

    std::string checkoutURL;
    std::string checkoutID;
    if (!JsonUtil::readJsonStringField(responseBody, "url", checkoutURL) || checkoutURL.empty()) {
        statusMessage = "There was an error processing payment.";
        return false;
    }
    if (!JsonUtil::readJsonStringField(responseBody, "id", checkoutID) || checkoutID.empty()) {
        statusMessage = "There was an error processing payment.";
        return false;
    }

    if (!openExternalURL(checkoutURL)) {
        statusMessage = "There was an error processing payment.";
        return false;
    }

    checkoutStartCoins = game.getPackRefundCoins();
    checkoutSessionId = checkoutID;
    checkoutPollStartTick = SDL_GetTicks();
    checkoutLastPollTick = 0;
    awaitingCoinUpdate = true;
    statusMessage = "Checkout opened in your browser. Waiting for payment confirmation...";

    // Slide back to package selection and clear form
    slideBack     = true;
    slideForward  = false;
    selectedPackage = -1;

    return true;
}

bool Payment::tryLoadLatestCoins(Game& game, int& outCoins) const {
    const int uid = game.getPlayerId();
    if (uid <= 0) {
        return false;
    }

    const std::string host = EnvUtil::getCardsServiceHost();
    const int port = EnvUtil::getCardsServicePort();
    const std::string path = "/cards/inventories/" + std::to_string(uid);

    int statusCode = -1;
    std::string responseBody;
    if (!HttpUtil::sendHttp(host, port, "GET", path, "", statusCode, responseBody)) {
        return false;
    }
    if (statusCode < 200 || statusCode >= 300) {
        return false;
    }

    return JsonUtil::readJsonIntField(responseBody, "coins", outCoins);
}

std::string Payment::formatMoney(int amountCents, const std::string& currencyCode) {
    const int dollars = amountCents / 100;
    const int cents   = std::abs(amountCents % 100);
    std::ostringstream out;
    out << StringUtil::toUpper(currencyCode) << " " << dollars << ".";
    if (cents < 10) out << '0';
    out << cents;
    return out.str();
}
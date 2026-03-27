#include "states/PackOpening.hpp"

#include "core/Game.hpp"
#include "objects/CreatureCard.h"
#include "objects/SpellCard.h"
#include "objects/Inventory.hpp"
#include "render/RenderCard.hpp"
#include "render/RenderButton.hpp"
#include "render/RenderText.hpp"
#include "render/Theme.hpp"
#include "objects/Deck.h"
#include "objects/Inventory.hpp"
#include "utils/HttpUtil.hpp"
#include "utils/EnvUtil.hpp"
#include "utils/LoadAvailableCards.hpp"
#include "utils/RenderUtil.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <random>
#include <sstream>
#include <unordered_map>
#include <render/RenderBackdrop.hpp>
#include <iostream>

namespace {
    void drawCenteredText(SDL_Renderer* renderer, const std::string& text, TTF_Font* font, SDL_Color color, const SDL_Rect& box) {
        if (!renderer || !font || text.empty()) return;
        int textW = 0;
        int textH = 0;
        if (TTF_SizeUTF8(font, text.c_str(), &textW, &textH) != 0) return;
        const int x = box.x + (box.w - textW) / 2;
        const int y = box.y + (box.h - textH) / 2;
        RenderText::drawText(renderer, text, font, color, x, y);
    }

    // Shared layout helper: given screen dimensions and the back button's Y,
    // returns the Y positions used for even and odd cards in the staggered layout.
    // Also returns groupY (top of the entire card group including summary panel).
    struct CardLayout {
        int cardW, cardH, totalW, startX;
        int groupY, summaryY;
        int evenCardY;   // top Y for cards 0, 2, 4
        int oddCardY;    // top Y for cards 1, 3
    };

    CardLayout computeCardLayout(int screenW, int backButtonY,
                                 int packSize, int staggerOffsetY) {
        const int sidePad  = Theme::PackOpening::CARD_SIDE_PADDING;
        const int cardGap  = Theme::PackOpening::CARD_GAP;
        const int usableW  = screenW - 2 * sidePad;
        const int cardW    = (usableW - (packSize - 1) * cardGap) / packSize;
        const int cardH    = cardW * 3 / 2;
        const int totalW   = packSize * cardW + (packSize - 1) * cardGap;
        const int startX   = (screenW - totalW) / 2;

        const int badgeH     = Theme::PackOpening::CARD_BADGE_HEIGHT;
        const int summaryH   = Theme::PackOpening::SUMMARY_HEIGHT;
        const int summaryGap = Theme::PackOpening::SUMMARY_GAP;
        const int headerY    = Theme::PackOpening::HEADER_Y;
        const int topClear   = headerY + Theme::PackOpening::HEADER_CLEARANCE;
        const int bottomClear = backButtonY;

        // Group height now includes the extra vertical room for the stagger band.
        const int groupH = summaryH + summaryGap
                         + cardH + 2 * staggerOffsetY
                         + Theme::PackOpening::SUMMARY_CARD_GAP + badgeH;

        const int groupY    = topClear + std::max(0, (bottomClear - topClear - groupH) / 2);
        const int summaryY  = groupY;
        const int evenCardY = groupY + summaryH + summaryGap;          // cards 0, 2, 4 (upper)
        const int oddCardY  = evenCardY + 2 * staggerOffsetY;          // cards 1, 3   (lower)

        return {cardW, cardH, totalW, startX, groupY, summaryY, evenCardY, oddCardY};
    }
} // namespace

// ── State entry ──────────────────────────────────────────────────────────────

void PackOpening::enter(Game& game) {
    updateLayout(game.getRenderer());
    statusMessage.clear();
    lastOpenedCards.clear();
    lastRefundCoins    = 0;
    hoveredOpenedCard  = -1;
    revealStartTick    = 0;

    cardSlideInTicks.clear();
    cardFlipped.clear();
    cardFlipTicks.clear();
    cardHoverStartTicks.clear();
    cardHoverActive.clear();

    pendingCoinDelta = 0;
    pendingInventoryDelta.clear();

    if (!LoadAvailableCardsUtil::ensureAvailableCardsLoaded()) {
        statusMessage = "Failed to load card list.";
        return;
    }

    const auto& availableCards = LoadAvailableCardsUtil::getAvailableCards();
    if (!Inventory::loadInventoryAndCoinsFromService(game)) {
        statusMessage = "Failed to load inventory.";
    } else {
        game.setPackRefundCoins(Inventory::getCachedCoins());
    }

    lastFlushTick = SDL_GetTicks();
}

// ── State exit — flush all accumulated deltas to the service in one shot ─────

void PackOpening::exit(Game& game) {
    const bool hasInventoryChanges = !pendingInventoryDelta.empty();
    const bool hasCoinChanges      = (pendingCoinDelta != 0);

    if (!hasInventoryChanges && !hasCoinChanges) {
        return;
    }

    if (hasInventoryChanges) {
        if (!applyInventoryDelta(game, pendingInventoryDelta)) {
            std::cerr << "[PackOpening]: Failed to update Inventory delta\n";
        }
    }

    if (hasCoinChanges) {
        if (!Inventory::updateCoinsOnService(game, game.getPackRefundCoins())) {
            std::cerr << "[PackOpening]: Failed to update coin balance\n";
        }
    }

    pendingCoinDelta = 0;
    pendingInventoryDelta.clear();
}

// ── Helper: flush then transition ────────────────────────────────────────────

void PackOpening::leaveState(Game& game, GameState next) {
    exit(game);
    game.setNextState(next);
}

// ── Event handling ───────────────────────────────────────────────────────────

void PackOpening::handleEvents(Game& game, const SDL_Event& event) {
    updateLayout(game.getRenderer());

    if (event.type == SDL_QUIT) {
        leaveState(game, GameState::Quit);
        return;
    }

    if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
        leaveState(game, GameState::Title);
        return;
    }

    if (event.type == SDL_MOUSEMOTION) {
        const int x = event.motion.x;
        const int y = event.motion.y;
        const bool canOpenPack = game.getPackRefundCoins() >= PackCostCoins;

        backHovered = (x >= backButton.x && x <= backButton.x + backButton.w &&
                       y >= backButton.y && y <= backButton.y + backButton.h);
        shopHovered = (x >= shopButton.x && x <= shopButton.x + shopButton.w &&
                       y >= shopButton.y && y <= shopButton.y + shopButton.h);
        openHovered = (x >= openPackButton.x && x <= openPackButton.x + openPackButton.w &&
                       y >= openPackButton.y && y <= openPackButton.y + openPackButton.h &&
                       canOpenPack);

        hoveredOpenedCard = -1;
        if (!lastOpenedCards.empty() && !cardSlideInTicks.empty()) {
            int screenW = Theme::SCREEN_DEFAULT_WIDTH;
            int screenH = Theme::SCREEN_DEFAULT_HEIGHT;
            if (SDL_Renderer* renderer = game.getRenderer()) {
                SDL_GetRendererOutputSize(renderer, &screenW, &screenH);
            }
            (void)screenH;

            const auto [cardW, cardH, totalW, startX, groupY, summaryY, evenCardY, oddCardY] =
                computeCardLayout(screenW, backButton.y, PackSize, StaggerOffsetY);
            const int cardGap = Theme::PackOpening::CARD_GAP;
            const Uint32 now  = SDL_GetTicks();

            // ── Determine which slid-in card (if any) is under the cursor ──
            for (int i = 0; i < static_cast<int>(lastOpenedCards.size()); ++i) {
                if (i >= static_cast<int>(cardSlideInTicks.size())) continue;
                if (now < cardSlideInTicks[i]) continue; // still off-screen

                const int cardTopY = (i % 2 == 0) ? evenCardY : oddCardY;
                const SDL_Rect cardRect{startX + i * (cardW + cardGap), cardTopY, cardW, cardH};
                if (x >= cardRect.x && x <= cardRect.x + cardRect.w &&
                    y >= cardRect.y && y <= cardRect.y + cardRect.h) {
                    hoveredOpenedCard = i;
                    break;
                }
            }

            // ── Update per-card hover transition state ────────────────────
            for (int i = 0; i < static_cast<int>(lastOpenedCards.size()); ++i) {
                if (i >= static_cast<int>(cardHoverActive.size())) break;
                const bool isHovered = (i == hoveredOpenedCard);
                if (isHovered != cardHoverActive[i]) {
                    cardHoverActive[i]     = isHovered;
                    cardHoverStartTicks[i] = SDL_GetTicks();
                }
            }
        }
    }

    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        const int x = event.button.x;
        const int y = event.button.y;

        if (x >= backButton.x && x <= backButton.x + backButton.w &&
            y >= backButton.y && y <= backButton.y + backButton.h) {
            leaveState(game, GameState::Title);
            return;
        }

        if (x >= shopButton.x && x <= shopButton.x + shopButton.w &&
            y >= shopButton.y && y <= shopButton.y + shopButton.h) {
            leaveState(game, GameState::Payment);
            return;
        }

        if (x >= openPackButton.x && x <= openPackButton.x + openPackButton.w &&
            y >= openPackButton.y && y <= openPackButton.y + openPackButton.h) {
            if (game.getPackRefundCoins() < PackCostCoins) {
                statusMessage = "Not enough coins. Need " + std::to_string(PackCostCoins) + ".";
                return;
            }
            openPack(game);
            return;
        }

        // ── Click-to-flip: reveal a face-down card ────────────────────────
        if (!lastOpenedCards.empty() && !cardSlideInTicks.empty()) {
            int screenW = Theme::SCREEN_DEFAULT_WIDTH;
            int screenH = Theme::SCREEN_DEFAULT_HEIGHT;
            if (SDL_Renderer* renderer = game.getRenderer()) {
                SDL_GetRendererOutputSize(renderer, &screenW, &screenH);
            }
            (void)screenH;

            const auto [cardW, cardH, totalW, startX, groupY, summaryY, evenCardY, oddCardY] =
                computeCardLayout(screenW, backButton.y, PackSize, StaggerOffsetY);
            const int cardGap = Theme::PackOpening::CARD_GAP;
            const Uint32 now  = SDL_GetTicks();

            for (int i = 0; i < PackSize && i < static_cast<int>(lastOpenedCards.size()); ++i) {
                if (i >= static_cast<int>(cardSlideInTicks.size())) continue;
                if (now < cardSlideInTicks[i]) continue;                             // not yet slid in
                if (i < static_cast<int>(cardFlipped.size()) && cardFlipped[i]) continue; // already flipped

                const int cardTopY = (i % 2 == 0) ? evenCardY : oddCardY;
                const SDL_Rect cardRect{startX + i * (cardW + cardGap), cardTopY, cardW, cardH};
                if (x >= cardRect.x && x <= cardRect.x + cardRect.w &&
                    y >= cardRect.y && y <= cardRect.y + cardRect.h) {
                    cardFlipped[i]   = true;
                    cardFlipTicks[i] = SDL_GetTicks();
                    break;
                }
            }
        }
    }
}

// ── Update ───────────────────────────────────────────────────────────────────

void PackOpening::update(Game& game) {
    // Slide-in and flip animations are purely time-driven in render();
    // no per-frame state machine needed here.
    tryFlush(game);
}

// ── Render ───────────────────────────────────────────────────────────────────

void PackOpening::render(Game& game) {
    SDL_Renderer* renderer = game.getRenderer();
    if (!renderer) return;
    const auto& availableCards = LoadAvailableCardsUtil::getAvailableCards();

    updateLayout(renderer);

    const Uint32 now = SDL_GetTicks();

    const auto& uiFonts    = game.getUIFonts();
    const auto& titleFonts = game.getTitleFonts();

    int screenW = 800;
    int screenH = 600;
    SDL_GetRendererOutputSize(renderer, &screenW, &screenH);

    RenderBackdrop::drawBackgroundWithVignette(
        renderer, screenW, screenH,
        Theme::BG,
        Theme::Loading::VIGNETTE_COLOR,
        Theme::Loading::VIGNETTE_LAYERS,
        Theme::Loading::VIGNETTE_ALPHA_FALLOFF,
        Theme::Loading::VIGNETTE_MAX_ALPHA
    );

    // ── Header: title (left) + coins (right) ─────────────────────────────────
    const int headerY = Theme::PackOpening::HEADER_Y;
    {
        int titleW = 0, titleH = 0;
        RenderText::measureText(titleFonts.large, "Pack Opening", titleW, titleH);
        RenderText::drawText(renderer, "Pack Opening", titleFonts.large, Theme::BANNER_TEXT, 40, headerY);

        const std::string coinsText = "Coins: " + std::to_string(game.getPackRefundCoins());
        int coinsW = 0, coinsH = 0;
        RenderText::measureText(uiFonts.large, coinsText, coinsW, coinsH);
        const int coinsCentreY = headerY + (titleH - coinsH) / 2;
        RenderText::drawText(renderer, coinsText, uiFonts.large, Theme::BANNER_BORDER,
                             screenW - coinsW - 20, coinsCentreY);
    }

    // ── Buttons ───────────────────────────────────────────────────────────────
    const bool canOpenPack = game.getPackRefundCoins() >= PackCostCoins;

    RenderButton::drawButton(renderer, backButton, "Back to Title", uiFonts.large,
                             Theme::BTN_QUIT, Theme::BTN_BORDER, Theme::BTN_TEXT, backHovered);
    RenderButton::drawButton(renderer, shopButton, "Coin Shop", uiFonts.large,
                             Theme::BTN_PRIMARY, Theme::BTN_BORDER, Theme::BTN_TEXT, shopHovered);
    RenderButton::drawButton(renderer, openPackButton, "Open Pack (100c)", uiFonts.large,
                             canOpenPack ? Theme::BTN_START    : Theme::BTN_DISABLED,
                             canOpenPack ? Theme::BTN_BORDER   : Theme::BTN_DISABLED_BORDER,
                             canOpenPack ? Theme::BTN_TEXT     : Theme::BTN_DISABLED_TEXT,
                             canOpenPack && openHovered);

    // ── Empty state ───────────────────────────────────────────────────────────
    if (lastOpenedCards.empty()) {
        int promptW = 0, promptH = 0;
        const std::string promptStr = "Open a pack to add cards to inventory.";
        RenderText::measureText(uiFonts.small, promptStr, promptW, promptH);
        RenderText::drawText(renderer, promptStr, uiFonts.small, Theme::TEXT_MUTED,
                             (screenW - promptW) / 2, screenH / 2 - promptH / 2);
        if (!statusMessage.empty()) {
            int errW = 0, errH = 0;
            RenderText::measureText(uiFonts.large, statusMessage, errW, errH);
            RenderText::drawText(renderer, statusMessage, uiFonts.large, Theme::ERROR_RED,
                                 (screenW - errW) / 2, screenH / 2 + promptH + 8);
        }
        return;
    }

    // ── Layout ───────────────────────────────────────────────────────────────
    const int cardGap = Theme::PackOpening::CARD_GAP;
    const auto [cardW, cardH, totalW, startX, groupY, summaryY, evenCardY, oddCardY] =
        computeCardLayout(screenW, backButton.y, PackSize, StaggerOffsetY);

    const int badgeH    = Theme::PackOpening::CARD_BADGE_HEIGHT;
    const int summaryH  = Theme::PackOpening::SUMMARY_HEIGHT;

    RenderText textRenderer;

    // ── Summary panel ─────────────────────────────────────────────────────────
    if (!statusMessage.empty()) {
        SDL_Rect panel{startX, summaryY, totalW, summaryH};
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, Theme::PackOpening::SUMMARY_FILL.r,
                               Theme::PackOpening::SUMMARY_FILL.g,
                               Theme::PackOpening::SUMMARY_FILL.b,
                               Theme::PackOpening::SUMMARY_FILL.a);
        SDL_RenderFillRect(renderer, &panel);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(renderer, Theme::BANNER_BORDER.r, Theme::BANNER_BORDER.g,
                               Theme::BANNER_BORDER.b, 255);
        SDL_RenderDrawRect(renderer, &panel);
        drawCenteredText(renderer, statusMessage, uiFonts.large, Theme::BANNER_BORDER, panel);
    }

    // ── BADGES — drawn first (background layer) ───────────────────────────────
    // Only shown once the flip animation is past the midpoint (face is becoming visible).
    for (int i = 0; i < static_cast<int>(lastOpenedCards.size()); ++i) {
        if (i >= static_cast<int>(cardFlipped.size()) || !cardFlipped[i]) continue;

        // Wait until past the flip midpoint so the badge appears together with the face.
        if (i < static_cast<int>(cardFlipTicks.size()) && cardFlipTicks[i] > 0) {
            const float flipT = std::min(1.0f, static_cast<float>(now - cardFlipTicks[i])
                                               / static_cast<float>(FlipDurationMs));
            if (flipT < 0.5f) continue;
        }

        const auto& result  = lastOpenedCards[i];
        const int cardTopY  = (i % 2 == 0) ? evenCardY : oddCardY;
        SDL_Rect baseRect{startX + i * (cardW + cardGap), cardTopY, cardW, cardH};

        const SDL_Color badgeFill  = result.refunded ? Theme::PackOpening::DUPLICATE_FILL
                                                     : Theme::PackOpening::NEW_FILL;
        const std::string badgeLabel = result.refunded
            ? ("DUPE  +" + std::to_string(RefundCoinsPerExtra) + "c") : "NEW!";

        SDL_Rect badge{
            baseRect.x,
            baseRect.y + baseRect.h + Theme::PackOpening::SUMMARY_CARD_GAP,
            baseRect.w,
            badgeH
        };
        SDL_SetRenderDrawColor(renderer, badgeFill.r, badgeFill.g, badgeFill.b, 255);
        SDL_RenderFillRect(renderer, &badge);
        drawCenteredText(renderer, badgeLabel, uiFonts.small, Theme::PackOpening::BADGE_TEXT, badge);
    }

    // ── CARDS ─────────────────────────────────────────────────────────────────
    constexpr Uint32 hoverTransMs     = 150U;
    constexpr float  hoverTargetScale = 1.12F;

    for (int i = 0; i < static_cast<int>(lastOpenedCards.size()); ++i) {
        const auto& result = lastOpenedCards[i];

        // ── Slide-in: skip cards not yet scheduled to appear ──────────────
        if (i >= static_cast<int>(cardSlideInTicks.size())) continue;
        const Uint32 slideStart = cardSlideInTicks[i];
        if (now < slideStart) continue;

        // Ease-out cubic: starts fast, decelerates into the target position.
        float slideT = std::min(1.0f, static_cast<float>(now - slideStart)
                                       / static_cast<float>(SlideInDurationMs));
        {
            const float inv = 1.0f - slideT;
            slideT = 1.0f - inv * inv * inv;
        }

        const int targetX  = startX + i * (cardW + cardGap);
        const int fromX    = screenW; // off-screen to the right
        const int currentX = static_cast<int>(fromX + (targetX - fromX) * slideT);
        const int cardTopY = (i % 2 == 0) ? evenCardY : oddCardY;

        SDL_Rect baseRect{currentX, cardTopY, cardW, cardH};

        // ── Flip animation ────────────────────────────────────────────────
        bool  showFace   = false;
        float flipXScale = 1.0f;
        bool  midFlip    = false;

        const bool wasFlipped   = (i < static_cast<int>(cardFlipped.size())) && cardFlipped[i];
        const Uint32 flipStart  = (i < static_cast<int>(cardFlipTicks.size())) ? cardFlipTicks[i] : 0u;

        if (wasFlipped && flipStart > 0) {
            const float flipT = std::min(1.0f, static_cast<float>(now - flipStart)
                                               / static_cast<float>(FlipDurationMs));
            midFlip = (flipT < 1.0f);

            if (flipT < 0.5f) {
                // First half: card-back shrinks toward zero width.
                flipXScale = 1.0f - 2.0f * flipT;
                showFace   = false;
            } else {
                // Second half: card-face grows from zero width.
                flipXScale = 2.0f * (flipT - 0.5f);
                showFace   = true;
            }
        }

        // ── Hover scale — preserved, but suppressed during active flip ────
        float hoverScale = 1.0f;
        if (!midFlip && i < static_cast<int>(cardHoverActive.size())) {
            const bool   hovered   = cardHoverActive[i];
            const Uint32 hoverTick = (i < static_cast<int>(cardHoverStartTicks.size()))
                                     ? cardHoverStartTicks[i] : 0u;
            const float  elapsed   = static_cast<float>(now - hoverTick);
            const float  t         = std::min(1.0f, elapsed / static_cast<float>(hoverTransMs));
            const float  eased     = t * t * (3.0f - 2.0f * t); // smoothstep
            hoverScale = hovered
                ? (1.0f + (hoverTargetScale - 1.0f) * eased)
                : (hoverTargetScale - (hoverTargetScale - 1.0f) * eased);
        }

        // ── Combine hover (XY) and flip (X-only) into final draw rect ─────
        const int hoverW = static_cast<int>(cardW * hoverScale);
        const int hoverH = static_cast<int>(cardH * hoverScale);
        const int finalW = std::max(1, static_cast<int>(hoverW * std::abs(flipXScale)));

        SDL_Rect drawRect{
            baseRect.x + (cardW - finalW) / 2,
            baseRect.y + (cardH - hoverH) / 2,
            finalW,
            hoverH
        };

        // ── Draw face or back ─────────────────────────────────────────────
        if (showFace) {
            if (result.cardIndex >= 0 &&
                result.cardIndex < static_cast<int>(availableCards.size()) &&
                availableCards[result.cardIndex]) {
                RenderCard::drawPreview(renderer, textRenderer,
                                        *availableCards[result.cardIndex],
                                        drawRect, uiFonts.small, uiFonts.large);
            } else {
                RenderCard::drawCardBack(renderer, drawRect);
            }

            // Qty chip — only visible on face-up cards.
            const std::string qtyStr = std::to_string(result.resultingCopies)
                                     + "/" + std::to_string(MaxCardCopies);
            const int chipW = Theme::PackOpening::QTY_CHIP_WIDTH;
            const int chipH = Theme::PackOpening::QTY_CHIP_HEIGHT;
            SDL_Rect chip{
                baseRect.x + baseRect.w - chipW - Theme::PackOpening::QTY_CHIP_MARGIN,
                baseRect.y + Theme::PackOpening::QTY_CHIP_MARGIN,
                chipW, chipH
            };
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, Theme::PackOpening::QTY_CHIP_BG.r,
                                   Theme::PackOpening::QTY_CHIP_BG.g,
                                   Theme::PackOpening::QTY_CHIP_BG.b,
                                   Theme::PackOpening::QTY_CHIP_BG.a);
            SDL_RenderFillRect(renderer, &chip);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
            drawCenteredText(renderer, qtyStr, uiFonts.small, Theme::PackOpening::QTY_TEXT, chip);
        } else {
            RenderCard::drawCardBack(renderer, drawRect);
        }
    }
}

// ── Layout ───────────────────────────────────────────────────────────────────

void PackOpening::updateLayout(SDL_Renderer* renderer) {
    int screenW = 800;
    int screenH = 600;
    if (renderer) {
        SDL_GetRendererOutputSize(renderer, &screenW, &screenH);
    }

    const int gap    = 20;
    const int groupW = backButton.w + gap + shopButton.w + gap + openPackButton.w;
    const int maxButtonH = std::max(backButton.h, std::max(shopButton.h, openPackButton.h));
    const int startX = (screenW - groupW) / 2;
    const int y      = screenH - maxButtonH - Theme::PackOpening::BUTTON_BOTTOM_MARGIN;

    backButton.x = startX;
    backButton.y = y;

    shopButton.x = backButton.x + backButton.w + gap;
    shopButton.y = y;

    openPackButton.x = shopButton.x + shopButton.w + gap;
    openPackButton.y = y;
}

// ── openPack — accumulates deltas, no service calls ──────────────────────────

void PackOpening::openPack(Game& game) {
    const auto& availableCards    = LoadAvailableCardsUtil::getAvailableCards();
    auto& inventoryCopies         = Inventory::getCachedInventoryCopiesMutable();

    if (availableCards.empty()) {
        statusMessage = "No cards available to open.";
        return;
    }

    if (inventoryCopies.size() != availableCards.size()) {
        if (!Inventory::ensureInventoryAndCoinsLoaded(game, availableCards, MaxCardCopies)) {
            statusMessage = "Failed to load inventory for your account.";
            return;
        }
        inventoryCopies = Inventory::getCachedInventoryCopies();
        game.setPackRefundCoins(Inventory::getCachedCoins());
    }

    const int currentCoins = game.getPackRefundCoins();
    if (currentCoins < PackCostCoins) {
        statusMessage = "Not enough coins. Need " + std::to_string(PackCostCoins) + ".";
        return;
    }

    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_int_distribution<int> dist(0, static_cast<int>(availableCards.size()) - 1);

    std::vector<int> nextInventory = inventoryCopies;
    std::unordered_map<int, int> deltaByCardId;
    std::vector<OpenedCardResult> results;
    results.reserve(PackSize);

    int refundCoins = 0;

    for (int i = 0; i < PackSize; ++i) {
        const int cardIndex = dist(rng);
        OpenedCardResult result;
        result.cardIndex = cardIndex;

        if (cardIndex < 0 || cardIndex >= static_cast<int>(nextInventory.size())) {
            result.refunded        = true;
            refundCoins           += RefundCoinsPerExtra;
            result.resultingCopies = 0;
            results.push_back(result);
            continue;
        }

        if (nextInventory[cardIndex] >= MaxCardCopies) {
            result.refunded        = true;
            refundCoins           += RefundCoinsPerExtra;
            result.resultingCopies = nextInventory[cardIndex];
            results.push_back(result);
            continue;
        }

        result.refunded = false;
        nextInventory[cardIndex] += 1;
        result.resultingCopies    = nextInventory[cardIndex];

        const int cardId = availableCards[cardIndex] ? availableCards[cardIndex]->getId() : -1;
        if (cardId > 0) {
            deltaByCardId[cardId] += 1;
        }

        results.push_back(result);
    }

    // ── Apply in-memory state immediately ────────────────────────────────────
    inventoryCopies = std::move(nextInventory);

    const int updatedCoins = currentCoins - PackCostCoins + refundCoins;
    game.setPackRefundCoins(updatedCoins);
    Inventory::setCachedCoins(updatedCoins);

    // ── Accumulate deltas for deferred flush ──────────────────────────────────
    for (const auto& pair : deltaByCardId) {
        pendingInventoryDelta[pair.first] += pair.second;
    }
    pendingCoinDelta += (updatedCoins - currentCoins);

    // ── Initialise slide-in animation ─────────────────────────────────────────
    // Cards enter staggered from the right, all face-down.
    // The user must click each card to trigger its flip reveal.
    const Uint32 packOpenTick = SDL_GetTicks();
    revealStartTick = packOpenTick;

    cardSlideInTicks.resize(PackSize);
    for (int i = 0; i < PackSize; ++i) {
        // Each card's slide begins SlideInDelayMs after the previous one.
        cardSlideInTicks[i] = packOpenTick + static_cast<Uint32>(i * SlideInDelayMs);
    }

    cardFlipped.assign(PackSize, false);
    cardFlipTicks.assign(PackSize, 0u);
    cardHoverStartTicks.assign(PackSize, 0u);
    cardHoverActive.assign(PackSize, false);
    hoveredOpenedCard = -1;

    // ── Update UI state ───────────────────────────────────────────────────────
    lastOpenedCards = std::move(results);
    lastRefundCoins = refundCoins;

    int cardsAdded = 0;
    for (const auto& pair : deltaByCardId) { cardsAdded += pair.second; }

    statusMessage = "Pack opened (-" + std::to_string(PackCostCoins) + "c): +"
                  + std::to_string(cardsAdded) + " card(s)";
    if (refundCoins > 0) {
        statusMessage += ", +" + std::to_string(refundCoins) + " coins refund.";
    }
}

// ── applyInventoryDelta ───────────────────────────────────────────────────────

bool PackOpening::applyInventoryDelta(const Game& game, const std::unordered_map<int, int>& deltaByCardId) {
    if (deltaByCardId.empty()) return true;

    const std::string host   = EnvUtil::getCardsServiceHost();
    const int         port   = EnvUtil::getCardsServicePort();
    const std::string path   = "/cards/inventories";
    const int         userId = game.getPlayerId();
    if (userId <= 0) return false;

    std::ostringstream payload;
    payload << "{\"uid\":" << userId << ",\"cards\":{";
    bool first = true;
    for (const auto& pair : deltaByCardId) {
        if (pair.second <= 0) continue;
        if (!first) payload << ',';
        payload << "\"" << pair.first << "\":" << pair.second;
        first = false;
    }
    payload << "}}";

    int statusCode = -1;
    std::string responseBody;
    if (!HttpUtil::sendHttp(host, port, "PUT", path, payload.str(), statusCode, responseBody)) {
        return false;
    }

    return statusCode >= 200 && statusCode < 300;
}

int PackOpening::getPendingInventoryOps() const {
    int total = 0;
    for (const auto& pair : pendingInventoryDelta) {
        total += std::abs(pair.second);
    }
    return total;
}

void PackOpening::tryFlush(Game& game) {
    const Uint32 now     = SDL_GetTicks();
    const Uint32 elapsed = now - lastFlushTick;

    const bool hasInventoryChanges = !pendingInventoryDelta.empty();
    const bool hasCoinChanges      = (pendingCoinDelta != 0);

    if (!hasInventoryChanges && !hasCoinChanges) return;

    const bool timeExceeded      = elapsed >= FlushIntervalMs;
    const bool coinExceeded      = std::abs(pendingCoinDelta) >= CoinFlushThreshold;
    const bool inventoryExceeded = getPendingInventoryOps() >= InventoryFlushThreshold;

    if (!(timeExceeded || coinExceeded || inventoryExceeded)) return;

    std::cout << "[PackOpening]: Flushing batch...\n";

    if (hasInventoryChanges) {
        if (!applyInventoryDelta(game, pendingInventoryDelta)) {
            std::cerr << "[PackOpening]: Failed to update Inventory delta\n";
            return;
        }
    }

    if (hasCoinChanges) {
        if (!Inventory::updateCoinsOnService(game, game.getPackRefundCoins())) {
            std::cerr << "[PackOpening]: Failed to update coin balance\n";
            return;
        }
    }

    pendingInventoryDelta.clear();
    pendingCoinDelta  = 0;
    lastFlushTick     = now;
}
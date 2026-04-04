#include "states/PackOpening.hpp"

#include "core/Game.hpp"
#include "core/Audio.hpp"

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

    struct CardLayout {
        int cardW, cardH, totalW, startX;
        int groupY, summaryY;
        int evenCardY;
        int oddCardY;
    };

    CardLayout computeCardLayout(int screenW, int backButtonY,
                                 int packSize, int staggerOffsetY) {
        const int sidePad  = Theme::PackOpening::CARD_SIDE_PADDING;
        const int cardGap  = Theme::PackOpening::CARD_GAP;
        const int usableW  = screenW - 2 * sidePad;
        const int cardW    = (usableW - (packSize - 1) * cardGap) / packSize;
        const int cardH    = cardW * Theme::PREVIEW_ASPECT_RATIO;
        const int totalW   = packSize * cardW + (packSize - 1) * cardGap;
        const int startX   = (screenW - totalW) / 2;

        const int badgeH     = Theme::PackOpening::CARD_BADGE_HEIGHT;
        const int summaryH   = Theme::PackOpening::SUMMARY_HEIGHT;
        const int summaryGap = Theme::PackOpening::SUMMARY_GAP;
        const int headerY    = Theme::PackOpening::HEADER_Y;
        const int topClear   = headerY + Theme::PackOpening::HEADER_CLEARANCE;
        const int bottomClear = backButtonY;

        const int groupH = summaryH + summaryGap
                         + cardH + 2 * staggerOffsetY
                         + Theme::PackOpening::SUMMARY_CARD_GAP + badgeH;

        const int groupY    = topClear + std::max(0, (bottomClear - topClear - groupH) / 2);
        const int summaryY  = groupY;
        const int evenCardY = groupY + summaryH + summaryGap;
        const int oddCardY  = evenCardY + 2 * staggerOffsetY;

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

    // Reset flush state so it's clean for this session.
    flushState = FlushState{};

    if (!LoadAvailableCardsUtil::ensureAvailableCardsLoaded()) {
        statusMessage = "Failed to load card list.";
        return;
    }

    if (!Inventory::loadInventoryAndCoinsFromService(game)) {
        statusMessage = "Failed to load inventory.";
    } else {
        game.setPackRefundCoins(Inventory::getCachedCoins());
    }
}

// ── State exit — final guaranteed flush ──────────────────────────────────────

void PackOpening::exit(Game& game) {
    // Unconditional flush: anything still pending must reach the server.
    // We skip the backoff check here — the player is leaving so we must try.
    if (pendingInventoryDelta.empty() && pendingCoinDelta == 0) return;

    std::cout << "[PackOpening] exit: flushing remaining deltas.\n";

    if (!pendingInventoryDelta.empty()) {
        if (!applyInventoryDelta(game, pendingInventoryDelta)) {
            std::cerr << "[PackOpening] exit: inventory flush failed.\n";
        }
    }

    if (pendingCoinDelta != 0) {
        if (!Inventory::updateCoinsOnService(game, game.getPackRefundCoins())) {
            std::cerr << "[PackOpening] exit: coin flush failed.\n";
        }
    }

    pendingCoinDelta = 0;
    pendingInventoryDelta.clear();
    flushState = FlushState{};
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

            const auto [cardW, cardH, totalW, startX, groupY, summaryY, evenCardY, oddCardY] =computeCardLayout(screenW, openPackButton.y, PackSize, StaggerOffsetY);
            const int cardGap = Theme::PackOpening::CARD_GAP;
            const Uint32 now  = SDL_GetTicks();

            for (int i = 0; i < static_cast<int>(lastOpenedCards.size()); ++i) {
                if (i >= static_cast<int>(cardSlideInTicks.size())) continue;
                if (now < cardSlideInTicks[i]) continue;

                const int cardTopY = (i % 2 == 0) ? evenCardY : oddCardY;
                const SDL_Rect cardRect{startX + i * (cardW + cardGap), cardTopY, cardW, cardH};
                if (x >= cardRect.x && x <= cardRect.x + cardRect.w &&
                    y >= cardRect.y && y <= cardRect.y + cardRect.h) {
                    hoveredOpenedCard = i;
                    break;
                }
            }

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

        if (!lastOpenedCards.empty() && !cardSlideInTicks.empty()) {
            Audio::playSFX("draw");
            int screenW = Theme::SCREEN_DEFAULT_WIDTH;
            int screenH = Theme::SCREEN_DEFAULT_HEIGHT;
            if (SDL_Renderer* renderer = game.getRenderer()) {
                SDL_GetRendererOutputSize(renderer, &screenW, &screenH);
            }
            (void)screenH;

            const auto [cardW, cardH, totalW, startX, groupY, summaryY, evenCardY, oddCardY] = computeCardLayout(screenW, openPackButton.y, PackSize, StaggerOffsetY);
            const int cardGap = Theme::PackOpening::CARD_GAP;
            const Uint32 now  = SDL_GetTicks();

            for (int i = 0; i < PackSize && i < static_cast<int>(lastOpenedCards.size()); ++i) {
                if (i >= static_cast<int>(cardSlideInTicks.size())) continue;
                if (now < cardSlideInTicks[i]) continue;
                if (i < static_cast<int>(cardFlipped.size()) && cardFlipped[i]) continue;

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

    // ── Header ────────────────────────────────────────────────────────────────
    const int headerY = Theme::PackOpening::HEADER_Y;
    {
        int titleW = 0, titleH = 0;
        RenderText::measureText(titleFonts.large, "Open Packs", titleW, titleH);
        RenderText::drawText(renderer, "Open Packs", titleFonts.large, Theme::BANNER_TEXT, 40, headerY);

        const std::string coinsText = "Coins: " + std::to_string(game.getPackRefundCoins());
        int coinsW = 0, coinsH = 0;
        RenderText::measureText(uiFonts.large, coinsText, coinsW, coinsH);
        const int coinsCentreY = headerY + (titleH - coinsH) / 2;
        RenderText::drawText(renderer, coinsText, uiFonts.large, Theme::BANNER_BORDER,
                             screenW - coinsW - 20, coinsCentreY);
    }

    // ── Buttons ───────────────────────────────────────────────────────────────
    const bool canOpenPack = game.getPackRefundCoins() >= PackCostCoins;

    RenderButton::drawButton(renderer, backButton, "Back to Title", uiFonts.small,
                            Theme::BTN_QUIT, Theme::BTN_BORDER, Theme::BTN_TEXT, backHovered);
    RenderButton::drawButton(renderer, shopButton, "Coin Shop", uiFonts.small,
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

    // ── Layout ────────────────────────────────────────────────────────────────
    const int cardGap = Theme::PackOpening::CARD_GAP;
    const auto [cardW, cardH, totalW, startX, groupY, summaryY, evenCardY, oddCardY] = computeCardLayout(screenW, openPackButton.y, PackSize, StaggerOffsetY);

    const int badgeH    = Theme::PackOpening::CARD_BADGE_HEIGHT;
    const int summaryH  = Theme::PackOpening::SUMMARY_HEIGHT;

    RenderText textRenderer;

    // ── Badges ────────────────────────────────────────────────────────────────
    for (int i = 0; i < static_cast<int>(lastOpenedCards.size()); ++i) {
        if (i >= static_cast<int>(cardFlipped.size()) || !cardFlipped[i]) continue;

        if (i < static_cast<int>(cardFlipTicks.size()) && cardFlipTicks[i] > 0) {
            const float flipT = std::min(1.0f, static_cast<float>(now - cardFlipTicks[i])
                                               / static_cast<float>(FlipDurationMs));
            if (flipT < 0.5f) continue;
        }

        const auto& result  = lastOpenedCards[i];
        const int cardTopY  = (i % 2 == 0) ? evenCardY : oddCardY;
        SDL_Rect baseRect{startX + i * (cardW + cardGap), cardTopY, cardW, cardH};

        SDL_Color badgeFill;
        std::string badgeLabel;
        if (result.resultingCopies >= MaxCardCopies) {
            badgeFill  = Theme::PackOpening::DUPLICATE_FILL;
            badgeLabel = "DUPE  +" + std::to_string(RefundCoinsPerExtra) + "c";
        } else if (result.resultingCopies == 1) {
            badgeFill  = Theme::PackOpening::NEW_FILL;
            badgeLabel = "NEW!";
        } else {
            badgeFill  = Theme::PackOpening::NEW_FILL;
            badgeLabel = std::to_string(result.resultingCopies)
                       + "/" + std::to_string(MaxCardCopies) + " OWNED!";
        }

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

    // ── Cards ─────────────────────────────────────────────────────────────────
    constexpr Uint32 hoverTransMs     = 150U;
    constexpr float  hoverTargetScale = 1.12F;

    for (int i = 0; i < static_cast<int>(lastOpenedCards.size()); ++i) {
        const auto& result = lastOpenedCards[i];

        if (i >= static_cast<int>(cardSlideInTicks.size())) continue;
        const Uint32 slideStart = cardSlideInTicks[i];
        if (now < slideStart) continue;

        float slideT = std::min(1.0f, static_cast<float>(now - slideStart)
                                       / static_cast<float>(SlideInDurationMs));
        {
            const float inv = 1.0f - slideT;
            slideT = 1.0f - inv * inv * inv;
        }

        const int targetX  = startX + i * (cardW + cardGap);
        const int fromX    = screenW;
        const int currentX = static_cast<int>(fromX + (targetX - fromX) * slideT);
        const int cardTopY = (i % 2 == 0) ? evenCardY : oddCardY;

        SDL_Rect baseRect{currentX, cardTopY, cardW, cardH};

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
                flipXScale = 1.0f - 2.0f * flipT;
                showFace   = false;
            } else {
                flipXScale = 2.0f * (flipT - 0.5f);
                showFace   = true;
            }
        }

        float hoverScale = 1.0f;
        if (!midFlip && i < static_cast<int>(cardHoverActive.size())) {
            const bool   hovered   = cardHoverActive[i];
            const Uint32 hoverTick = (i < static_cast<int>(cardHoverStartTicks.size()))
                                     ? cardHoverStartTicks[i] : 0u;
            const float  elapsed   = static_cast<float>(now - hoverTick);
            const float  t         = std::min(1.0f, elapsed / static_cast<float>(hoverTransMs));
            const float  eased     = t * t * (3.0f - 2.0f * t);
            hoverScale = hovered
                ? (1.0f + (hoverTargetScale - 1.0f) * eased)
                : (hoverTargetScale - (hoverTargetScale - 1.0f) * eased);
        }

        const int hoverW = static_cast<int>(cardW * hoverScale);
        const int hoverH = static_cast<int>(cardH * hoverScale);
        const int finalW = std::max(1, static_cast<int>(hoverW * std::abs(flipXScale)));

        SDL_Rect drawRect{
            baseRect.x + (cardW - finalW) / 2,
            baseRect.y + (cardH - hoverH) / 2,
            finalW,
            hoverH
        };

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

    // Back to Title and Coin Shop sit in the top-left, mirroring Payment's nav bar.
    const int topButtonY = Theme::PackOpening::HEADER_Y + Theme::PackOpening::HEADER_CLEARANCE + 20;
    const int smallGap   = 12;
    const int leftX      = 36;

    backButton  = {leftX, topButtonY, 110, 34};
    shopButton  = {leftX + 110 + smallGap, topButtonY, 160, 34};

    // Open Pack stays at the bottom, centered horizontally.
    const int openPackY  = screenH - openPackButton.h - Theme::PackOpening::BUTTON_BOTTOM_MARGIN;
    openPackButton.x     = (screenW - openPackButton.w) / 2;
    openPackButton.y     = openPackY;
}

// ── openPack ─────────────────────────────────────────────────────────────────

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

    // ── Accumulate deltas ────────────────────────────────────────────────────
    for (const auto& pair : deltaByCardId) {
        pendingInventoryDelta[pair.first] += pair.second;
    }
    pendingCoinDelta = std::clamp(
        pendingCoinDelta + static_cast<int64_t>(updatedCoins - currentCoins),
        -CoinDeltaCap,
        CoinDeltaCap
    );
    const int totalOps = [&] {
        int n = 0;
        for (const auto& pair : pendingInventoryDelta) n += pair.second;
        return n;
    }();

    const Uint32 now = SDL_GetTicks();
    if (flushState.dirtyWindowStart == 0) {
        flushState.dirtyWindowStart = now;
    }
    flushState.lastDirtyTick = now;

    const bool coinCapHit      = std::abs(pendingCoinDelta) >= CoinDeltaCap;
    const bool inventoryCapHit = totalOps >= HardFlushOpsThreshold;

    if (coinCapHit || inventoryCapHit) {
        std::cout << "[PackOpening] Cap reached ("
                << (coinCapHit ? "coins" : "inventory")
                << "), forcing flush.\n";
        tryFlush(game, true);
    }

    // ── Slide-in animation ────────────────────────────────────────────────────
    const Uint32 packOpenTick = now;
    revealStartTick = packOpenTick;

    cardSlideInTicks.resize(PackSize);
    for (int i = 0; i < PackSize; ++i) {
        Audio::playSFX("draw");
        cardSlideInTicks[i] = packOpenTick + static_cast<Uint32>(i * SlideInDelayMs);
    }

    cardFlipped.assign(PackSize, false);
    cardFlipTicks.assign(PackSize, 0u);
    cardHoverStartTicks.assign(PackSize, 0u);
    cardHoverActive.assign(PackSize, false);
    hoveredOpenedCard = -1;

    lastOpenedCards = std::move(results);
    lastRefundCoins = refundCoins;

    statusMessage.clear();
}

// ── applyInventoryDelta ───────────────────────────────────────────────────────

bool PackOpening::applyInventoryDelta(const Game& game,
                                      const std::unordered_map<int, int>& deltaByCardId) {
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

// ── tryFlush — debounce + coalesce window + exponential backoff ───────────────
//
//  Design:
//
//  A flush is triggered when ANY of these conditions is true:
//   1. Quiet period:    No new packs opened for DebounceMs          → user paused; safe to flush
//   2. Coalesce cap:    First dirty change is older than CoalesceMaxMs → must not delay further
//   3. Forced:        Cap threshold reached in openPack()            → caller demands flush
//  After a failed flush, the next attempt is delayed by an exponential backoff
//  (capped at MaxBackoffMs). This prevents a broken server from being hammered by
//  hundreds of clients all retrying at the same cadence.
//
//  tryFlush() exits immediately (no HTTP work) when:
//   - There is nothing pending
//   - We are still inside a backoff window
//   - None of the three conditions above is satisfied
//
void PackOpening::tryFlush(Game& game, bool force) {
    // Nothing to do → exit immediately, no SDL_GetTicks() overhead.
    if (pendingInventoryDelta.empty() && pendingCoinDelta == 0) return;

    const Uint32 now = SDL_GetTicks();

    // Respect backoff window from a previous failure.
    if (now < flushState.nextAllowedFlushTick) return;

    // ── Evaluate flush conditions ─────────────────────────────────────────────

    const bool quietPeriod = (now - flushState.lastDirtyTick)     >= DebounceMs;
    const bool windowExpired = (now - flushState.dirtyWindowStart) >= CoalesceMaxMs;


    if (!force && !quietPeriod && !windowExpired) return;

    // ── Flush ─────────────────────────────────────────────────────────────────
    std::cout << "[PackOpening] Flushing ("
              << (force         ? "forced"   :
                  quietPeriod   ? "quiet"    : "window")
              << ") "
              << " retries: " << flushState.retryCount << "\n";

    bool ok = true;

    if (!pendingInventoryDelta.empty()) {
        if (!applyInventoryDelta(game, pendingInventoryDelta)) {
            std::cerr << "[PackOpening] Inventory flush failed.\n";
            ok = false;
        }
    }

    if (ok && pendingCoinDelta != 0) {
        if (!Inventory::updateCoinsOnService(game, game.getPackRefundCoins())) {
            std::cerr << "[PackOpening] Coin flush failed.\n";
            ok = false;
        }
    }

    if (ok) {
        // ── Success: clear all pending state and reset flush tracker ─────────
        pendingInventoryDelta.clear();
        pendingCoinDelta = 0;
        flushState       = FlushState{};
    } else {
        // ── Failure: schedule an exponential backoff ──────────────────────────
        // Backoff doubles with each consecutive failure, capped at MaxBackoffMs.
        // The dirty-window start and last-dirty tick are NOT reset so the coalesce
        // window continues to accumulate time — ensuring we don't delay forever.
        const Uint32 backoffMs = std::min(
            BaseBackoffMs << flushState.retryCount,   // 2^n * base
            MaxBackoffMs
        );
        flushState.retryCount       = std::min(flushState.retryCount + 1u, MaxRetryCount);
        flushState.nextAllowedFlushTick = now + backoffMs;

        std::cerr << "[PackOpening] Next retry in " << backoffMs << "ms "
                  << "(attempt " << flushState.retryCount << ").\n";
    }
}
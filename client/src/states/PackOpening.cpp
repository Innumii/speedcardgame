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
    int resolveUserId(const Game& game) {
        const int playerId = game.getPlayerId();
        if (playerId > 0) {
            return playerId;
        }
        return EnvUtil::getEnvIntOrDefault("CARDS_SERVICE_UID", -1);
    }

    void drawCenteredText(SDL_Renderer* renderer, const std::string& text, TTF_Font* font, SDL_Color color, const SDL_Rect& box) {
        if (!renderer || !font || text.empty()) return;
        int textW = 0;
        int textH = 0;
        if (TTF_SizeUTF8(font, text.c_str(), &textW, &textH) != 0) return;
        const int x = box.x + (box.w - textW) / 2;
        const int y = box.y + (box.h - textH) / 2;
        RenderText::drawText(renderer, text, font, color, x, y);
    }
}

// ── State entry ──────────────────────────────────────────────────────────────

void PackOpening::enter(Game& game) {
    updateLayout(game.getRenderer());
    statusMessage.clear();
    lastOpenedCards.clear();
    lastRefundCoins = 0;
    hoveredOpenedCard = -1;

    // Reset pending deltas for this session in PackOpening
    pendingCoinDelta = 0;
    pendingInventoryDelta.clear();

    if (!loadAvailableCards(game)) {
        statusMessage = "Failed to load card list.";
        inventoryCopies.clear();
        return;
    }

    int loadedCoins = 0;
    if (!Inventory::loadInventoryAndCoinsFromService(game, availableCards, inventoryCopies, loadedCoins, MaxCardCopies)) {
        statusMessage = "Failed to load inventory.";
    } else {
        game.setPackRefundCoins(loadedCoins);
    }

    lastFlushTick = SDL_GetTicks();
}

// ── State exit — flush all accumulated deltas to the service in one shot ─────

void PackOpening::exit(Game& game) {
    // Nothing to flush if the user never opened a pack, or all packs were
    // pure duplicates (no inventory changes needed).
    std::cout << "EXIT CALLED\n";
    const bool hasInventoryChanges = !pendingInventoryDelta.empty();
    const bool hasCoinChanges      = (pendingCoinDelta != 0);

    if (!hasInventoryChanges && !hasCoinChanges) {
        return;
    }

    // Apply accumulated inventory delta in a single PUT request.
    if (hasInventoryChanges) {
        if (!applyInventoryDelta(game, pendingInventoryDelta)) {
            std::cerr << "[PackOpening]: Failed to update Inventory delta\n";
        }
    }

    // Persist the final coin balance (already kept accurate on game object).
    if (hasCoinChanges) {
        if (!Inventory::updateCoinsOnService(game, game.getPackRefundCoins())) {
            std::cerr << "[PackOpening]: Failed to update coin balance\n";
        }
    }

    pendingCoinDelta = 0;
    pendingInventoryDelta.clear();
}

// ── Helper: change state and flush before leaving ────────────────────────────

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
        if (!lastOpenedCards.empty()) {
            int screenW = Theme::SCREEN_DEFAULT_WIDTH;
            int screenH = Theme::SCREEN_DEFAULT_HEIGHT;
            if (SDL_Renderer* renderer = game.getRenderer()) {
                SDL_GetRendererOutputSize(renderer, &screenW, &screenH);
            }

            const int sidePad = Theme::PackOpening::CARD_SIDE_PADDING;
            const int cardGap = Theme::PackOpening::CARD_GAP;
            const int usableW  = screenW - 2 * sidePad;
            const int cardW    = (usableW - (PackSize - 1) * cardGap) / PackSize;
            const int cardH    = cardW * 3 / 2;
            const int totalW   = PackSize * cardW + (PackSize - 1) * cardGap;
            const int startX   = (screenW - totalW) / 2;

            const int badgeH    = Theme::PackOpening::CARD_BADGE_HEIGHT;
            const int summaryH  = Theme::PackOpening::SUMMARY_HEIGHT;
            const int summaryGap = Theme::PackOpening::SUMMARY_GAP;
            const int groupH    = summaryH + summaryGap + cardH + Theme::PackOpening::SUMMARY_CARD_GAP + badgeH;
            const int headerY = Theme::PackOpening::HEADER_Y;
            const int topClear    = headerY + Theme::PackOpening::HEADER_CLEARANCE;
            const int bottomClear = backButton.y;
            const int groupY      = topClear + std::max(0, (bottomClear - topClear - groupH) / 2);
            const int cardY = groupY + summaryH + summaryGap;

            for (int i = 0; i < static_cast<int>(lastOpenedCards.size()); ++i) {
                if (i >= revealedCount) {
                    break;
                }

                const SDL_Rect cardRect{startX + i * (cardW + cardGap), cardY, cardW, cardH};
                if (x >= cardRect.x && x <= cardRect.x + cardRect.w && y >= cardRect.y && y <= cardRect.y + cardRect.h) {
                    hoveredOpenedCard = i;
                    break;
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
    }
}

// ── Update ───────────────────────────────────────────────────────────────────

void PackOpening::update(Game& game) {
    if (!lastOpenedCards.empty() && revealedCount < static_cast<int>(lastOpenedCards.size())) {
        const Uint32 now = SDL_GetTicks();
        const int elapsed = static_cast<int>(now - revealStartTick);
        revealedCount = std::min(static_cast<int>(lastOpenedCards.size()),
                                  elapsed / CardRevealIntervalMs + 1);
    }

    tryFlush(game);
}

// ── Render ───────────────────────────────────────────────────────────────────

void PackOpening::render(Game& game) {
    SDL_Renderer* renderer = game.getRenderer();
    if (!renderer) return;

    updateLayout(renderer);

    const auto& uiFonts = game.getUIFonts();
    const auto& titleFonts = game.getTitleFonts();

    int screenW = 800;
    int screenH = 600;
    SDL_GetRendererOutputSize(renderer, &screenW, &screenH);

    RenderBackdrop::drawBackgroundWithVignette(
        renderer,
        screenW,
        screenH,
        Theme::BG,
        Theme::Loading::VIGNETTE_COLOR,
        Theme::Loading::VIGNETTE_LAYERS,
        Theme::Loading::VIGNETTE_ALPHA_FALLOFF,
        Theme::Loading::VIGNETTE_MAX_ALPHA
    );

    // ── Top strip: title (centred) + coins (top-right), same baseline ────────
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
                             canOpenPack ? Theme::BTN_START : Theme::BTN_DISABLED,
                             canOpenPack ? Theme::BTN_BORDER : Theme::BTN_DISABLED_BORDER,
                             canOpenPack ? Theme::BTN_TEXT : Theme::BTN_DISABLED_TEXT,
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

    // ── Dynamic card sizing (fit all 5 cards on screen) ──────────────────────
    const int sidePad = Theme::PackOpening::CARD_SIDE_PADDING;
    const int cardGap = Theme::PackOpening::CARD_GAP;
    const int usableW  = screenW - 2 * sidePad;
    const int cardW    = (usableW - (PackSize - 1) * cardGap) / PackSize;
    const int cardH    = cardW * 3 / 2;
    const int totalW   = PackSize * cardW + (PackSize - 1) * cardGap;
    const int startX   = (screenW - totalW) / 2;

    // ── Layout: summary panel above cards ─────────────────────────────────────
    const int badgeH    = Theme::PackOpening::CARD_BADGE_HEIGHT;
    const int summaryH  = Theme::PackOpening::SUMMARY_HEIGHT;
    const int summaryGap = Theme::PackOpening::SUMMARY_GAP;
    const int groupH    = summaryH + summaryGap + cardH + Theme::PackOpening::SUMMARY_CARD_GAP + badgeH;

    const int topClear    = headerY + Theme::PackOpening::HEADER_CLEARANCE;
    const int bottomClear = backButton.y;
    const int groupY      = topClear + std::max(0, (bottomClear - topClear - groupH) / 2);

    const int summaryY = groupY;
    const int cardY = summaryY + summaryH + summaryGap;

    RenderText textRenderer;

    for (int i = 0; i < static_cast<int>(lastOpenedCards.size()); ++i) {
        const auto& result = lastOpenedCards[i];
        SDL_Rect cardRect{startX + i * (cardW + cardGap), cardY, cardW, cardH};

        if (i >= revealedCount) {
            RenderCard::drawCardBack(renderer, cardRect);
            continue;
        }

        if (result.cardIndex >= 0 && result.cardIndex < static_cast<int>(availableCards.size()) && availableCards[result.cardIndex]) {
            RenderCard::drawPreview(renderer, textRenderer, *availableCards[result.cardIndex], cardRect, uiFonts.small, uiFonts.large);
        } else {
            RenderCard::drawCardBack(renderer, cardRect);
        }

        const std::string qtyStr = std::to_string(result.resultingCopies) + "/" + std::to_string(MaxCardCopies);
        const int chipW = Theme::PackOpening::QTY_CHIP_WIDTH;
        const int chipH = Theme::PackOpening::QTY_CHIP_HEIGHT;
        SDL_Rect chip{cardRect.x + cardRect.w - chipW - Theme::PackOpening::QTY_CHIP_MARGIN, cardRect.y + Theme::PackOpening::QTY_CHIP_MARGIN, chipW, chipH};
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, Theme::PackOpening::QTY_CHIP_BG.r,
                               Theme::PackOpening::QTY_CHIP_BG.g,
                               Theme::PackOpening::QTY_CHIP_BG.b,
                               Theme::PackOpening::QTY_CHIP_BG.a);
        SDL_RenderFillRect(renderer, &chip);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        drawCenteredText(renderer, qtyStr, uiFonts.small, Theme::PackOpening::QTY_TEXT, chip);

        const SDL_Color badgeFill   = result.refunded ? Theme::PackOpening::DUPLICATE_FILL
                                                       : Theme::PackOpening::NEW_FILL;
        const std::string badgeLabel = result.refunded
            ? ("DUPE  +" + std::to_string(RefundCoinsPerExtra) + "c")
            : "NEW!";
        SDL_Rect badge{cardRect.x, cardRect.y + cardRect.h + Theme::PackOpening::SUMMARY_CARD_GAP, cardRect.w, badgeH};
        SDL_SetRenderDrawColor(renderer, badgeFill.r, badgeFill.g, badgeFill.b, 255);
        SDL_RenderFillRect(renderer, &badge);
        drawCenteredText(renderer, badgeLabel, uiFonts.small, Theme::PackOpening::BADGE_TEXT, badge);
    }

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

    if (hoveredOpenedCard >= 0 && hoveredOpenedCard < revealedCount && hoveredOpenedCard < static_cast<int>(lastOpenedCards.size())) {
        const OpenedCardResult& hoveredResult = lastOpenedCards[hoveredOpenedCard];
        if (hoveredResult.cardIndex >= 0 && hoveredResult.cardIndex < static_cast<int>(availableCards.size())) {
            const std::unique_ptr<Card>& hoveredCard = availableCards[hoveredResult.cardIndex];
            if (hoveredCard) {
                const SDL_Rect hoveredRect{
                    startX + hoveredOpenedCard * (cardW + cardGap),
                    cardY,
                    cardW,
                    cardH
                };

                int previewW = static_cast<int>(std::round(cardW * Theme::PackOpening::HOVER_PREVIEW_SCALE));
                const int maxPreviewW = screenW - (Theme::PackOpening::PREVIEW_MARGIN * 2);
                previewW = std::min(previewW, maxPreviewW);
                const int previewH = static_cast<int>(std::round(previewW * Theme::PREVIEW_ASPECT_RATIO));

                int previewX = hoveredRect.x + (hoveredRect.w - previewW) / 2;
                int previewY = hoveredRect.y + (hoveredRect.h - previewH) / 2;

                if (previewX < Theme::PackOpening::PREVIEW_MARGIN) {
                    previewX = Theme::PackOpening::PREVIEW_MARGIN;
                }
                if (previewX + previewW > screenW - Theme::PackOpening::PREVIEW_MARGIN) {
                    previewX = screenW - Theme::PackOpening::PREVIEW_MARGIN - previewW;
                }
                if (previewY < Theme::PackOpening::PREVIEW_MARGIN) {
                    previewY = Theme::PackOpening::PREVIEW_MARGIN;
                }
                if (previewY + previewH > screenH - Theme::PackOpening::PREVIEW_MARGIN) {
                    previewY = screenH - Theme::PackOpening::PREVIEW_MARGIN - previewH;
                }

                const SDL_Rect previewRect{previewX, previewY, previewW, previewH};
                RenderCard::drawPreview(renderer, textRenderer, *hoveredCard, previewRect, uiFonts.large, titleFonts.medium);
            }
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

    const int gap = 20;
    const int groupW = backButton.w + gap + shopButton.w + gap + openPackButton.w;
    const int maxButtonH = std::max(backButton.h, std::max(shopButton.h, openPackButton.h));
    const int startX = (screenW - groupW) / 2;
    const int y = screenH - maxButtonH - Theme::PackOpening::BUTTON_BOTTOM_MARGIN;

    backButton.x = startX;
    backButton.y = y;

    shopButton.x = backButton.x + backButton.w + gap;
    shopButton.y = y;

    openPackButton.x = shopButton.x + shopButton.w + gap;
    openPackButton.y = y;
}

// ── openPack — accumulates deltas, no service calls ──────────────────────────

void PackOpening::openPack(Game& game) {
    if (availableCards.empty()) {
        statusMessage = "No cards available to open.";
        return;
    }

    if (inventoryCopies.size() != availableCards.size()) {
        int loadedCoins = 0;
        if (!Inventory::loadInventoryAndCoinsFromService(game, availableCards, inventoryCopies, loadedCoins, MaxCardCopies)) {
            statusMessage = "Failed to load inventory for your account.";
            return;
        }
        game.setPackRefundCoins(loadedCoins);
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
            result.refunded = true;
            refundCoins += RefundCoinsPerExtra;
            result.resultingCopies = 0;
            results.push_back(result);
            continue;
        }

        if (nextInventory[cardIndex] >= MaxCardCopies) {
            result.refunded = true;
            refundCoins += RefundCoinsPerExtra;
            result.resultingCopies = nextInventory[cardIndex];
            results.push_back(result);
            continue;
        }

        result.refunded = false;
        nextInventory[cardIndex] += 1;
        result.resultingCopies = nextInventory[cardIndex];

        const int cardId = availableCards[cardIndex] ? availableCards[cardIndex]->getId() : -1;
        if (cardId > 0) {
            deltaByCardId[cardId] += 1;
        }

        results.push_back(result);
    }

    // ── Update in-memory state immediately so UI stays accurate ──────────────
    inventoryCopies = std::move(nextInventory);

    const int updatedCoins = currentCoins - PackCostCoins + refundCoins;
    game.setPackRefundCoins(updatedCoins);

    // ── Accumulate deltas for deferred flush on exit ──────────────────────────
    for (const auto& pair : deltaByCardId) {
        pendingInventoryDelta[pair.first] += pair.second;
    }
    pendingCoinDelta += (updatedCoins - currentCoins); // net change this pack

    // ── Update UI ─────────────────────────────────────────────────────────────
    lastOpenedCards = std::move(results);
    lastRefundCoins = refundCoins;
    revealedCount = 0;
    revealStartTick = SDL_GetTicks();
    hoveredOpenedCard = -1;

    int cardsAdded = 0;
    for (const auto& pair : deltaByCardId) {
        cardsAdded += pair.second;
    }

    statusMessage = "Pack opened (-" + std::to_string(PackCostCoins) + "c): +" + std::to_string(cardsAdded) + " card(s)";
    if (refundCoins > 0) {
        statusMessage += ", +" + std::to_string(refundCoins) + " coins refund.";
    }
}

// ── loadAvailableCards ────────────────────────────────────────────────────────

bool PackOpening::loadAvailableCards(const Game& game) {
    (void)game;
    if (!LoadAvailableCardsUtil::ensureAvailableCardsLoaded()) {
        availableCards.clear();
        return false;
    }

    const auto& cachedCards = LoadAvailableCardsUtil::getAvailableCards();
    availableCards.clear();
    availableCards.reserve(cachedCards.size());
    for (const auto& card : cachedCards) {
        if (card) {
            availableCards.push_back(card->clone());
        }
    }

    return !availableCards.empty();
}

// ── applyInventoryDelta ───────────────────────────────────────────────────────

bool PackOpening::applyInventoryDelta(const Game& game, const std::unordered_map<int, int>& deltaByCardId) {
    if (deltaByCardId.empty()) return true;

    const std::string host = EnvUtil::getCardsServiceHost();
    const int port = EnvUtil::getCardsServicePort();
    const std::string path = "/cards/inventories";
    const int userId = resolveUserId(game);
    if (userId <= 0) {
        return false;
    }

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
    const Uint32 now = SDL_GetTicks();
    const Uint32 elapsed = now - lastFlushTick;

    const bool hasInventoryChanges = !pendingInventoryDelta.empty();
    const bool hasCoinChanges = (pendingCoinDelta != 0);

    if (!hasInventoryChanges && !hasCoinChanges) {
        return;
    }

    const bool timeExceeded = elapsed >= FlushIntervalMs;
    const bool coinExceeded = std::abs(pendingCoinDelta) >= CoinFlushThreshold;
    const bool inventoryExceeded = getPendingInventoryOps() >= InventoryFlushThreshold;

    if (!(timeExceeded || coinExceeded || inventoryExceeded)) {
        return;
    }

    std::cout << "[PackOpening]: Flushing batch...\n";

    if (hasInventoryChanges) {
        if (!applyInventoryDelta(game, pendingInventoryDelta)) {
            std::cerr << "[PackOpening]: Failed to update Inventory delta\n";
            return; // don’t reset if failed
        }
    }

    if (hasCoinChanges) {
        if (!Inventory::updateCoinsOnService(game, game.getPackRefundCoins())) {
            std::cerr << "[PackOpening]: Failed to update coin balance\n";
            return;
        }
    }

    // Reset after successful flush
    pendingInventoryDelta.clear();
    pendingCoinDelta = 0;
    lastFlushTick = now;
}
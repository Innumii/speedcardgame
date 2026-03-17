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
#include <random>
#include <sstream>
#include <unordered_map>

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

void PackOpening::enter(Game& game) {
    updateLayout(game.getRenderer());
    statusMessage.clear();
    lastOpenedCards.clear();
    lastRefundCoins = 0;

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
}

void PackOpening::handleEvents(Game& game, const SDL_Event& event) {
    updateLayout(game.getRenderer());

    if (event.type == SDL_QUIT) {
        game.setNextState(GameState::Quit);
        return;
    }

    if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
        game.setNextState(GameState::Title);
        return;
    }

    if (event.type == SDL_MOUSEMOTION) {
        const int x = event.motion.x;
        const int y = event.motion.y;
        const bool canOpenPack = game.getPackRefundCoins() >= PackCostCoins;
        backHovered = (x >= backButton.x && x <= backButton.x + backButton.w &&
                       y >= backButton.y && y <= backButton.y + backButton.h);
        openHovered = (x >= openPackButton.x && x <= openPackButton.x + openPackButton.w &&
                       y >= openPackButton.y && y <= openPackButton.y + openPackButton.h &&
                       canOpenPack);
    }

    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        const int x = event.button.x;
        const int y = event.button.y;

        if (x >= backButton.x && x <= backButton.x + backButton.w &&
            y >= backButton.y && y <= backButton.y + backButton.h) {
            game.setNextState(GameState::Title);
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

void PackOpening::update(Game& game) {
    (void)game;
    if (!lastOpenedCards.empty() && revealedCount < static_cast<int>(lastOpenedCards.size())) {
        const Uint32 now = SDL_GetTicks();
        const int elapsed = static_cast<int>(now - revealStartTick);
        revealedCount = std::min(static_cast<int>(lastOpenedCards.size()),
                                  elapsed / CardRevealIntervalMs + 1);
    }
}

void PackOpening::render(Game& game) {
    SDL_Renderer* renderer = game.getRenderer();
    if (!renderer) return;

    updateLayout(renderer);

    const auto& uiFonts = game.getUIFonts();
    const auto& titleFonts = game.getTitleFonts();

    int screenW = 800;
    int screenH = 600;
    SDL_GetRendererOutputSize(renderer, &screenW, &screenH);

    SDL_SetRenderDrawColor(renderer, Theme::BG.r, Theme::BG.g, Theme::BG.b, 255);
    SDL_RenderClear(renderer);

    // ── Top strip: title (centred) + coins (top-right), same baseline ───────
    const int headerY = 14;
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

    // ── Buttons ─────────────────────────────────────────────────────────────
    const bool canOpenPack = game.getPackRefundCoins() >= PackCostCoins;

    RenderButton::drawButton(renderer, backButton, "Back to Title", uiFonts.large,
                             Theme::BTN_QUIT, Theme::BTN_BORDER, Theme::BTN_TEXT, backHovered);
    RenderButton::drawButton(renderer, openPackButton, "Open Pack (100c)", uiFonts.large,
                             canOpenPack ? Theme::BTN_START : Theme::BTN_DISABLED,
                             canOpenPack ? Theme::BTN_BORDER : Theme::BTN_DISABLED_BORDER,
                             canOpenPack ? Theme::BTN_TEXT : Theme::BTN_DISABLED_TEXT,
                             canOpenPack && openHovered);

    // ── Empty state ──────────────────────────────────────────────────────────
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
    const int sidePad = 24;
    const int cardGap = 10;
    const int usableW  = screenW - 2 * sidePad;
    const int cardW    = (usableW - (PackSize - 1) * cardGap) / PackSize;
    const int cardH    = cardW * 3 / 2;
    const int totalW   = PackSize * cardW + (PackSize - 1) * cardGap;
    const int startX   = (screenW - totalW) / 2;

    // ── Layout: summary panel above cards ────────────────────────────────────
    const int badgeH    = 26;
    const int summaryH  = 42;
    const int summaryGap = 10;
    const int groupH    = summaryH + summaryGap + cardH + 4 + badgeH;

    // Vertically centre the group between header strip and buttons
    const int topClear    = headerY + 44;   // a bit below the title row
    const int bottomClear = backButton.y;   // top of buttons
    const int groupY      = topClear + std::max(0, (bottomClear - topClear - groupH) / 2);

    const int summaryY = groupY;
    const int cardY = summaryY + summaryH + summaryGap;

    RenderText textRenderer;

    for (int i = 0; i < static_cast<int>(lastOpenedCards.size()); ++i) {
        const auto& result = lastOpenedCards[i];
        SDL_Rect cardRect{startX + i * (cardW + cardGap), cardY, cardW, cardH};

        if (i >= revealedCount) {
            // Not revealed yet — show card back
            RenderCard::drawCardBack(renderer, cardRect);
            continue;
        }

        // ── Draw card face ───────────────────────────────────────────────────
        if (result.cardIndex >= 0 && result.cardIndex < static_cast<int>(availableCards.size()) && availableCards[result.cardIndex]) {
            RenderCard::drawPreview(renderer, textRenderer, *availableCards[result.cardIndex], cardRect, uiFonts.small, uiFonts.large);
        } else {
            RenderCard::drawCardBack(renderer, cardRect);
        }

        // ── Qty chip (top-right corner of card) ──────────────────────────────
        const std::string qtyStr = std::to_string(result.resultingCopies) + "/" + std::to_string(MaxCardCopies);
        const int chipW = 34;
        const int chipH = 18;
        SDL_Rect chip{cardRect.x + cardRect.w - chipW - 2, cardRect.y + 2, chipW, chipH};
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, Theme::PackOpening::QTY_CHIP_BG.r,
                               Theme::PackOpening::QTY_CHIP_BG.g,
                               Theme::PackOpening::QTY_CHIP_BG.b,
                               Theme::PackOpening::QTY_CHIP_BG.a);
        SDL_RenderFillRect(renderer, &chip);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        drawCenteredText(renderer, qtyStr, uiFonts.small, Theme::PackOpening::QTY_TEXT, chip);

        // ── Status badge (coloured strip below card) ─────────────────────────
        const SDL_Color badgeFill   = result.refunded ? Theme::PackOpening::DUPLICATE_FILL
                                                       : Theme::PackOpening::NEW_FILL;
        const std::string badgeLabel = result.refunded
            ? ("DUPE  +" + std::to_string(RefundCoinsPerExtra) + "c")
            : "NEW!";
        SDL_Rect badge{cardRect.x, cardRect.y + cardRect.h + 4, cardRect.w, badgeH};
        SDL_SetRenderDrawColor(renderer, badgeFill.r, badgeFill.g, badgeFill.b, 255);
        SDL_RenderFillRect(renderer, &badge);
        drawCenteredText(renderer, badgeLabel, uiFonts.small, Theme::PackOpening::BADGE_TEXT, badge);
    }

    // ── Summary panel (above cards) ────────────────────────────────────────
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
}

void PackOpening::updateLayout(SDL_Renderer* renderer) {
    int screenW = 800;
    int screenH = 600;
    if (renderer) {
        SDL_GetRendererOutputSize(renderer, &screenW, &screenH);
    }

    const int gap = 20;
    const int groupW = backButton.w + gap + openPackButton.w;
    const int maxButtonH = std::max(backButton.h, openPackButton.h);
    const int startX = (screenW - groupW) / 2;
    const int y = screenH - maxButtonH - 24;

    backButton.x = startX;
    backButton.y = y;

    openPackButton.x = backButton.x + backButton.w + gap;
    openPackButton.y = y;
}

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

    if (!deltaByCardId.empty() && !applyInventoryDelta(game, deltaByCardId)) {
        statusMessage = "Failed to open pack: inventory update error.";
        return;
    }

    const int updatedCoins = currentCoins - PackCostCoins + refundCoins;
    if (!Inventory::updateCoinsOnService(game, updatedCoins)) {
        statusMessage = "Pack opened but failed to sync coins.";
        return;
    }
    game.setPackRefundCoins(updatedCoins);

    inventoryCopies = std::move(nextInventory);
    lastOpenedCards = std::move(results);
    lastRefundCoins = refundCoins;
    revealedCount = 0;
    revealStartTick = SDL_GetTicks();

    int cardsAdded = 0;
    for (const auto& pair : deltaByCardId) {
        cardsAdded += pair.second;
    }

    statusMessage = "Pack opened (-" + std::to_string(PackCostCoins) + "c): +" + std::to_string(cardsAdded) + " card(s)";
    if (refundCoins > 0) {
        statusMessage += ", +" + std::to_string(refundCoins) + " coins refund.";
    }
}

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

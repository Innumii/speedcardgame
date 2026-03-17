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

    const bool canOpenPack = game.getPackRefundCoins() >= PackCostCoins;
    const SDL_Color disabledFill{92, 92, 92, 255};
    const SDL_Color disabledBorder{140, 140, 140, 255};
    const SDL_Color disabledText{190, 190, 190, 255};

    RenderButton::drawButton(renderer, backButton, "Back to Title", uiFonts.large,
                             Theme::BTN_QUIT, Theme::BTN_BORDER, Theme::BTN_TEXT, backHovered);
    RenderButton::drawButton(renderer, openPackButton, "Open Pack (100c)", uiFonts.large,
                             canOpenPack ? Theme::BTN_START : disabledFill,
                             canOpenPack ? Theme::BTN_BORDER : disabledBorder,
                             canOpenPack ? Theme::BTN_TEXT : disabledText,
                             canOpenPack && openHovered);

    RenderText::drawText(renderer, "Pack Opening", titleFonts.large,
                         Theme::BANNER_TEXT, 40, 100);

    const std::string coinsText = "Coins: " + std::to_string(game.getPackRefundCoins());
    RenderText::drawText(renderer, coinsText, uiFonts.large, Theme::BTN_TEXT, 40, 160);

    if (!statusMessage.empty()) {
        RenderText::drawText(renderer, statusMessage, uiFonts.small, Theme::BTN_TEXT, 40, 200);
    }

    if (lastOpenedCards.empty()) {
        RenderText::drawText(renderer, "Open a pack to add cards to inventory.", uiFonts.small, Theme::BTN_TEXT, 40, 240);
        return;
    }

    const int cardW = 170;
    const int cardH = 240;
    const int gap = 16;
    const int totalW = (PackSize * cardW) + ((PackSize - 1) * gap);
    const int startX = (screenW - totalW) / 2;
    const int y = 220;

    RenderText textRenderer;

    for (int i = 0; i < static_cast<int>(lastOpenedCards.size()); ++i) {
        const auto& result = lastOpenedCards[i];
        SDL_Rect cardRect{startX + i * (cardW + gap), y, cardW, cardH};

        std::string name = "Unknown";
        if (result.cardIndex >= 0 && result.cardIndex < static_cast<int>(availableCards.size()) && availableCards[result.cardIndex]) {
            name = availableCards[result.cardIndex]->getName();
            RenderCard::drawPreview(renderer, textRenderer, *availableCards[result.cardIndex], cardRect, uiFonts.small, uiFonts.large);
        } else {
            RenderCard::drawCardBack(renderer, cardRect);
        }

        const std::string qtyLine = "Qty: " + std::to_string(result.resultingCopies) + "/" + std::to_string(MaxCardCopies);
        const std::string statusLine = result.refunded
            ? "Duplicate (+" + std::to_string(RefundCoinsPerExtra) + "c)"
            : "Added";

        SDL_Rect qtyBox{cardRect.x, cardRect.y + cardRect.h + 4, cardRect.w, 22};
        SDL_Rect statusBox{cardRect.x, cardRect.y + cardRect.h + 24, cardRect.w, 22};
        drawCenteredText(renderer, qtyLine, uiFonts.small, Theme::BTN_TEXT, qtyBox);
        drawCenteredText(renderer, statusLine, uiFonts.small, Theme::BTN_TEXT, statusBox);
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

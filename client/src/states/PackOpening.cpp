#include "states/PackOpening.hpp"

#include "core/Game.hpp"
#include "objects/CreatureCard.h"
#include "objects/SpellCard.h"
#include "render/RenderCard.hpp"
#include "render/RenderButton.hpp"
#include "render/RenderText.hpp"
#include "render/Theme.hpp"
#include "utils/HttpUtil.hpp"
#include "utils/JsonUtil.hpp"
#include "utils/EnvUtil.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <random>
#include <sstream>
#include <unordered_map>

namespace {
    std::string toLower(std::string value) {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return value;
    }

    bool parseCsvLine(const std::string& line, std::vector<std::string>& out) {
        out.clear();
        std::string field;
        bool inQuotes = false;
        for (std::size_t i = 0; i < line.size(); ++i) {
            char c = line[i];
            if (c == '"') {
                if (inQuotes && i + 1 < line.size() && line[i + 1] == '"') {
                    field.push_back('"');
                    ++i;
                } else {
                    inQuotes = !inQuotes;
                }
                continue;
            }
            if (c == ',' && !inQuotes) {
                out.push_back(field);
                field.clear();
                continue;
            }
            field.push_back(c);
        }
        out.push_back(field);
        return true;
    }

    bool tryOpenCsv(const std::string& path, std::ifstream& stream) {
        stream.open(path);
        return stream.is_open();
    }

    bool extractCardsObjectForUser(const std::string& json, int userId, std::string& outCards) {
        std::size_t pos = 0;
        while (pos < json.size()) {
            if (json[pos] != '{') {
                ++pos;
                continue;
            }

            std::size_t objEnd = 0;
            if (!JsonUtil::findMatchingBrace(json, pos, objEnd)) {
                return false;
            }

            const std::string obj = json.substr(pos, objEnd - pos + 1);
            int uid = -1;
            JsonUtil::readJsonIntField(obj, "uid", uid);
            if (uid == userId) {
                const std::string needle = "\"cards\"";
                std::size_t cardsPos = obj.find(needle);
                if (cardsPos == std::string::npos) return false;
                cardsPos = obj.find('{', cardsPos + needle.size());
                if (cardsPos == std::string::npos) return false;
                std::size_t cardsEnd = 0;
                if (!JsonUtil::findMatchingBrace(obj, cardsPos, cardsEnd)) return false;
                if (cardsEnd <= cardsPos + 1) {
                    outCards.clear();
                    return true;
                }
                outCards = obj.substr(cardsPos + 1, cardsEnd - cardsPos - 1);
                return true;
            }

            pos = objEnd + 1;
        }

        return false;
    }

    bool parseCardsMap(const std::string& cardsJson, std::vector<std::pair<int, int>>& out) {
        out.clear();
        std::size_t pos = 0;
        while (pos < cardsJson.size()) {
            while (pos < cardsJson.size() &&
                   (std::isspace(static_cast<unsigned char>(cardsJson[pos])) || cardsJson[pos] == ',')) {
                ++pos;
            }
            if (pos >= cardsJson.size()) break;

            std::string key;
            if (!JsonUtil::parseJsonQuotedStringAt(cardsJson, pos, key)) {
                ++pos;
                continue;
            }

            while (pos < cardsJson.size() && std::isspace(static_cast<unsigned char>(cardsJson[pos]))) {
                ++pos;
            }
            if (pos >= cardsJson.size() || cardsJson[pos] != ':') {
                return false;
            }
            ++pos;

            int value = 0;
            if (!JsonUtil::parseJsonIntAt(cardsJson, pos, value)) {
                return false;
            }

            int cardId = -1;
            try {
                cardId = std::stoi(key);
            } catch (...) {
                cardId = -1;
            }

            if (cardId >= 0) {
                out.emplace_back(cardId, value);
            }
        }

        return true;
    }

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

    if (!loadInventoryFromService(game)) {
        statusMessage = "Failed to load inventory.";
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
        backHovered = (x >= backButton.x && x <= backButton.x + backButton.w &&
                       y >= backButton.y && y <= backButton.y + backButton.h);
        openHovered = (x >= openPackButton.x && x <= openPackButton.x + openPackButton.w &&
                       y >= openPackButton.y && y <= openPackButton.y + openPackButton.h);
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

    RenderButton::drawButton(renderer, backButton, "Back to Title", uiFonts.large,
                             Theme::BTN_QUIT, Theme::BTN_BORDER, Theme::BTN_TEXT, backHovered);
    RenderButton::drawButton(renderer, openPackButton, "Open Pack", uiFonts.large,
                             Theme::BTN_START, Theme::BTN_BORDER, Theme::BTN_TEXT, openHovered);

    RenderText::drawText(renderer, "Pack Opening", titleFonts.large,
                         Theme::BANNER_TEXT, 40, 100);

    const std::string coinsText = "Refund Coins: " + std::to_string(game.getPackRefundCoins());
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
    const int y = 270;

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
    if (renderer) {
        int screenH = 600;
        SDL_GetRendererOutputSize(renderer, &screenW, &screenH);
    }

    backButton.x = 30;
    backButton.y = 24;

    openPackButton.x = backButton.x + backButton.w + 20;
    openPackButton.y = backButton.y;

    if (openPackButton.x + openPackButton.w > screenW - 20) {
        openPackButton.x = screenW - openPackButton.w - 20;
    }
}

void PackOpening::openPack(Game& game) {
    if (availableCards.empty()) {
        statusMessage = "No cards available to open.";
        return;
    }

    if (inventoryCopies.size() != availableCards.size()) {
        if (!loadInventoryFromService(game)) {
            statusMessage = "Failed to load inventory for your account.";
            return;
        }
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

    inventoryCopies = std::move(nextInventory);
    game.addPackRefundCoins(refundCoins);
    lastOpenedCards = std::move(results);
    lastRefundCoins = refundCoins;

    int cardsAdded = 0;
    for (const auto& pair : deltaByCardId) {
        cardsAdded += pair.second;
    }

    statusMessage = "Pack opened: +" + std::to_string(cardsAdded) + " card(s)";
    if (refundCoins > 0) {
        statusMessage += ", +" + std::to_string(refundCoins) + " coins refund.";
    }
}

bool PackOpening::loadAvailableCards(const Game& game) {
    if (loadAvailableCardsFromService(game)) {
        return true;
    }
    return loadAvailableCardsFromCsv(game);
}

bool PackOpening::loadAvailableCardsFromService(const Game& game) {
    (void)game;
    const std::string host = EnvUtil::getServiceHost("CARDS_SERVICE", "127.0.0.1", "cards.speedcardgame.aws");
    const int port = EnvUtil::getServicePort("CARDS_SERVICE", 8082, 443);
    const std::string path = "/cardbase/cards";
    int statusCode = -1;
    std::string responseBody;
    if (!HttpUtil::sendHttp(host, port, "GET", path, "", statusCode, responseBody)) {
        return false;
    }

    std::vector<std::unique_ptr<Card>> fetchedCards;
    std::size_t pos = 0;
    while (true) {
        const std::size_t objStart = responseBody.find('{', pos);
        if (objStart == std::string::npos) break;
        const std::size_t objEnd = responseBody.find('}', objStart);
        if (objEnd == std::string::npos) break;
        const std::string obj = responseBody.substr(objStart, objEnd - objStart + 1);

        int cid = -1;
        int cost = 0;
        int value = 0;
        int power = 0;
        int toughness = 0;
        std::string name;
        std::string type;
        std::string effect;

        JsonUtil::readJsonIntField(obj, "cid", cid);
        JsonUtil::readJsonStringField(obj, "name", name);
        JsonUtil::readJsonStringField(obj, "type", type);
        JsonUtil::readJsonIntField(obj, "cost", cost);
        JsonUtil::readJsonIntField(obj, "value", value);
        JsonUtil::readJsonIntField(obj, "power", power);
        JsonUtil::readJsonIntField(obj, "toughness", toughness);
        JsonUtil::readJsonStringField(obj, "effect", effect);

        if (!name.empty()) {
            const std::string typeLower = toLower(type);
            const int manaValue = value > 0 ? value : cost;
            if (typeLower == "creature") {
                fetchedCards.push_back(std::make_unique<CreatureCard>(name, effect, manaValue, cost, power, toughness, cid));
            } else {
                fetchedCards.push_back(std::make_unique<SpellCard>(name, effect, manaValue, cost, cid));
            }
        }

        pos = objEnd + 1;
    }

    if (fetchedCards.empty()) {
        return false;
    }

    availableCards = std::move(fetchedCards);
    return true;
}

bool PackOpening::loadAvailableCardsFromCsv(const Game& game) {
    (void)game;
    const std::string envPath = EnvUtil::getEnvOrDefault("CARDS_CSV_PATH", "");
    std::ifstream file;
    if (!envPath.empty() && tryOpenCsv(envPath, file)) {
    } else if (tryOpenCsv("cards/cards.csv", file)) {
    } else if (tryOpenCsv("../cards/cards.csv", file)) {
    } else if (tryOpenCsv("../../cards/cards.csv", file)) {
    } else {
        return false;
    }

    std::string line;
    if (!std::getline(file, line)) {
        return false;
    }

    std::vector<std::unique_ptr<Card>> fetchedCards;
    std::vector<std::string> fields;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        parseCsvLine(line, fields);
        if (fields.size() < 8) continue;

        int cid = -1;
        int cost = 0;
        int value = 0;
        int power = 0;
        int toughness = 0;

        try {
            cid = std::stoi(fields[0]);
            cost = std::stoi(fields[3]);
            value = std::stoi(fields[4]);
            power = std::stoi(fields[5]);
            toughness = std::stoi(fields[6]);
        } catch (...) {
            continue;
        }

        const std::string name = fields[1];
        const std::string type = fields[2];
        const std::string effect = fields[7];

        if (name.empty()) continue;
        const std::string typeLower = toLower(type);
        const int manaValue = value > 0 ? value : cost;

        if (typeLower == "creature") {
            fetchedCards.push_back(std::make_unique<CreatureCard>(name, effect, manaValue, cost, power, toughness, cid));
        } else {
            fetchedCards.push_back(std::make_unique<SpellCard>(name, effect, manaValue, cost, cid));
        }
    }

    if (fetchedCards.empty()) {
        return false;
    }

    availableCards = std::move(fetchedCards);
    return true;
}

bool PackOpening::loadInventoryFromService(const Game& game) {
    if (availableCards.empty()) return false;

    const std::string host = EnvUtil::getServiceHost("CARDS_SERVICE", "127.0.0.1", "cards.speedcardgame.aws");
    const int port = EnvUtil::getServicePort("CARDS_SERVICE", 8082, 443);
    const int userId = resolveUserId(game);
    if (userId <= 0) {
        inventoryCopies.assign(availableCards.size(), 0);
        return false;
    }

    const std::string path = "/cardbase/inventories/" + std::to_string(userId);
    int statusCode = -1;
    std::string responseBody;
    if (!HttpUtil::sendHttp(host, port, "GET", path, "", statusCode, responseBody) || statusCode < 200 || statusCode >= 300) {
        inventoryCopies.assign(availableCards.size(), 0);
        return false;
    }

    std::string cardsObject;
    if (!JsonUtil::extractJsonObject(responseBody, "cards", cardsObject)) {
        inventoryCopies.assign(availableCards.size(), 0);
        return false;
    }

    std::string cardsJson;
    if (cardsObject.size() >= 2 && cardsObject.front() == '{' && cardsObject.back() == '}') {
        cardsJson = cardsObject.substr(1, cardsObject.size() - 2);
    } else {
        cardsJson = cardsObject;
    }

    std::vector<std::pair<int, int>> cardCounts;
    if (!parseCardsMap(cardsJson, cardCounts)) {
        inventoryCopies.assign(availableCards.size(), 0);
        return false;
    }

    std::unordered_map<int, std::size_t> cardIndexById;
    cardIndexById.reserve(availableCards.size());
    for (std::size_t i = 0; i < availableCards.size(); ++i) {
        cardIndexById.emplace(availableCards[i]->getId(), i);
    }

    inventoryCopies.assign(availableCards.size(), 0);
    for (const auto& pair : cardCounts) {
        const int cardId = pair.first;
        const int copies = pair.second;
        if (copies <= 0) continue;
        auto it = cardIndexById.find(cardId);
        if (it == cardIndexById.end()) continue;
        inventoryCopies[it->second] = std::min(copies, MaxCardCopies);
    }

    return true;
}

bool PackOpening::applyInventoryDelta(const Game& game, const std::unordered_map<int, int>& deltaByCardId) {
    if (deltaByCardId.empty()) return true;

    const std::string host = EnvUtil::getServiceHost("CARDS_SERVICE", "127.0.0.1", "cards.speedcardgame.aws");
    const int port = EnvUtil::getServicePort("CARDS_SERVICE", 8082, 443);
    const std::string path = "/cardbase/inventories";
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

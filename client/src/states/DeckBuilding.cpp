// 1. gets all cards in the game from server
// 2. if
    // user owns it, it is not greyed out
    // else greyed out and not interactable
// 3. user can add cards to their deck buy dragging them in the deck area
// 4. user can remove cards from their deck by dragging them out of the deck area
    // cards are sorted by mana similar to hearthstone

#include "states/DeckBuilding.hpp"
#include "render/RenderDeckBuilding.hpp"
#include "core/Game.hpp"
#include "core/NetworkClient.hpp"
#include "utils/JsonUtil.hpp"
#include "objects/CreatureCard.h"
#include "objects/SpellCard.h"
#include <SDL2/SDL.h>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace {
    SDL_Point getPoint(int x, int y) {
        SDL_Point point{x, y};
        return point;
    }

    bool pointInRect(const SDL_Point& point, const SDL_Rect& rect) {
        return SDL_PointInRect(&point, &rect) == SDL_TRUE;
    }

    std::string toLower(std::string value) {
        for (char& c : value) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        return value;
    }

    std::string getEnvOrDefault(const char* key, const char* fallback) {
        const char* value = std::getenv(key);
        return value ? std::string(value) : std::string(fallback);
    }

    int getEnvIntOrDefault(const char* key, int fallback) {
        const char* value = std::getenv(key);
        if (!value) return fallback;
        try {
            return std::stoi(value);
        } catch (...) {
            return fallback;
        }
    }

    int getDeckSizeLimitFromEnv() {
        const int configured = getEnvIntOrDefault("DECK_SIZE", 30);
        return configured > 0 ? configured : 30;
    }


    bool sendHttpRequest(const std::string& host, int port, const std::string& method, const std::string& path,
                         const std::string& body, std::string& responseBody) {
        NetworkClient client(NetworkClient::SocketMode::Blocking); // blocking!
        if (!client.connectTo(host, port)) {
            return false;
        }

        std::ostringstream request;
        request << method << " " << path << " HTTP/1.1\r\n";
        request << "Host: " << host << "\r\n";
        request << "Connection: close\r\n";
        if (method == "POST" || method == "PUT") {
            request << "Content-Type: application/json\r\n";
            request << "Content-Length: " << body.size() << "\r\n";
        }
        request << "\r\n";
        request << body;

        const std::string requestText = request.str();
        if (!client.send(requestText.data(), requestText.size())) {
            client.disconnect();
            return false;
        }

        std::string response;
        char buffer[4096];
        while (true) {
            int received = client.receive(buffer, sizeof(buffer));
            if (received <= 0) break;
            response.append(buffer, static_cast<std::size_t>(received));
        }
        client.disconnect();

        const std::size_t headerEnd = response.find("\r\n\r\n");
        if (headerEnd == std::string::npos) return false;
        responseBody = response.substr(headerEnd + 4);
        return true;
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
        return !out.empty();
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
}

DeckBuilding::DeckBuilding() = default;

bool DeckBuilding::refreshFromService(Game& game) {
    cardsLoadedFromService = loadAvailableCardsFromService(game);
    if (!cardsLoadedFromService) {
        cardsLoadedFromService = loadAvailableCardsFromCsv(game);
    }

    if (availableCards.empty()) {
        deckCopies.clear();
        inventoryCopies.clear();
        inventoryLoaded = false;
        return false;
    }

    const bool deckLoaded = loadDeckFromService(game);
    loadInventoryFromService(game);
    return deckLoaded;
}

void DeckBuilding::enter(Game& game) {
    refreshFromService(game);
    collectionPage = 0;
    statusMessage.clear();
    statusMessageUntil = 0;
}

void DeckBuilding::exit(Game& game) {
    (void)game;
}

void DeckBuilding::handleEvents(Game& game, const SDL_Event& event) {
    if (event.type == SDL_QUIT) {
        game.setNextState(GameState::Title);
    }

    const auto layout = buildLayout(game);
    updateMenuButtons(layout);

    // Further event handling for deck building would go here
    const bool inTitle = (event.type == SDL_MOUSEBUTTONDOWN) &&
                     (event.button.button == SDL_BUTTON_LEFT) &&
                     (event.button.x >= TitleButton.x && event.button.x <= (TitleButton.x + TitleButton.w)) &&
                     (event.button.y >= TitleButton.y && event.button.y <= (TitleButton.y + TitleButton.h));
    if (inTitle) {
        game.setNextState(GameState::Title);
        return;
    };

    const bool inSave = (event.type == SDL_MOUSEBUTTONDOWN) &&
                    (event.button.button == SDL_BUTTON_LEFT) &&
                    (event.button.x >= SaveButton.x && event.button.x <= (SaveButton.x + SaveButton.w)) &&
                    (event.button.y >= SaveButton.y && event.button.y <= (SaveButton.y + SaveButton.h));
    if (inSave) {
        if (!hasFullDeck()) {
            const int deckCount = getDeckCardCount();
            const int deckLimit = getDeckSizeLimit();
            setStatusMessage(
                "Deck size too small (" + std::to_string(deckCount) + "/" + std::to_string(deckLimit) + ").",
                2500
            );
            return;
        }
        if (saveDeckToService(game)) {
            setStatusMessage("Deck saved.", 2000);
        } else {
            setStatusMessage("Failed to save deck.", 2000);
        }
        return;
    }

    const bool inPlay = (event.type == SDL_MOUSEBUTTONDOWN) &&
                    (event.button.button == SDL_BUTTON_LEFT) &&
                    (event.button.x >= PlayButton.x && event.button.x <= (PlayButton.x + PlayButton.w)) &&
                    (event.button.y >= PlayButton.y && event.button.y <= (PlayButton.y + PlayButton.h));
    if (inPlay) {
        if (!hasFullDeck()) {
            const int deckCount = getDeckCardCount();
            const int deckLimit = getDeckSizeLimit();
            setStatusMessage(
                "Deck size too small (" + std::to_string(deckCount) + "/" + std::to_string(deckLimit) + ").",
                2500
            );
            return;
        }
        game.setPlayingDeck(buildDeck());
        game.setNextState(GameState::Playing);
        return;
    }

    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        const SDL_Point point = getPoint(event.button.x, event.button.y);

        if (layout.pageCount > 1 && pointInRect(point, layout.prevPageButton) && collectionPage > 0) {
            collectionPage -= 1;
            return;
        }
        if (layout.pageCount > 1 && pointInRect(point, layout.nextPageButton) && collectionPage < layout.pageCount - 1) {
            collectionPage += 1;
            return;
        }

        for (std::size_t i = 0; i < layout.collectionCardRects.size(); ++i) {
            if (pointInRect(point, layout.collectionCardRects[i])) {
                dragging = true;
                draggingFromDeck = false;
                if (i < layout.collectionCardIndices.size()) {
                    draggedCardIndex = layout.collectionCardIndices[i];
                } else {
                    draggedCardIndex = static_cast<int>(i);
                }
                if (getRemainingCount(draggedCardIndex) <= 0) {
                    dragging = false;
                    draggedCardIndex = -1;
                    return;
                }
                dragPos = point;
                dragOffset.x = point.x - layout.collectionCardRects[i].x;
                dragOffset.y = point.y - layout.collectionCardRects[i].y;
                return;
            }
        }

        for (std::size_t i = 0; i < layout.deckEntryRects.size(); ++i) {
            if (pointInRect(point, layout.deckEntryRects[i])) {
                dragging = true;
                draggingFromDeck = true;
                draggedCardIndex = layout.deckEntryCardIndices[i];
                dragPos = point;
                dragOffset.x = point.x - layout.deckEntryRects[i].x;
                dragOffset.y = point.y - layout.deckEntryRects[i].y;
                return;
            }
        }
    }

    if (event.type == SDL_MOUSEMOTION && dragging) {
        dragPos = getPoint(event.motion.x, event.motion.y);
    }

    if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT && dragging) {
        const SDL_Point point = getPoint(event.button.x, event.button.y);
        const bool overDeck = pointInRect(point, layout.deckArea);

        if (!draggingFromDeck && overDeck) {
            tryAddToDeck(draggedCardIndex);
        }

        if (draggingFromDeck && !overDeck) {
            tryRemoveFromDeck(draggedCardIndex);
        }

        dragging = false;
        draggingFromDeck = false;
        draggedCardIndex = -1;
    }
}

void DeckBuilding::update(Game& game) {
}

void DeckBuilding::render(Game& game) {
    SDL_Renderer* renderer = game.getRenderer();

    const auto layout = buildLayout(game);
    updateMenuButtons(layout);

    // return to title menu
    SDL_SetRenderDrawColor(renderer, 25, 25, 25, 255);
    SDL_RenderFillRect(renderer, &TitleButton);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &TitleButton);

    // render cards and deck building UI
    RenderDeckBuilding::render(*this, game);
}

DeckBuilding::Layout DeckBuilding::buildLayout(Game& game) const {
    Layout layout;

    int screenW = 800;
    int screenH = 600;
    if (SDL_Renderer* renderer = game.getRenderer()) {
        SDL_GetRendererOutputSize(renderer, &screenW, &screenH);
    }

    const int cardWidth = 140;
    const int cardHeight = 200;
    const int marginX = 16;
    const int marginY = 16;
    const int gridRows = 2;

    const int rightPadding = 20;
    const int deckGap = 12;
    const int deckWidth = 240;

    const int maxContentWidth = screenW - (rightPadding * 2);
    const int availableGridWidth = maxContentWidth - deckWidth - deckGap - 40;
    int gridCols = (availableGridWidth + marginX) / (cardWidth + marginX);
    if (gridCols < 1) gridCols = 1;
    if (gridCols > 4) gridCols = 4;
    const int maxSlots = gridCols * gridRows;

    const int gridWidth = gridCols * cardWidth + (gridCols - 1) * marginX;
    const int gridHeight = gridRows * cardHeight + (gridRows - 1) * marginY;

    const int pagerHeight = 24;
    const int pagerSpacing = 8;
    const int bottomPadding = 20;
    const int collectionHeight = gridHeight + 60;
    const int totalHeight = collectionHeight + pagerSpacing + pagerHeight;
    const int maxTop = screenH - bottomPadding - totalHeight;
    int collectionY = (screenH - totalHeight) / 2;
    if (collectionY > maxTop) {
        collectionY = maxTop;
    }
    if (collectionY < 20) collectionY = 20;

    const int totalWidth = (gridWidth + 40) + deckGap + deckWidth;
    int leftPadding = (screenW - totalWidth) / 2;
    if (leftPadding < 20) leftPadding = 20;

    layout.collectionArea = SDL_Rect{leftPadding, collectionY, gridWidth + 40, collectionHeight};

    int deckX = layout.collectionArea.x + layout.collectionArea.w + deckGap;
    int deckY = layout.collectionArea.y;
    int deckH = layout.collectionArea.h;
    if (deckX + deckWidth > screenW - rightPadding) {
        deckX = screenW - deckWidth - rightPadding;
    }
    if (deckX < layout.collectionArea.x + layout.collectionArea.w + deckGap) {
        deckX = layout.collectionArea.x + layout.collectionArea.w + deckGap;
    }
    layout.deckArea = SDL_Rect{deckX, deckY, deckWidth, deckH};

    const int totalCards = static_cast<int>(availableCards.size());
    int pageCount = (totalCards + maxSlots - 1) / maxSlots;
    if (pageCount < 1) pageCount = 1;

    int pageIndex = collectionPage;
    if (pageIndex < 0) pageIndex = 0;
    if (pageIndex > pageCount - 1) pageIndex = pageCount - 1;

    const int startIndex = pageIndex * maxSlots;
    const int remaining = totalCards - startIndex;
    const int slotCount = std::min(remaining < 0 ? 0 : remaining, maxSlots);
    layout.maxSlots = maxSlots;
    layout.pageCount = pageCount;
    layout.pageIndex = pageIndex;
    layout.collectionCardRects.reserve(slotCount);
    layout.collectionCardIndices.reserve(slotCount);
    const int startX = layout.collectionArea.x + 20;
    const int startY = layout.collectionArea.y + 40;
    for (int i = 0; i < slotCount; ++i) {
        int row = i / gridCols;
        int col = i % gridCols;
        SDL_Rect cardRect{
            startX + col * (cardWidth + marginX),
            startY + row * (cardHeight + marginY),
            cardWidth,
            cardHeight
        };
        layout.collectionCardRects.push_back(cardRect);
        layout.collectionCardIndices.push_back(startIndex + i);
    }

    const int pagerY = layout.collectionArea.y + layout.collectionArea.h + pagerSpacing;
    layout.prevPageButton = SDL_Rect{layout.collectionArea.x + 10, pagerY, 70, pagerHeight};
    layout.nextPageButton = SDL_Rect{layout.collectionArea.x + layout.collectionArea.w - 80, pagerY, 70, pagerHeight};
    layout.pageLabelRect = SDL_Rect{layout.prevPageButton.x + layout.prevPageButton.w + 8, pagerY, layout.nextPageButton.x - (layout.prevPageButton.x + layout.prevPageButton.w + 16), pagerHeight};

    const auto deckOrder = getDeckEntryOrder();
    layout.deckEntryCardIndices = deckOrder;
    layout.deckEntryRects.reserve(deckOrder.size());

    const int entryHeight = 28;
    const int entryStartY = layout.deckArea.y + 40;
    for (std::size_t i = 0; i < deckOrder.size(); ++i) {
        SDL_Rect entryRect{
            layout.deckArea.x + 10,
            entryStartY + static_cast<int>(i) * (entryHeight + 6),
            layout.deckArea.w - 20,
            entryHeight
        };
        layout.deckEntryRects.push_back(entryRect);
    }

    return layout;
}

void DeckBuilding::updateMenuButtons(const Layout& layout) {
    const int contentX = layout.collectionArea.x;
    const int contentRight = layout.deckArea.x + layout.deckArea.w;
    const int contentW = contentRight - contentX;

    const int buttonGap = 12;
    const int totalButtonsW = TitleButton.w + SaveButton.w + PlayButton.w + (buttonGap * 2);
    int startX = contentX + (contentW - totalButtonsW) / 2;
    if (startX < 20) startX = 20;

    int buttonY = layout.collectionArea.y - TitleButton.h - 12;
    if (buttonY < 20) buttonY = 20;

    TitleButton.x = startX;
    TitleButton.y = buttonY;
    SaveButton.x = startX + TitleButton.w + buttonGap;
    SaveButton.y = buttonY;
    PlayButton.x = SaveButton.x + SaveButton.w + buttonGap;
    PlayButton.y = buttonY;
}

std::vector<int> DeckBuilding::getDeckEntryOrder() const {
    std::vector<int> indices;
    indices.reserve(availableCards.size());
    for (std::size_t i = 0; i < availableCards.size(); ++i) {
        if (deckCopies[i] > 0) {
            indices.push_back(static_cast<int>(i));
        }
    }

    std::sort(indices.begin(), indices.end(), [this](int a, int b) {
        const int manaA = availableCards[a]->getManaCost();
        const int manaB = availableCards[b]->getManaCost();
        if (manaA != manaB) {
            return manaA < manaB;
        }
        return availableCards[a]->getName() < availableCards[b]->getName();
    });

    return indices;
}

void DeckBuilding::tryAddToDeck(int cardIndex) {
    if (cardIndex < 0 || cardIndex >= static_cast<int>(deckCopies.size())) return;
    if (deckCopies[cardIndex] >= MaxDeckCopies) return;
    if (getDeckCardCount() >= getDeckSizeLimit()) return;
    if (getRemainingCount(cardIndex) <= 0) return;
    deckCopies[cardIndex] += 1;
}

void DeckBuilding::tryRemoveFromDeck(int cardIndex) {
    if (cardIndex < 0 || cardIndex >= static_cast<int>(deckCopies.size())) return;
    if (deckCopies[cardIndex] <= 0) return;
    deckCopies[cardIndex] -= 1;
}

Deck DeckBuilding::buildDeck() const {
    Deck deck;
    for (std::size_t i = 0; i < availableCards.size(); ++i) {
        if (deckCopies[i] <= 0) continue;

        const Card* base = availableCards[i].get();
        if (!base) continue;

        for (int copy = 0; copy < deckCopies[i]; ++copy) {
            switch (base->getType()) {
                case CardType::Creature: {
                    const auto* creature = dynamic_cast<const CreatureCard*>(base);
                    if (!creature) break;
                    deck.addCard(std::make_unique<CreatureCard>(
                        creature->getName(),
                        creature->getText(),
                        creature->getManaValue(),
                        creature->getManaCost(),
                        creature->getPower(),
                        creature->getToughness()
                    ));
                    break;
                }
                case CardType::Spell: {
                    deck.addCard(std::make_unique<SpellCard>(
                        base->getName(),
                        base->getText(),
                        base->getManaValue(),
                        base->getManaCost()
                    ));
                    break;
                }
                default:
                    break;
            }
        }
    }

    return deck;
}

bool DeckBuilding::hasCardsInDeck() const {
    return getDeckCardCount() > 0;
}

bool DeckBuilding::hasFullDeck() const {
    return getDeckCardCount() >= getDeckSizeLimit();
}

int DeckBuilding::getDeckCardCount() const {
    int total = 0;
    for (int copies : deckCopies) {
        total += copies;
    }
    return total;
}

int DeckBuilding::getDeckSizeLimit() const {
    return getDeckSizeLimitFromEnv();
}

const std::string& DeckBuilding::getStatusMessage() const {
    return statusMessage;
}

bool DeckBuilding::isStatusMessageActive(Uint32 now) const {
    return !statusMessage.empty() && now <= statusMessageUntil;
}

void DeckBuilding::setStatusMessage(const std::string& message, Uint32 durationMs) {
    statusMessage = message;
    statusMessageUntil = SDL_GetTicks() + durationMs;
}

bool DeckBuilding::loadAvailableCardsFromService(Game& game) {
    (void)game;
    const std::string host = getEnvOrDefault("CARDS_SERVICE_HOST", "127.0.0.1");
    const int port = getEnvIntOrDefault("CARDS_SERVICE_PORT", 8082);
    const std::string path = "/cardbase/cards";

    std::string responseBody;
    if (!sendHttpRequest(host, port, "GET", path, "", responseBody)) {
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
    deckCopies.assign(availableCards.size(), 0);
    return true;
}

bool DeckBuilding::loadAvailableCardsFromCsv(Game& game) {
    (void)game;
    const std::string envPath = getEnvOrDefault("CARDS_CSV_PATH", "");
    std::ifstream file;
    if (!envPath.empty() && tryOpenCsv(envPath, file)) {
        // file opened
    } else if (tryOpenCsv("cards/cards.csv", file)) {
        // file opened
    } else if (tryOpenCsv("../cards/cards.csv", file)) {
        // file opened
    } else if (tryOpenCsv("../../cards/cards.csv", file)) {
        // file opened
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
        if (!parseCsvLine(line, fields)) continue;
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
    deckCopies.assign(availableCards.size(), 0);
    return true;
}

bool DeckBuilding::saveDeckToService(Game& game) const {
    if (!hasFullDeck()) {
        return false;
    }

    const std::string host = getEnvOrDefault("CARDS_SERVICE_HOST", "127.0.0.1");
    const int port = getEnvIntOrDefault("CARDS_SERVICE_PORT", 8082);
    const std::string path = "/cardbase/decks";
    const int userId = getEnvIntOrDefault("CARDS_SERVICE_UID", game.getPlayerId());

    std::ostringstream payload;
    payload << "{\"uid\":" << userId << ",\"cards\":{";
    bool first = true;
    for (std::size_t i = 0; i < availableCards.size(); ++i) {
        const int copies = deckCopies[i];
        if (copies <= 0) continue;
        const int cardId = availableCards[i]->getId();
        if (cardId < 0) continue;
        if (!first) payload << ',';
        payload << "\"" << cardId << "\":" << copies;
        first = false;
    }
    payload << "}}";

    std::string responseBody;
    return sendHttpRequest(host, port, "POST", path, payload.str(), responseBody);
}

bool DeckBuilding::loadInventoryFromService(Game& game) {
    if (availableCards.empty()) return false;

    const std::string host = getEnvOrDefault("CARDS_SERVICE_HOST", "127.0.0.1");
    const int port = getEnvIntOrDefault("CARDS_SERVICE_PORT", 8082);
    const std::string path = "/cardbase/inventories";
    const int userId = getEnvIntOrDefault("CARDS_SERVICE_UID", game.getPlayerId());

    std::string responseBody;
    if (!sendHttpRequest(host, port, "GET", path, "", responseBody)) {
        inventoryLoaded = false;
        inventoryCopies.assign(availableCards.size(), 0);
        return false;
    }

    std::string cardsJson;
    if (!extractCardsObjectForUser(responseBody, userId, cardsJson)) {
        inventoryLoaded = false;
        inventoryCopies.assign(availableCards.size(), 0);
        return false;
    }

    std::vector<std::pair<int, int>> cardCounts;
    if (!parseCardsMap(cardsJson, cardCounts)) {
        inventoryLoaded = false;
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
        inventoryCopies[it->second] = copies;
    }

    inventoryLoaded = true;
    return true;
}

int DeckBuilding::getInventoryCount(int cardIndex) const {
    if (cardIndex < 0 || cardIndex >= static_cast<int>(inventoryCopies.size())) return 0;
    return inventoryCopies[cardIndex];
}

int DeckBuilding::getRemainingCount(int cardIndex) const {
    if (cardIndex < 0 || cardIndex >= static_cast<int>(deckCopies.size())) return 0;
    if (!inventoryLoaded || inventoryCopies.size() != deckCopies.size()) {
        return MaxDeckCopies - deckCopies[cardIndex];
    }
    const int remaining = getInventoryCount(cardIndex) - deckCopies[cardIndex];
    return remaining > 0 ? remaining : 0;
}

bool DeckBuilding::loadDeckFromService(Game& game) {
    if (availableCards.empty()) return false;

    const std::string host = getEnvOrDefault("CARDS_SERVICE_HOST", "127.0.0.1");
    const int port = getEnvIntOrDefault("CARDS_SERVICE_PORT", 8082);
    const std::string path = "/cardbase/decks";
    const int userId = getEnvIntOrDefault("CARDS_SERVICE_UID", game.getPlayerId());

    std::string responseBody;
    if (!sendHttpRequest(host, port, "GET", path, "", responseBody)) {
        return false;
    }

    std::string cardsJson;
    if (!extractCardsObjectForUser(responseBody, userId, cardsJson)) {
        deckCopies.assign(availableCards.size(), 0);
        return false;
    }

    std::vector<std::pair<int, int>> cardCounts;
    if (!parseCardsMap(cardsJson, cardCounts)) {
        deckCopies.assign(availableCards.size(), 0);
        return false;
    }

    std::unordered_map<int, std::size_t> cardIndexById;
    cardIndexById.reserve(availableCards.size());
    for (std::size_t i = 0; i < availableCards.size(); ++i) {
        cardIndexById.emplace(availableCards[i]->getId(), i);
    }

    deckCopies.assign(availableCards.size(), 0);
    for (const auto& pair : cardCounts) {
        const int cardId = pair.first;
        const int copies = pair.second;
        if (copies <= 0) continue;
        auto it = cardIndexById.find(cardId);
        if (it == cardIndexById.end()) continue;
        const int clamped = std::min(copies, MaxDeckCopies);
        deckCopies[it->second] = clamped;
    }

    return true;
}


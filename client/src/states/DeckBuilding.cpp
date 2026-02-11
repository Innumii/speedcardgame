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
#include "objects/CreatureCard.h"
#include "objects/SpellCard.h"
#include <SDL2/SDL.h>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>

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

    bool readJsonStringField(const std::string& json, const std::string& key, std::string& out) {
        const std::string needle = "\"" + key + "\"";
        std::size_t pos = json.find(needle);
        if (pos == std::string::npos) return false;
        pos = json.find(':', pos + needle.size());
        if (pos == std::string::npos) return false;
        pos = json.find('"', pos);
        if (pos == std::string::npos) return false;
        std::size_t end = pos + 1;
        while (end < json.size()) {
            if (json[end] == '"' && json[end - 1] != '\\') break;
            ++end;
        }
        if (end >= json.size()) return false;
        out = json.substr(pos + 1, end - pos - 1);
        return true;
    }

    bool readJsonIntField(const std::string& json, const std::string& key, int& out) {
        const std::string needle = "\"" + key + "\"";
        std::size_t pos = json.find(needle);
        if (pos == std::string::npos) return false;
        pos = json.find(':', pos + needle.size());
        if (pos == std::string::npos) return false;
        ++pos;
        while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) {
            ++pos;
        }
        std::size_t end = pos;
        if (end < json.size() && json[end] == '-') {
            ++end;
        }
        while (end < json.size() && std::isdigit(static_cast<unsigned char>(json[end]))) {
            ++end;
        }
        if (end == pos) return false;
        try {
            out = std::stoi(json.substr(pos, end - pos));
        } catch (...) {
            return false;
        }
        return true;
    }

    bool sendHttpRequest(const std::string& host, int port, const std::string& method, const std::string& path,
                         const std::string& body, std::string& responseBody) {
        NetworkClient client;
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
}

DeckBuilding::DeckBuilding() {
    availableCards.push_back(std::make_unique<CreatureCard>("Blazing Drake", "Flying", 3, 3, 3, 2));
    availableCards.push_back(std::make_unique<CreatureCard>("River Sentinel", "Guard", 2, 2, 2, 3));
    availableCards.push_back(std::make_unique<CreatureCard>("Stone Golem", "Heavy", 4, 4, 4, 4));
    availableCards.push_back(std::make_unique<SpellCard>("Spark", "Deal 2 damage", 1, 1));
    availableCards.push_back(std::make_unique<SpellCard>("Frost Bind", "Freeze a foe", 2, 2));
    availableCards.push_back(std::make_unique<CreatureCard>("Night Stalker", "Stealth", 3, 3, 3, 1));
    availableCards.push_back(std::make_unique<SpellCard>("Arcane Surge", "Draw 1", 3, 3));
    availableCards.push_back(std::make_unique<CreatureCard>("Sunblade", "Charge", 5, 5, 5, 4));

    deckCopies.resize(availableCards.size(), 0);
}

void DeckBuilding::enter(Game& game) {
    cardsLoadedFromService = loadAvailableCardsFromService(game);
}

void DeckBuilding::exit(Game& game) {
    if (hasCardsInDeck()) {
        saveDeckToService(game);
    }
}

void DeckBuilding::handleEvents(Game& game, const SDL_Event& event) {
    if (event.type == SDL_QUIT) {
        game.setNextState(GameState::Title);
    }

    // Further event handling for deck building would go here
    const bool inTitle = (event.type == SDL_MOUSEBUTTONDOWN) &&
                     (event.button.button == SDL_BUTTON_LEFT) &&
                     (event.button.x >= TitleButton.x && event.button.x <= (TitleButton.x + TitleButton.w)) &&
                     (event.button.y >= TitleButton.y && event.button.y <= (TitleButton.y + TitleButton.h));
    if (inTitle) {
        game.setNextState(GameState::Title);
    };

    const bool inPlay = (event.type == SDL_MOUSEBUTTONDOWN) &&
                    (event.button.button == SDL_BUTTON_LEFT) &&
                    (event.button.x >= PlayButton.x && event.button.x <= (PlayButton.x + PlayButton.w)) &&
                    (event.button.y >= PlayButton.y && event.button.y <= (PlayButton.y + PlayButton.h));
    if (inPlay && hasCardsInDeck()) {
        game.setPlayingDeck(buildDeck());
        game.setNextState(GameState::Playing);
        return;
    }

    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        const auto layout = buildLayout(game);
        const SDL_Point point = getPoint(event.button.x, event.button.y);

        for (std::size_t i = 0; i < layout.collectionCardRects.size(); ++i) {
            if (pointInRect(point, layout.collectionCardRects[i])) {
                dragging = true;
                draggingFromDeck = false;
                draggedCardIndex = static_cast<int>(i);
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
        const auto layout = buildLayout(game);
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

    const int cardWidth = 110;
    const int cardHeight = 150;
    const int marginX = 16;
    const int marginY = 16;
    const int gridCols = 4;
    const int gridRows = 2;
    const int maxSlots = gridCols * gridRows;

    const int gridWidth = gridCols * cardWidth + (gridCols - 1) * marginX;
    const int gridHeight = gridRows * cardHeight + (gridRows - 1) * marginY;
    const int startX = 40;
    const int startY = 100;

    layout.collectionArea = SDL_Rect{startX - 20, startY - 40, gridWidth + 40, gridHeight + 60};
    layout.deckArea = SDL_Rect{screenW - 270, 90, 240, screenH - 140};

    const int slotCount = std::min(static_cast<int>(availableCards.size()), maxSlots);
    layout.collectionCardRects.reserve(slotCount);
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
    }

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
    for (int copies : deckCopies) {
        if (copies > 0) return true;
    }
    return false;
}

bool DeckBuilding::loadAvailableCardsFromService(Game& game) {
    (void)game;
    const std::string host = getEnvOrDefault("CARDS_SERVICE_HOST", "127.0.0.1");
    const int port = getEnvIntOrDefault("CARDS_SERVICE_PORT", 8080);
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

        readJsonIntField(obj, "cid", cid);
        readJsonStringField(obj, "name", name);
        readJsonStringField(obj, "type", type);
        readJsonIntField(obj, "cost", cost);
        readJsonIntField(obj, "value", value);
        readJsonIntField(obj, "power", power);
        readJsonIntField(obj, "toughness", toughness);
        readJsonStringField(obj, "effect", effect);

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

bool DeckBuilding::saveDeckToService(Game& game) const {
    const std::string host = getEnvOrDefault("CARDS_SERVICE_HOST", "127.0.0.1");
    const int port = getEnvIntOrDefault("CARDS_SERVICE_PORT", 8080);
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


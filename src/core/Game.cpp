#include "Game.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include "objects/CreatureCard.h"
#include "objects/SpellCard.h"

namespace {
    int clampPositive(int value, int maxValue) {
        if (value < 0) return 0;
        if (value > maxValue) return maxValue;
        return value;
    }
}

Game::Game(const char *title, int xpos, int ypos, int width, int height, bool fullscreen, int drawIntervalSeconds)
    : drawIntervalSeconds(drawIntervalSeconds) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) {
        throw std::runtime_error(std::string("SDL_Init failed: ") + SDL_GetError());
    }

    Uint32 flags = fullscreen ? SDL_WINDOW_FULLSCREEN : 0;

    window.reset(SDL_CreateWindow(title, xpos, ypos, width, height, flags));
    if (!window) {
        SDL_Quit();
        throw std::runtime_error(std::string("SDL_CreateWindow Failed: ") + SDL_GetError());
    }

    renderer.reset(SDL_CreateRenderer(window.get(), -1, SDL_RENDERER_ACCELERATED));
    if (!renderer) {
        SDL_Quit();
        throw std::runtime_error(std::string("SDL_CreateRenderer Failed: ") + SDL_GetError());
    }

    SDL_SetRenderDrawBlendMode(renderer.get(), SDL_BLENDMODE_BLEND);

    populateDeck();
    deck.shuffle();
    lastDrawTick = SDL_GetTicks();

    isRunning = true;
}

Game::~Game() {
    clean();
}

void Game::setState(GameState newState) {
    state = newState;
    if (state == GameState::Quit || state == GameState::GameOver) {
        isRunning = false;
    }
}

GameState Game::getState() const {
    return state;
}

SDL_Renderer* Game::getRenderer() const {
    return renderer.get();
}

void Game::handleEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            setState(GameState::Quit);
            continue;
        }

        switch (state) {
            case GameState::Title:
                titleState.handleEvents(*this, event);
                break;
            case GameState::Playing:
            case GameState::GameOver:
            case GameState::Connecting:
            case GameState::Waiting:
            default:
                break;
        }
    }
}

void Game::update() {
    if (state == GameState::Quit || !isRunning) {
        isRunning = false;
        return;
    }

    if (state == GameState::Playing) {
        updatePlaying();
    }
}

void Game::render() {
    SDL_SetRenderDrawColor(renderer.get(), 30, 30, 30, 255);
    SDL_RenderClear(renderer.get());

    switch (state) {
        case GameState::Title:
            titleState.render(*this);
            break;
        case GameState::Playing:
            renderPlaying();
            break;
        case GameState::GameOver:
            break;
        default:
            break;
    }

    SDL_RenderPresent(renderer.get());
}

void Game::clean() {
    SDL_Quit();
    isRunning = false;
}

bool Game::running() const {
    return isRunning;
}

void Game::populateDeck() {
    for (int i = 0; i < 20; ++i) {
        deck.addCard(std::make_unique<CreatureCard>("Goblin", "A small but angry creature", 1, 1, 1, 1));
        deck.addCard(std::make_unique<SpellCard>("Fireball", "Deal 3 damage", 2, 2));
    }
}

void Game::updatePlaying() {
    const Uint32 now = SDL_GetTicks();
    const Uint32 intervalMs = static_cast<Uint32>(drawIntervalSeconds * 1000);

    if (now - lastDrawTick >= intervalMs) {
        if (!player.handFull()) {
            player.drawCard(deck);
            logLatestDraw();
        }
        lastDrawTick = now;
    }

    if (player.isDead()) {
        setState(GameState::GameOver);
    }
}

void Game::renderPlaying() {
    renderHealthBar();
    renderDeckIndicator();

    const int cardWidth = 120;
    const int spacing = 20;
    const int startX = 50;
    const int startY = 360;

    for (std::size_t i = 0; i < player.hand.size(); ++i) {
        int x = startX + static_cast<int>(i) * (cardWidth + spacing);
        renderCardWidget(*player.hand[i], x, startY);
    }
}

void Game::renderHealthBar() {
    SDL_Renderer* r = renderer.get();

    const int barWidth = 220;
    const int barHeight = 24;
    SDL_Rect back{20, 20, barWidth, barHeight};

    SDL_SetRenderDrawColor(r, 60, 60, 60, 255);
    SDL_RenderFillRect(r, &back);

    const int clampedHealth = clampPositive(player.health, 100);
    SDL_Rect healthRect{20, 20, static_cast<int>(barWidth * clampedHealth / 100.0f), barHeight};
    SDL_SetRenderDrawColor(r, 70, 200, 70, 255);
    SDL_RenderFillRect(r, &healthRect);

    SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
    SDL_RenderDrawRect(r, &back);

    // fatigue indicator
    SDL_Rect fatigue{20, 20 + barHeight + 6, clampPositive(player.fatigueDamage * 10, 200), 6};
    SDL_SetRenderDrawColor(r, 200, 80, 80, 255);
    SDL_RenderFillRect(r, &fatigue);
}

void Game::renderDeckIndicator() {
    SDL_Renderer* r = renderer.get();

    const int baseHeight = 140;
    const int deckCount = clampPositive(deck.size(), 60);
    const int height = std::max(6, deckCount * 2);
    SDL_Rect deckRect{270, 20 + (baseHeight - height), 24, height};

    SDL_SetRenderDrawColor(r, 80, 160, 200, 255);
    SDL_RenderFillRect(r, &deckRect);
    SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
    SDL_RenderDrawRect(r, &deckRect);
}

void Game::renderCardWidget(const Card& card, int x, int y) {
    SDL_Renderer* r = renderer.get();
    const int cardWidth = 120;
    const int cardHeight = 180;

    SDL_Rect cardRect{x, y, cardWidth, cardHeight};

    if (card.getType() == CardType::Creature) {
        SDL_SetRenderDrawColor(r, 200, 220, 200, 255);
    } else {
        SDL_SetRenderDrawColor(r, 200, 205, 230, 255);
    }
    SDL_RenderFillRect(r, &cardRect);

    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    SDL_RenderDrawRect(r, &cardRect);

    const int costUnits = clampPositive(card.getManaCost(), 12);
    SDL_Rect costBar{x + 10, y + 10, costUnits * 8, 8};
    SDL_SetRenderDrawColor(r, 60, 100, 200, 255);
    SDL_RenderFillRect(r, &costBar);

    const int valueUnits = std::max(1, clampPositive(card.getManaValue(), 12));
    SDL_Rect valueBar{x + 10, y + 24, valueUnits * 8, 8};
    SDL_SetRenderDrawColor(r, 80, 200, 120, 255);
    SDL_RenderFillRect(r, &valueBar);

    if (card.getType() == CardType::Creature) {
        const auto* creature = dynamic_cast<const CreatureCard*>(&card);
        if (creature) {
            SDL_Rect powerBar{x + 10, y + cardHeight - 24, clampPositive(creature->getPower(), 12) * 8, 8};
            SDL_Rect toughBar{x + 10, y + cardHeight - 10, clampPositive(creature->getToughness(), 12) * 8, 8};

            SDL_SetRenderDrawColor(r, 200, 80, 60, 255);
            SDL_RenderFillRect(r, &powerBar);

            SDL_SetRenderDrawColor(r, 60, 160, 200, 255);
            SDL_RenderFillRect(r, &toughBar);
        }
    }
}

void Game::logLatestDraw() {
    if (player.hand.empty()) return;
    if (player.hand.size() == lastLoggedHandSize) return;

    const Card* card = player.hand.back().get();
    std::cout << "Drew: " << card->getName()
              << " | cost " << card->getManaCost()
              << " | value " << card->getManaValue();

    if (card->getType() == CardType::Creature) {
        const auto* creature = dynamic_cast<const CreatureCard*>(card);
        if (creature) {
            std::cout << " | stats " << creature->getPower() << '/' << creature->getToughness();
        }
    }

    std::cout << '\n';
    lastLoggedHandSize = player.hand.size();
}
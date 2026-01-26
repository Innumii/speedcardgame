#include "Game.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>

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

    isRunning = true;
}

Game::~Game() {
    clean();
}

void Game::setState(GameState newState) {
    state = newState;
    if (state == GameState::Playing && !playingSetup) {
        playingState.setup(*this);
        playingSetup = true;
    }
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
                playingState.handleEvent(*this, event);
                break;
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
        playingState.update(*this);
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
            playingState.render(*this);
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


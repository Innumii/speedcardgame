#include "core/Game.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <fstream>

namespace {
    int clampPositive(int value, int maxValue) {
        if (value < 0) return 0;
        if (value > maxValue) return maxValue;
        return value;
    }
}

//helper functions
bool isWSL() {
    // Check env variable first
    if (std::getenv("WSL_DISTRO_NAME")) return true;

    // Fallback to osrelease
    std::ifstream f("/proc/sys/kernel/osrelease");
    if (!f) return false;
    std::string line;
    std::getline(f, line);
    for (auto &c : line) c = tolower(c);
    return line.find("microsoft") != std::string::npos;
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

    bool forceSoftware = true;
#ifdef _WIN32
    // Accelerated is safe on Windows
    forceSoftware = false;
#else
    // On Linux/WSL: check environment
    if (isWSL()) {
        std::cout << "WSL detected: forcing software renderer\n";
        forceSoftware = true;
    }
#endif

    if (!forceSoftware) {
        renderer.reset(SDL_CreateRenderer(window.get(), -1, SDL_RENDERER_ACCELERATED));
        if (!renderer) {
            SDL_Quit();
            throw std::runtime_error(std::string("SDL_CreateRenderer Failed: ") + SDL_GetError());
        }
    } else {
        renderer.reset(SDL_CreateRenderer(window.get(), -1, SDL_RENDERER_SOFTWARE));
        if (!renderer) {
            SDL_Quit();
            throw std::runtime_error(std::string("SDL_CreateRenderer Failed: ") + SDL_GetError());
        }
    }
    

    SDL_SetRenderDrawBlendMode(renderer.get(), SDL_BLENDMODE_BLEND);

    SDL_RaiseWindow(window.get());
    SDL_SetWindowInputFocus(window.get());

    isRunning = true;
}

Game::~Game() {
    clean();
}

void Game::getRemoteDeckHandSize(std::size_t remoteDeckSize, std::size_t remoteHandSize) {
    this->remoteDeckSize = remoteDeckSize;
    this->remoteHandSize = remoteHandSize;
}

void Game::commitStateChange() {
    if (nextState != state) {
        state = nextState;

        if (state == GameState::Playing && !playingSetup) {
            playingState.setup(*this);
            playingSetup = true;
        }
        if (state == GameState::Quit || state == GameState::GameOver) {
            isRunning = false;
        }
    }
    
}

void Game::setNextState(GameState newState) {
    nextState = newState;
}

GameState Game::getState() const {
    return state;
}

GameState Game::getNextState() const {
    return nextState;
}

SDL_Renderer* Game::getRenderer() const {
    return renderer.get();
}

NetworkClient& Game::getNetworkClient() {
    return netClient;
}


void Game::handleEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            setNextState(GameState::Quit);
            continue;
        }

        switch (state) {
            case GameState::Title:
                titleState.handleEvents(*this, event);
                break;
            case GameState::DeckBuilding:
                deckBuildingState.handleEvents(*this, event);
                break;
            case GameState::Playing:
                playingState.handleEvents(*this, event);
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

    switch (state) {
        case GameState::Playing:
            playingState.update(*this);
            break;
        case GameState::DeckBuilding:
            deckBuildingState.update(*this);
            break;
        default:
            break;
    }
}

void Game::render() {
    SDL_SetRenderDrawColor(renderer.get(), 30, 30, 30, 255);
    SDL_RenderClear(renderer.get());

    switch (state) {
        case GameState::Title:
            titleState.render(*this);
            break;
        case GameState::DeckBuilding:
            deckBuildingState.render(*this);
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


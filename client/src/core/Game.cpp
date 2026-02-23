#include "core/Game.hpp"
#include "render/RenderCard.hpp"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <stdexcept>
#include <fstream>
#include <utility>
#include <SDL2/SDL_image.h>

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
    std::transform(line.begin(), line.end(), line.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return line.find("microsoft") != std::string::npos;
}

Game::Game(const char *title, int xpos, int ypos, int width, int height, bool fullscreen, int drawIntervalSeconds)
    : drawIntervalSeconds(drawIntervalSeconds) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) {
        throw std::runtime_error(std::string("SDL_Init failed: ") + SDL_GetError());
    }

    Uint32 flags = fullscreen ? SDL_WINDOW_FULLSCREEN : 0;
    flags |= SDL_WINDOW_RESIZABLE;

    window.reset(SDL_CreateWindow(title, xpos, ypos, width, height, flags));
    if (!window) {
        SDL_Quit();
        throw std::runtime_error(std::string("SDL_CreateWindow Failed: ") + SDL_GetError());
    }

    bool useSoftwareRenderer = true;
#ifdef _WIN32
    // Accelerated is safe on Windows
    useSoftwareRenderer = false;
#else
    if (isWSL()) {
        std::cout << "WSL detected: forcing software renderer\n";
    }
#endif

    if (useSoftwareRenderer) {
        renderer.reset(SDL_CreateRenderer(window.get(), -1, SDL_RENDERER_SOFTWARE));
        if (!renderer) {
            SDL_Quit();
            throw std::runtime_error(std::string("SDL_CreateRenderer Failed: ") + SDL_GetError());
        }
    } else {
        renderer.reset(SDL_CreateRenderer(window.get(), -1, SDL_RENDERER_ACCELERATED));
        if (!renderer) {
            SDL_Quit();
            throw std::runtime_error(std::string("SDL_CreateRenderer Failed: ") + SDL_GetError());
        }
    }
    

    SDL_SetRenderDrawBlendMode(renderer.get(), SDL_BLENDMODE_BLEND);

    const int imgFlags = IMG_INIT_PNG | IMG_INIT_JPG;
    if ((IMG_Init(imgFlags) & imgFlags) == 0) {
        SDL_Quit();
        throw std::runtime_error(std::string("IMG_Init failed: ") + IMG_GetError());
    }

    SDL_RaiseWindow(window.get());
    SDL_SetWindowInputFocus(window.get());

    //loading fonts
    titleFonts = RenderText::loadFonts("assets/Cinzel/Cinzel-VariableFont_wght.ttf",   12, 10, 48);
    uiFonts    = RenderText::loadFonts("assets/Rajdhani/Rajdhani-SemiBold.ttf", 16, 12, 22);

    loginState.enter(*this);

    isRunning = true;
}

Game::~Game() {
    std::cout << "cleaning up!\n";
    clean();
    std::cout << "Goodbye!~\n";
}

//check this again ltr
void Game::commitStateChange() {
    if (nextState != state) {
        const GameState previousState = state;
        state = nextState;

        if (previousState == GameState::DeckBuilding) {
            deckBuildingState.exit(*this);
        }

        if (previousState == GameState::Login && state != GameState::Login) {
            loginState.exit(*this);
        }

        if (previousState == GameState::Register && state != GameState::Register) {
            registerState.exit(*this);
        }

        if (previousState == GameState::Playing && state != GameState::Playing) {
            playingSetup = false;
        }

        // if (state == GameState::Connecting) {
        //     connectingState.reset();
        // }
        
        state = nextState;

        if (state == GameState::Connecting) {
            //hardcoded for now
            connectingState.emplace("127.0.0.1", 4000);
        }

        if (state == GameState::Waiting) {
            waitingState.emplace();
            std::cout << "Waiting now\n";
        }

        if (state == GameState::Login && previousState != GameState::Login) {
            loginState.enter(*this);
        }

        if (state == GameState::Register && previousState != GameState::Register) {
            registerState.enter(*this);
        }

        if (state == GameState::Playing && !playingSetup) {
            playingState.setup(*this);
            playingSetup = true;
        }
        if (state == GameState::DeckBuilding && previousState != GameState::DeckBuilding) {
            deckBuildingState.enter(*this);
        }
        if (state == GameState::Quit || state == GameState::GameOver) {
            isRunning = false;
        }
    }
    
}

void Game::setNextState(GameState newState) {
    nextState = newState;
}

void Game::setPlayingDeck(Deck newDeck) {
    playingState.setDeck(std::move(newDeck));
    playingSetup = false;
}

bool Game::refreshPlayerDeckFromService() {
    if (!deckBuildingState.refreshFromService(*this)) {
        return false;
    }

    setPlayingDeck(deckBuildingState.buildDeck());
    return true;
}

bool Game::tryStartPlayingWithBuiltDeck() {
    if (!deckBuildingState.hasFullDeck()) {
        return false;
    }

    setPlayingDeck(deckBuildingState.buildDeck());
    setNextState(GameState::Playing);
    return true;
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

int Game::getPlayerId() const {
    return player.id;
}

void Game::setPlayerId(int playerId) {
    if (playerId > 0) {
        player.id = playerId;
    }
}

const std::string& Game::getPlayerUsername() const {
    return playerUsername;
}

void Game::setPlayerUsername(std::string username) {
    if (username.empty()) {
        playerUsername = "Player";
        return;
    }

    playerUsername = std::move(username);
}

int Game::getOpponentPlayerId() const {
    return remotePlayer.id;
}

const std::string& Game::getOpponentPlayerUsername() const {
    return remotePlayerUsername;
}

void Game::setOpponentPlayerInfo(int playerId, std::string username) {
    if (playerId > 0) {
        remotePlayer.id = playerId;
    }

    if (username.empty()) {
        remotePlayerUsername = "Opponent";
    } else {
        remotePlayerUsername = std::move(username);
    }
}

std::size_t Game::getOpponentHandCount() const {
    return opponentHandCount;
}

std::size_t Game::getOpponentDeckCount() const {
    return opponentDeckCount;
}

void Game::setOpponentCounts(std::size_t handCount, std::size_t deckCount) {
    opponentHandCount = handCount;
    opponentDeckCount = deckCount;
}

void Game::applyOpponentDraw() {
    ++opponentHandCount;
    if (opponentDeckCount > 0) {
        --opponentDeckCount;
    }
}

const Deck& Game::getDeck(const Player& player) const {
    return player.getDeck();
}

const Player& Game::getPlayer(bool isOpponent) const {
    return isOpponent ? remotePlayer : player;
}

std::size_t Game::getHandSize(const Player& player) const {
    return player.hand.size();
}

int Game::getHealth(const Player& player) const {
    return player.health;
}

int Game::getMana(const Player& player) const {
    return player.mana;
}

NetworkClient& Game::getNetworkClient() {
    return netClient;
}

const RenderText::FontSet& Game::getTitleFonts() const { return titleFonts; }
const RenderText::FontSet& Game::getUIFonts()    const { return uiFonts;    }


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
            case GameState::Login:
                loginState.handleEvents(*this, event);
                break;
            case GameState::Register:
                registerState.handleEvents(*this, event);
                break;
            case GameState::DeckBuilding:
                deckBuildingState.handleEvents(*this, event);
                break;
            case GameState::Playing:
                playingState.handleEvents(*this, event);
                break;
            case GameState::GameOver:
            case GameState::Connecting:
                if (connectingState) {
                    connectingState->handleEvents(*this, event);
                }
                break;
            case GameState::Waiting:
                if (waitingState) {
                    waitingState->handleEvents(*this, event);
                }
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
        case GameState::Title:
                titleState.update(*this);
                break;
        case GameState::Playing:
            playingState.update(*this);
            break;
        case GameState::DeckBuilding:
            deckBuildingState.update(*this);
            break;
        case GameState::Login:
            loginState.update(*this);
            break;
        case GameState::Register:
            registerState.update(*this);
            break;
        case GameState::Connecting:
            if (connectingState) {
                connectingState->update(*this);
            }
            break;
        case GameState::Waiting:
            if (waitingState) {
                waitingState->update(*this);
            }
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
        case GameState::Login:
            loginState.render(*this);
            break;
        case GameState::Register:
            registerState.render(*this);
            break;
        case GameState::DeckBuilding:
            deckBuildingState.render(*this);
            break;
        case GameState::Playing:
            playingState.render(*this);
            break;
        case GameState::Connecting:
            if (connectingState) {
                connectingState->render(*this);
            }
            break;
        case GameState::Waiting:
            if (waitingState) {
                waitingState->render(*this);
            }
            break;
        default:
            break;
    }

    SDL_RenderPresent(renderer.get());
}

void Game::clean() {
    RenderCard::clearImageCache();
    RenderText::closeFonts(titleFonts);
    RenderText::closeFonts(uiFonts);
    IMG_Quit();
    SDL_Quit();
    isRunning = false;
}

bool Game::running() const {
    return isRunning;
}


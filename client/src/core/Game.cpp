#include "core/Game.hpp"
#include "render/RenderCard.hpp"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <stdexcept>
#include <fstream>
#include <utility>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>
#include "utils/EnvUtil.hpp"
#include "utils/LoadAvailableCards.hpp"
#include "utils/HttpUtil.hpp"
#include "core/Audio.hpp"
#include <map>

namespace {
    int clampPositive(int value, int maxValue) {
        if (value < 0) return 0;
        if (value > maxValue) return maxValue;
        return value;
    }

    bool loadPlayerDeckFromService(const Game& game, Deck& outDeck) {
        if (!LoadAvailableCardsUtil::ensureAvailableCardsLoaded()) {
            return false;
        }

        const auto& availableCards = LoadAvailableCardsUtil::getAvailableCards();

        std::vector<int> deckCopies(availableCards.size(), 0);
        if (!Deck::loadDeckCopiesFromService(game, availableCards, deckCopies, Deck::getDeckCopiesLimit())) {
            return false;
        }

        outDeck = Deck::buildFromCopies(availableCards, deckCopies);
        return true;
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

Game::Game(const char *title, int xpos, int ypos, int width, int height, bool fullscreen) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) {
        throw std::runtime_error(std::string("SDL_Init failed: ") + SDL_GetError());
    }
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        SDL_Quit();
        throw std::runtime_error(std::string("Mix_OpenAudio failed: ") + Mix_GetError());
    }

    Uint32 flags = fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0;
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
#elif defined(__APPLE__)
    // Accelerated is safe on native macOS
    useSoftwareRenderer = false;
#else
    if (isWSL()) {
        std::cout << "WSL detected: forcing software renderer\n";
    } else {
        useSoftwareRenderer = false;
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

    //load SFX
    Audio::loadSFX("draw");
    Audio::loadSFX("discard");
    Audio::loadSFX("activate");
    Audio::loadSFX("damage");
    Audio::loadSFX("destroyed");
    Audio::loadSFX("summon");
    Audio::loadSFX("attack");
    Audio::loadSFX("augment");
    Audio::loadSFX("gameEnd");

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
    if (nextState == state) return;

    const GameState previousState = state;
    state = nextState;

    // --- Exit previous state ---
    switch (previousState) {
        case GameState::Login:        loginState.exit(*this);                          break;
        case GameState::Register:     registerState.exit(*this);                       break;
        case GameState::DeckBuilding: deckBuildingState.exit(*this);                   break;
        case GameState::Loading:      loadingState.exit(*this);
                                      Audio::playMusic("title");      break;
        case GameState::Playing:      playingSetup = false;
                                      Audio::stopMusic();                              break;
        default: break;
    }

    if ((previousState == GameState::Waiting || previousState == GameState::Playing)
        && state == GameState::Title) {
        connectingState.reset();
        getNetworkClient().disconnect();
    }

    // --- Enter new state ---
    switch (state) {
        case GameState::Title:        Audio::playMusic("title");                            break;
        case GameState::Connecting:   connectingState.emplace(EnvUtil::getGameServerHost(),
                                                              EnvUtil::getGameServerPort()); break;
        case GameState::Waiting:      waitingState.emplace();
                                      Audio::playMusic("title");                            break;
        case GameState::Login:        loginState.enter(*this);
                                      Audio::stopMusic();                                   break;
        case GameState::Register:     registerState.enter(*this);                           break;
        case GameState::Loading:      loadingState.enter(*this);                            break;
        case GameState::DeckBuilding: deckBuildingState.enter(*this);                       break;
        case GameState::Payment:      paymentState.enter(*this);                            break;
        case GameState::PackOpening:  packOpeningState.enter(*this);                        break;
        case GameState::Playing:      std::cout << "Committing state change to Playing\n";
                                      playingState.setup(*this);
                                      playingSetup = true; 
                                      Audio::playMusic("battle");                           break;
        case GameState::Quit:
        default: break;
    }
}

void Game::setNextState(GameState newState) {
    nextState = newState;
}

void Game::setPlayingDeck() {
// newDeck.toString();
    Deck deckCopy = deck.clone();
    player.setDeck(std::move(deckCopy));
// player.deck.toString();
    playingSetup = false;
}

bool Game::refreshPlayerDeckFromService() {
    Deck loadedDeck;
    if (!loadPlayerDeckFromService(*this, loadedDeck)) {
        return false;
    }
    deck = std::move(loadedDeck);
    // setPlayingDeck(std::move(loadedDeck));
    return true;
}

int Game::getPackRefundCoins() const {
    return packRefundCoins;
}

void Game::setPackRefundCoins(int coins) {
    packRefundCoins = coins > 0 ? coins : 0;
}

void Game::addPackRefundCoins(int delta) {
    const int next = packRefundCoins + delta;
    packRefundCoins = next > 0 ? next : 0;
}

bool Game::tryStartPlayingWithBuiltDeck() {
    const int deckLimit = Deck::getDeckSizeLimit();
    if (player.getDeck().size() < deckLimit) {
        return false;
    }

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

const std::string& Game::getAuthSessionId() const {
    return authSessionId;
}

void Game::setAuthSessionId(std::string sessionId) {
    authSessionId = std::move(sessionId);
}

bool Game::hasActiveAuthSession() const {
    return !authSessionId.empty();
}

bool Game::endUserSession() {
    if (authSessionId.empty()) {
        return true;
    }

    const std::string host = EnvUtil::getAuthServiceHost();
    const int port = EnvUtil::getAuthServicePort();
    std::map<std::string, std::string> headers{{"X-Session-ID", authSessionId}};

    int statusCode = -1;
    std::string responseBody;
    const bool reachable = HttpUtil::sendHttpWithHeaders(
        host,
        port,
        "POST",
        "/auth/logout",
        "{}",
        headers,
        statusCode,
        responseBody
    );

    if (!reachable || (statusCode != 200 && statusCode != 401)) {
        return false;
    }

    authSessionId.clear();
    return true;
}

Playing& Game::getPlayingState() {
    return playingState;
}

// const Deck& Game::getDeck(const Player& player) const {
//     return player.getDeck();
// }

Player& Game::getPlayer() {
    return player;
}

// std::size_t Game::getHandSize(const Player& player) const {
//     return player.handSize();
// }

// int Game::getHealth(const Player& player) const {
//     return player.health;
// }

// int Game::getMana(const Player& player) const {
//     return player.mana;
// }

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
            case GameState::Loading:
                loadingState.handleEvents(*this, event);
                break;
            case GameState::DeckBuilding:
                deckBuildingState.handleEvents(*this, event);
                break;
            case GameState::Payment:
                paymentState.handleEvents(*this, event);
                break;
            case GameState::PackOpening:
                packOpeningState.handleEvents(*this, event);
                break;
            case GameState::Playing:
                playingState.handleEvents(*this, event);
                break;
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
        case GameState::Payment:
            paymentState.update(*this);
            break;
        case GameState::PackOpening:
            packOpeningState.update(*this);
            break;
        case GameState::Login:
            loginState.update(*this);
            break;
        case GameState::Register:
            registerState.update(*this);
            break;
        case GameState::Loading:
            loadingState.update(*this);
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
        case GameState::Loading:
            loadingState.render(*this);
            break;
        case GameState::DeckBuilding:
            deckBuildingState.render(*this);
            break;
        case GameState::Payment:
            paymentState.render(*this);
            break;
        case GameState::PackOpening:
            packOpeningState.render(*this);
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
	endUserSession();
    RenderCard::clearImageCache();
    RenderText::closeFonts(titleFonts);
    RenderText::closeFonts(uiFonts);
    IMG_Quit();
    Mix_CloseAudio();
    SDL_Quit();
    isRunning = false;
}

bool Game::running() const {
    return isRunning;
}

DeckBuilding& Game::getDeckBuildingState() {
    return deckBuildingState;
}
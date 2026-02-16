//if
#ifndef GAME_HPP
#define GAME_HPP

#include "SDL2/SDL.h"
#include <stdio.h>
#include <cstddef>
#include <memory> //for smart pointers
#include <stdexcept> //for signalling fatal init errors
#include <optional>

//core
#include "GameState.hpp"
#include "NetworkClient.hpp"

#include "states/Title.hpp"
#include "states/Login.hpp"
#include "states/Register.hpp"
#include "states/Playing.hpp"
#include "states/DeckBuilding.hpp"
#include "states/Connecting.hpp"
#include "render/RenderText.hpp"

#include "objects/Player.h"
#include "objects/Deck.h"
#include "Board.hpp"

class Game {

public:  
    //constructor
    Game(const char* title,
     int xpos,
     int ypos,
     int width,
     int height,
     bool fullscreen,
     int drawIntervalSeconds = 3);

    //destructor, uses RAII cleanup
    ~Game(); 

    Game(const Game&) = delete; //ensure you cannot copy the Game object
    Game& operator=(const Game&) = delete; //ensure you cannot reassign another existing Game object to it

    //This is to handle mem resource transfer between Game objects to ensure only 1 object owns the resources at any time
    Game(Game&&) noexcept = default;
    Game& operator=(Game&&) noexcept = default;

    //returns reference to NetworkClient object, NOT a copy
    NetworkClient& getNetworkClient();
    
    void handleEvents();
    void update();
    void render();
    void clean();

    //Getters and Setters
    void setNextState(GameState newState);
    void commitStateChange();
    void setPlayingDeck(Deck newDeck);
    bool tryStartPlayingWithBuiltDeck();
    bool refreshPlayerDeckFromService();

    GameState getState() const;
    GameState getNextState() const;
    SDL_Renderer* getRenderer() const;
    int getPlayerId() const;
    void setPlayerId(int playerId);

    const Player& getPlayer(bool isOpponent) const;
    const Deck& getDeck(const Player& player) const;
    std::size_t getHandSize(const Player& player) const;
    int getHealth(const Player& player) const;
    int getMana(const Player& player) const;

    const RenderText::FontSet& getTitleFonts() const;
    const RenderText::FontSet& getUIFonts() const;

    bool running() const;


private:
    bool isRunning{false};
    NetworkClient netClient;

    Player player;
    Player remotePlayer;

    int drawIntervalSeconds{3};
    Uint32 lastDrawTick{0};
    std::size_t lastLoggedHandSize{0};
    bool playingSetup{false};

    //RAII smart pointers, chain their destruction to the Game object's destruction
    using WindowPtr =
        std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)>;

    using RendererPtr =
        std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)>;

    WindowPtr window{nullptr, SDL_DestroyWindow};
    RendererPtr renderer{nullptr, SDL_DestroyRenderer};

    //GameState::Title used because Title is not in global scope, only exists within GameState
    GameState state{GameState::Login};
    GameState nextState{GameState::Login};


    RenderText::FontSet titleFonts;
    RenderText::FontSet uiFonts;

    Title titleState;
    Login loginState;
    Register registerState;
    Playing playingState{drawIntervalSeconds};
    DeckBuilding deckBuildingState;
    std::optional<Connecting> connectingState;
    // Connecting connectingState;
};

#endif
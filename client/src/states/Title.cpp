#include "states/Title.hpp"
#include "core/Game.hpp"
#include <SDL2/SDL.h>
#include <iostream>

void Title::handleEvents(Game& game, const SDL_Event& event) {
    if (event.type == SDL_QUIT)
    {
        game.setNextState(GameState::Quit);
    }

    if (event.type == SDL_MOUSEBUTTONDOWN &&
        event.button.button == SDL_BUTTON_LEFT)
    {
        int mouseX = event.button.x;
        int mouseY = event.button.y;

        const bool inStart = (mouseX >= startButton.x && mouseX <= (startButton.x + startButton.w)) &&
                             (mouseY >= startButton.y && mouseY <= (startButton.y + startButton.h));
        const bool inQuit = (mouseX >= quitButton.x && mouseX <= (quitButton.x + quitButton.w)) &&
                            (mouseY >= quitButton.y && mouseY <= (quitButton.y + quitButton.h));
        const bool inBuildDeck = (mouseX >= BuildDeckButton.x && mouseX <= (BuildDeckButton.x + BuildDeckButton.w)) &&
                                 (mouseY >= BuildDeckButton.y && mouseY <= (BuildDeckButton.y + BuildDeckButton.h));

        const bool inConnect = (mouseX >= ConnectButton.x && mouseX <= (ConnectButton.x + ConnectButton.w)) &&
                                 (mouseY >= ConnectButton.y && mouseY <= (ConnectButton.y + ConnectButton.h));

        if (inStart) {
            game.setNextState(GameState::Playing);
        } else if (inQuit) {
            game.setNextState(GameState::Quit);
        } else if (inBuildDeck) {
            game.setNextState(GameState::DeckBuilding);
        } else if (inConnect) {
            game.setNextState(GameState::Connecting);
        }
    }

}

void Title::update(Game& game) {

}

void Title::render(Game& game) {
    SDL_Renderer* renderer = game.getRenderer();

    // background
    SDL_SetRenderDrawColor(renderer, 25, 25, 25, 255);
    SDL_RenderClear(renderer);

    // title banner
    SDL_Rect titleBanner{180, 40, 440, 70};
    SDL_SetRenderDrawColor(renderer, 80, 120, 200, 255);
    SDL_RenderFillRect(renderer, &titleBanner);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &titleBanner);

    // start button
    SDL_SetRenderDrawColor(renderer, 80, 200, 120, 255);
    SDL_RenderFillRect(renderer, &startButton);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &startButton);

    // quit button
    SDL_SetRenderDrawColor(renderer, 200, 80, 80, 255);
    SDL_RenderFillRect(renderer, &quitButton);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &quitButton);

    // build deck button
    SDL_SetRenderDrawColor(renderer, 200, 200, 80, 255);
    SDL_RenderFillRect(renderer, &BuildDeckButton);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &BuildDeckButton);

    // build connect button
    SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
    SDL_RenderFillRect(renderer, &ConnectButton);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &ConnectButton);

}
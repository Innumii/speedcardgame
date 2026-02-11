#include "states/Title.hpp"
#include "core/Game.hpp"
#include <SDL2/SDL.h>
#include <iostream>

void Title::updateLayout(SDL_Renderer* renderer) {
    int screenW = 800;
    int screenH = 600;
    if (renderer) {
        SDL_GetRendererOutputSize(renderer, &screenW, &screenH);
    }

    const int bannerW = titleBanner.w;
    const int bannerH = titleBanner.h;
    const int buttonW = startButton.w;
    const int buttonH = startButton.h;
    const int bannerGap = 24;
    const int buttonGap = 16;

    const int buttonsTotalH = (buttonH * 4) + (buttonGap * 3);
    const int totalH = bannerH + bannerGap + buttonsTotalH;
    int topY = (screenH - totalH) / 2;
    if (topY < 20) topY = 20;

    const int centerX = screenW / 2;

    titleBanner.x = centerX - (bannerW / 2);
    titleBanner.y = topY;

    startButton.x = centerX - (buttonW / 2);
    startButton.y = titleBanner.y + bannerH + bannerGap;

    quitButton.x = startButton.x;
    quitButton.y = startButton.y + buttonH + buttonGap;

    BuildDeckButton.x = startButton.x;
    BuildDeckButton.y = quitButton.y + buttonH + buttonGap;

    ConnectButton.x = startButton.x;
    ConnectButton.y = BuildDeckButton.y + buttonH + buttonGap;
}

void Title::handleEvents(Game& game, const SDL_Event& event) {
    updateLayout(game.getRenderer());

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
            if (!game.tryStartPlayingWithBuiltDeck()) {
                game.setNextState(GameState::Playing);
            }
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
    updateLayout(renderer);

    // background
    SDL_SetRenderDrawColor(renderer, 25, 25, 25, 255);
    SDL_RenderClear(renderer);

    // title banner
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
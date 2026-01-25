#include "Title.hpp"
#include "../core/Game.hpp"
#include <SDL2/SDL.h>

void Title::handleEvents(Game& game, const SDL_Event& event) {
    if (event.type == SDL_QUIT)
    {
        game.setState(GameState::Quit);
    }

    if (event.type == SDL_MOUSEBUTTONDOWN &&
        event.button.button == SDL_BUTTON_LEFT)
    {
        int mouseX = event.button.x;
        int mouseY = event.button.y;

        if (mouseX >= quitButton.x && mouseX <= (quitButton.x + quitButton.w) &&
            mouseY >= quitButton.y && mouseY <= (quitButton.y + quitButton.h))
        {
            game.setState(GameState::Quit);
        }
    }

}

void Title::update(Game& game) {

}

void Title::render(Game& game) {
    SDL_Renderer* renderer = game.getRenderer();

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    // Draw the quit button
    SDL_SetRenderDrawColor(renderer, 0, 200, 0, 255);
    SDL_RenderFillRect(renderer, &quitButton);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &quitButton);

    SDL_RenderPresent(renderer);
}
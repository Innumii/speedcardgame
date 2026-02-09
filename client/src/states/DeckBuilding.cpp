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
#include <SDL2/SDL.h>
#include <iostream>

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


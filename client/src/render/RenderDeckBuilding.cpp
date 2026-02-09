#include "render/RenderDeckBuilding.hpp"

#include "core/Game.hpp"
#include "states/DeckBuilding.hpp"
#include "objects/Card.h"
#include "render/RenderCard.hpp"
#include "render/RenderText.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <algorithm>
#include <vector>

void RenderDeckBuilding::render(DeckBuilding& deckBuilding, Game& game) {
    SDL_Renderer* renderer = game.getRenderer();
    if (!renderer) return;

    if (!TTF_WasInit() && TTF_Init() != 0) {
        return;
    }

    RenderText textRenderer;

    // Title Button
    TTF_Font* font = TTF_OpenFont("assets/font.TTF", 18);
    textRenderer.drawText(
        renderer,
        "Return to Title",
        font,
        SDL_Color{255, 255, 255, 255},
        deckBuilding.TitleButton.x + 10,
        deckBuilding.TitleButton.y + 10
    );

    if (font) {
        TTF_CloseFont(font);
    }

    // Further rendering of deck building UI would go here
}
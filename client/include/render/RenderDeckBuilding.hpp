#ifndef RENDERDECKBUILDING_HPP
#define RENDERDECKBUILDING_HPP

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

class Card;
class RenderText;
class DeckBuilding;
class Game;
class RenderDeckBuilding {
public:
    static void render(DeckBuilding& deckBuilding, Game& game);
};


#endif
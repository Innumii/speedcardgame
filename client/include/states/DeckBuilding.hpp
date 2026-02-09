#ifndef DECKBUILDING_HPP
#define DECKBUILDING_HPP

#include <SDL2/SDL.h>

class Game;
class RenderDeckBuilding;

class DeckBuilding {
    friend class RenderDeckBuilding;
public:
	void handleEvents(Game& game, const SDL_Event& event);
	void update(Game& game);
	void render(Game& game);

private:
	SDL_Rect TitleButton{20, 20, 140, 50};
};

#endif
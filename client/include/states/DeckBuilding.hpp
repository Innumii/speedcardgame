#ifndef DECKBUILDING_HPP
#define DECKBUILDING_HPP

#include <SDL2/SDL.h>
#include <memory>
#include <vector>

#include "objects/Deck.h"

class Game;
class RenderDeckBuilding;
class Card;

class DeckBuilding {
    friend class RenderDeckBuilding;
public:
	DeckBuilding();
	void handleEvents(Game& game, const SDL_Event& event);
	void update(Game& game);
	void render(Game& game);
	Deck buildDeck() const;
	bool hasCardsInDeck() const;

private:
	struct Layout {
		SDL_Rect collectionArea{};
		SDL_Rect deckArea{};
		std::vector<SDL_Rect> collectionCardRects;
		std::vector<SDL_Rect> deckEntryRects;
		std::vector<int> deckEntryCardIndices;
	};

	Layout buildLayout(Game& game) const;
	std::vector<int> getDeckEntryOrder() const;
	void tryAddToDeck(int cardIndex);
	void tryRemoveFromDeck(int cardIndex);

	SDL_Rect TitleButton{20, 20, 140, 50};
	SDL_Rect PlayButton{20, 80, 140, 50};
	std::vector<std::unique_ptr<Card>> availableCards;
	std::vector<int> deckCopies;

	bool dragging{false};
	bool draggingFromDeck{false};
	int draggedCardIndex{-1};
	SDL_Point dragPos{0, 0};
	SDL_Point dragOffset{0, 0};

	static constexpr int MaxDeckCopies = 4;
};

#endif
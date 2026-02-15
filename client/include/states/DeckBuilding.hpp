#ifndef DECKBUILDING_HPP
#define DECKBUILDING_HPP

#include <SDL2/SDL.h>
#include <cstddef>
#include <memory>
#include <string>
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
	void enter(Game& game);
	void exit(Game& game);
	bool refreshFromService(Game& game);
	Deck buildDeck() const;
	bool hasCardsInDeck() const;
	bool hasFullDeck() const;
	int getDeckCardCount() const;
	int getDeckSizeLimit() const;
	const std::string& getStatusMessage() const;
	bool isStatusMessageActive(Uint32 now) const;

private:
	struct Layout {
		SDL_Rect collectionArea{};
		SDL_Rect deckArea{};
		SDL_Rect prevPageButton{};
		SDL_Rect nextPageButton{};
		SDL_Rect pageLabelRect{};
		std::vector<SDL_Rect> collectionCardRects;
		std::vector<int> collectionCardIndices;
		std::vector<SDL_Rect> deckEntryRects;
		std::vector<int> deckEntryCardIndices;
		int maxSlots{0};
		int pageCount{1};
		int pageIndex{0};
	};

	Layout buildLayout(Game& game) const;
	void updateMenuButtons(const Layout& layout);
	std::vector<int> getDeckEntryOrder() const;
	void tryAddToDeck(int cardIndex);
	void tryRemoveFromDeck(int cardIndex);
	bool loadAvailableCardsFromService(Game& game);
	bool loadAvailableCardsFromCsv(Game& game);
	bool loadInventoryFromService(Game& game);
	bool loadDeckFromService(Game& game);
	bool saveDeckToService(Game& game) const;
	int getInventoryCount(int cardIndex) const;
	int getRemainingCount(int cardIndex) const;
	void setStatusMessage(const std::string& message, Uint32 durationMs);

	SDL_Rect TitleButton{20, 20, 140, 50};
	SDL_Rect SaveButton{20, 80, 140, 50};
	SDL_Rect PlayButton{20, 140, 140, 50};
	std::vector<std::unique_ptr<Card>> availableCards;
	std::vector<int> deckCopies;
	std::vector<int> inventoryCopies;

	bool dragging{false};
	bool draggingFromDeck{false};
	int draggedCardIndex{-1};
	SDL_Point dragPos{0, 0};
	SDL_Point dragOffset{0, 0};
	bool cardsLoadedFromService{false};
	bool inventoryLoaded{false};
	std::size_t hoverIndex{static_cast<std::size_t>(-1)};
	Uint32 hoverStartTick{0};
	int collectionPage{0};
	std::string statusMessage{};
	Uint32 statusMessageUntil{0};

	static constexpr int MaxDeckCopies = 4;
};

#endif
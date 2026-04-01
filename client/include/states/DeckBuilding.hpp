#ifndef DECKBUILDING_HPP
#define DECKBUILDING_HPP

#include <SDL2/SDL.h>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "StateInterface.hpp"
#include "render/Theme.hpp"

class Game;
class RenderDeckBuilding;
class Card;

class DeckBuilding : public StateInterface {
    friend class RenderDeckBuilding;
public:
	DeckBuilding();
	void handleEvents(Game& game, const SDL_Event& event) override;
	void update(Game& game) override;
	void render(Game& game) override;
	void enter(Game& game) override;
	void exit(Game& game) override;
	bool refreshFromService(Game& game);
	const std::string& getStatusMessage() const;
	bool isStatusMessageActive(Uint32 now) const;

	const std::vector<std::unique_ptr<Card>>& getAvailableCards() const;
	


private:
struct Layout {
    SDL_Rect collectionArea{};
    SDL_Rect deckArea{};
    SDL_Rect prevPageButton{};
    SDL_Rect nextPageButton{};
    SDL_Rect pageLabelRect{};
    SDL_Rect deckEntriesClipRect{};
    std::vector<SDL_Rect> collectionCardRects;
    std::vector<int> collectionCardIndices;
    std::vector<SDL_Rect> deckEntryRects;
    std::vector<int> deckEntryCardIndices;
    std::vector<SDL_Rect> deckEntryRemoveRects;
    int maxSlots{0};
    int pageCount{1};
    int pageIndex{0};
    int maxDeckScrollOffset{0};
};

	Layout buildLayout(const Game& game) const;
	void updateMenuButtons(const Layout& layout);
	std::vector<int> getDeckEntryOrder() const;
	void setStatusMessage(const std::string& message, Uint32 durationMs);

	SDL_Rect TitleButton{
		Theme::DeckBuilding::MENU_BUTTON_INITIAL_X,
		Theme::DeckBuilding::MENU_BUTTON_INITIAL_Y,
		Theme::DeckBuilding::MENU_BUTTON_WIDTH,
		Theme::DeckBuilding::MENU_BUTTON_HEIGHT
	};
	SDL_Rect SaveButton{
		Theme::DeckBuilding::MENU_BUTTON_INITIAL_X,
		Theme::DeckBuilding::MENU_BUTTON_INITIAL_Y,
		Theme::DeckBuilding::MENU_BUTTON_WIDTH,
		Theme::DeckBuilding::MENU_BUTTON_HEIGHT
	};
	SDL_Rect PlayButton{
		Theme::DeckBuilding::MENU_BUTTON_INITIAL_X,
		Theme::DeckBuilding::MENU_BUTTON_INITIAL_Y,
		Theme::DeckBuilding::MENU_BUTTON_WIDTH,
		Theme::DeckBuilding::MENU_BUTTON_HEIGHT
	};
	SDL_Rect ClearButton{
		Theme::DeckBuilding::MENU_BUTTON_INITIAL_X,
		Theme::DeckBuilding::MENU_BUTTON_INITIAL_Y,
		Theme::DeckBuilding::MENU_BUTTON_WIDTH,
		Theme::DeckBuilding::MENU_BUTTON_HEIGHT
	};
	std::vector<int> deckCopies; //st


	bool dragging{false};
	bool draggingFromDeck{false};
	int draggedCardIndex{-1};
	SDL_Point dragPos{0, 0};
	SDL_Point dragOffset{0, 0};
	bool cardsLoadedFromService{false};
	std::size_t hoverIndex{static_cast<std::size_t>(-1)};
	Uint32 hoverStartTick{0};
	int collectionPage{0};
	std::string statusMessage{};
	Uint32 statusMessageUntil{0};
	int currentScale{1};
	int deckScrollOffset = 0;
};

#endif
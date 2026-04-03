#ifndef RENDERBOARD_HPP
#define RENDERBOARD_HPP

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <set>
#include <utility>
#include <vector>

class Board;
class RenderCard;
class RenderText;
class Card;

// Responsible for drawing board zones and placed cards
class RenderBoard {
public:
	static void drawOpponentPlayZones(SDL_Renderer* renderer, RenderText& textRenderer, const std::vector<SDL_Rect>& opponentSlots, TTF_Font* fontSmall);
	static void drawPlayZones(SDL_Renderer* renderer, RenderText& textRenderer, const std::vector<SDL_Rect>& playSlots, TTF_Font* fontSmall);
	static void drawDiscardZone(SDL_Renderer* renderer, RenderText& textRenderer, const SDL_Rect& discardZone, bool hovering, TTF_Font* fontSmall);
	static void drawBoardState(SDL_Renderer* renderer, RenderText& textRenderer, const Board& board, const std::vector<SDL_Rect>& playSlots, const std::vector<SDL_Rect>& opponentSlots, TTF_Font* fontTitle, TTF_Font* fontBody, const std::set<std::pair<int, int>>* skippedSlots = nullptr);
	static SDL_Texture* createZoneTexture(SDL_Renderer* renderer, const std::vector<SDL_Rect>& slots, SDL_Color fill, SDL_Color border);

private:
	static SDL_Texture* cachedPlayZones;
	static SDL_Texture* cachedOpponentZones;
	static SDL_Texture* cachedDiscardZone;
	static int cachedWidth;
	static int cachedHeight;
};

#endif
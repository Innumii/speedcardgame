#ifndef RENDERCARD_HPP
#define RENDERCARD_HPP

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

class Card;
class RenderText;

// Utilities for drawing cards and previews
class RenderCard {
public:
	static void drawCardFace(SDL_Renderer* renderer, RenderText& textRenderer, const Card& card, const SDL_Rect& cardRect, TTF_Font* titleFont, TTF_Font* bodyFont, bool dimmed);
	static void drawCardBack(SDL_Renderer* renderer, const SDL_Rect& cardRect);
	static void drawHandCard(SDL_Renderer* renderer, RenderText& textRenderer, const Card& card, const SDL_Rect& cardRect, TTF_Font* fontSmall);
	static void drawBoardCard(SDL_Renderer* renderer, RenderText& textRenderer, const Card& card, const SDL_Rect& cardRect, TTF_Font* fontSmall);
	static void drawPreview(SDL_Renderer* renderer, RenderText& textRenderer, const Card& card, const SDL_Rect& previewRect, TTF_Font* fontSmall, TTF_Font* fontLarge);
};

#endif

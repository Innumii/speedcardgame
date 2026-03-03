#ifndef RENDERCARD_HPP
#define RENDERCARD_HPP

#include <SDL2/SDL.h>

class Card;
class RenderText;
struct TTF_Font;

class RenderCard {
public:
    static void drawCardFace(SDL_Renderer* renderer, RenderText& textRenderer, const Card& card,
                            const SDL_Rect& rect, TTF_Font* titleFont, TTF_Font* bodyFont, 
                            bool dimmed = false, bool compact = true, int scrollOffset = 0);

    static void drawHandCard(SDL_Renderer* renderer, RenderText& textRenderer, const Card& card,
                            const SDL_Rect& cardRect, TTF_Font* titleFont, TTF_Font* bodyFont);

    static void drawBoardCard(SDL_Renderer* renderer, RenderText& textRenderer, const Card& card,
                             const SDL_Rect& cardRect, TTF_Font* titleFont, TTF_Font* bodyFont);

    static void drawPreview(SDL_Renderer* renderer, RenderText& textRenderer, const Card& card,
                           const SDL_Rect& previewRect, TTF_Font* bodyFont, TTF_Font* titleFont, 
                           int scrollOffset = 0);

    static void drawCardBack(SDL_Renderer* renderer, const SDL_Rect& cardRect);

    static void clearImageCache();
};

#endif
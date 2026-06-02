#ifndef RENDERCARD_HPP
#define RENDERCARD_HPP

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <cstdint>

class Card;
class RenderText;
// struct TTF_Font;

class RenderCard {
public:
    // ── Frame lifecycle ───────────────────────────────────────────────────────
    // beginFrame()      – call once at the top of every render-loop iteration.
    // evictTextCache()  – call periodically (e.g. every 300 frames) to release
    //                     GPU memory for text that is no longer being drawn.
    static void beginFrame();
    static void evictTextCache(uint32_t maxAge = 300);

    // ── Card render-texture cache ─────────────────────────────────────────────
    // invalidateCardCache() forces a one-frame redraw for a specific card on
    // its next draw call.  Call whenever a card's stats or effects change.
    // clearRenderCache()  destroys every cached card texture (e.g. on shutdown).
    static void invalidateCardCache(int cardId);
    static void clearRenderCache();

    // ── Draw API ──────────────────────────────────────────────────────────────
    // RenderText& is accepted for API compatibility; all text rendering is
    // handled internally via the text cache and no state is read from it.
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

    // ── Art cache ─────────────────────────────────────────────────────────────
    static bool preloadCardArt(SDL_Renderer* renderer, int cardId);
    static bool isCardArtCached(int cardId);
    static void clearImageCache();
};

#endif // RENDERCARD_HPP
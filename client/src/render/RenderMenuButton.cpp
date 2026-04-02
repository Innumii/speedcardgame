#include "render/RenderMenuButton.hpp"

// ─────────────────────────────────────────────────────────────────────────────
// draw
// ─────────────────────────────────────────────────────────────────────────────
void RenderMenuButton::draw(SDL_Renderer*      renderer,
                             int                x,
                             int                y,
                             const std::string& text,
                             TTF_Font*          font,
                             bool               hovered,
                             Uint8              alpha)
{
    if (!renderer || !font || text.empty() || alpha == 0) return;

    int tw = 0, th = 0;
    TTF_SizeText(font, text.c_str(), &tw, &th);
    if (th <= 0) return;

    // ── Vertical accent bar ───────────────────────────────────────────
    // Dims slightly when not hovered; respects the overall fade alpha.
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    const Uint8 barBase  = hovered ? 255 : 150;
    const Uint8 barAlpha = static_cast<Uint8>(barBase * alpha / 255);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, barAlpha);
    SDL_Rect bar{x, y, BAR_WIDTH, th};
    SDL_RenderFillRect(renderer, &bar);

    // ── Label ─────────────────────────────────────────────────────────
    // Render white text so we can alpha-mod the texture without colour shift.
    SDL_Surface* surf = TTF_RenderText_Blended(font, text.c_str(), {255, 255, 255, 255});
    if (!surf) return;

    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_FreeSurface(surf);
    if (!tex) return;

    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);

    // Hovered: full white. Normal: slightly dimmed.
    const Uint8 baseR = hovered ? 255 : 205;
    SDL_SetTextureColorMod(tex, baseR, baseR, baseR);
    SDL_SetTextureAlphaMod(tex, alpha);

    SDL_Rect dst{x + BAR_WIDTH + BAR_GAP, y, tw, th};
    SDL_RenderCopy(renderer, tex, nullptr, &dst);
    SDL_DestroyTexture(tex);
}

// ─────────────────────────────────────────────────────────────────────────────
// measure
// ─────────────────────────────────────────────────────────────────────────────
SDL_Rect RenderMenuButton::measure(TTF_Font*          font,
                                    const std::string& text,
                                    int                x,
                                    int                y)
{
    int tw = 0, th = 0;
    if (font) TTF_SizeText(font, text.c_str(), &tw, &th);
    return {x, y, BAR_WIDTH + BAR_GAP + tw, th};
}
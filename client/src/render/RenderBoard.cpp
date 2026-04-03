#include "render/RenderBoard.hpp"

#include "core/Board.hpp"
#include "objects/Card.h"
#include "render/RenderCard.hpp"
#include "render/RenderText.hpp"
#include "render/Theme.hpp"
#include "utils/RenderUtil.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <algorithm>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
//  Zone texture cache
//
//  Each zone type (player slot, opponent slot, discard-normal, discard-hover)
//  gets its own CachedZoneTex.  A texture is rebuilt only when the slot
//  dimensions change (i.e., the window was resized).  The texture is drawn
//  offscreen at {0,0,w,h} and then blitted at whatever world position is
//  needed, so every slot of the same type shares one texture.
// ─────────────────────────────────────────────────────────────────────────────

namespace {

struct CachedZoneTex {
    SDL_Texture* tex = nullptr;
    int cachedW      = -1;
    int cachedH      = -1;

    void invalidate() {
        if (tex) {
            SDL_DestroyTexture(tex);
            tex = nullptr;
        }
        cachedW = cachedH = -1;
    }

    bool matches(int w, int h) const {
        return tex != nullptr && cachedW == w && cachedH == h;
    }
};

// One cache entry per zone variant.
CachedZoneTex s_playerSlot;
CachedZoneTex s_opponentSlot;
CachedZoneTex s_discardNormal;
CachedZoneTex s_discardHover;
CachedZoneTex s_opponentDiscardNormal;

// ── Reference resolution the Theme constants were authored for ────────────────
constexpr float kRefW = 1200.0F;
constexpr float kRefH = 850.0F;

// ── Render a zone texture into a render-target texture ───────────────────────
//   The local rect passed in is {0,0,w,h}; all drawing is relative to it.
void renderZoneToTexture(
        SDL_Renderer*   renderer,
        SDL_Texture*    target,
        int             w,
        int             h,
        SDL_Color       fill,
        SDL_Color       border,
        int             borderThickness,
        SDL_Color       accentColor,
        int             accentLength,
        int             accentThickness)
{
    SDL_SetRenderTarget(renderer, target);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);

    const SDL_Rect local = {0, 0, w, h};

    // ── Fill ─────────────────────────────────────────────────────────
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);
    SDL_RenderFillRect(renderer, &local);

    // ── Plain border ─────────────────────────────────────────────────
    RenderUtil::drawPlainBorder(renderer, local, border, borderThickness);

    // ── Corner accents (drawn on top, bolder) ────────────────────────
    RenderUtil::drawCornerAccents(renderer, local, accentColor,
                                  accentLength, accentThickness);

    SDL_SetRenderTarget(renderer, nullptr);
}

// ── Get (or rebuild) a cached zone texture ───────────────────────────────────
SDL_Texture* getOrRebuild(
        SDL_Renderer*   renderer,
        CachedZoneTex&  cache,
        int             w,
        int             h,
        SDL_Color       fill,
        SDL_Color       border,
        int             borderThickness,
        SDL_Color       accentColor,
        int             accentLength,
        int             accentThickness)
{
    if (cache.matches(w, h)) {
        return cache.tex;   // reuse
    }

    cache.invalidate();

    SDL_Texture* tex = SDL_CreateTexture(renderer,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,
        w, h);
    if (!tex) return nullptr;

    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);

    renderZoneToTexture(renderer, tex, w, h,
        fill, border, borderThickness,
        accentColor, accentLength, accentThickness);

    cache.tex     = tex;
    cache.cachedW = w;
    cache.cachedH = h;
    return tex;
}

// ── Blit a cached zone texture at a world-space rect ─────────────────────────
void blitZone(SDL_Renderer* renderer, SDL_Texture* tex, const SDL_Rect& dst) {
    if (!tex) return;
    SDL_RenderCopy(renderer, tex, nullptr, &dst);
}

// ── Compute scaled accent dimensions from a slot rect ────────────────────────
//   The Theme constants are authored at kRefW×kRefH.  We scale them by the
//   ratio of the actual slot width to the reference slot width so they shrink
//   and grow proportionally with the rest of the UI.
void scaledAccentDims(int slotW,
                      int accentLengthRef, int accentThicknessRef,
                      int& outLength, int& outThickness)
{
    const float ratio    = static_cast<float>(slotW) /
                           static_cast<float>(Theme::Playing::SLOT_WIDTH);
    outLength    = std::max(4, static_cast<int>(accentLengthRef    * ratio));
    outThickness = std::max(1, static_cast<int>(accentThicknessRef * ratio));
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
//  Public API
// ─────────────────────────────────────────────────────────────────────────────

void RenderBoard::drawOpponentPlayZones(SDL_Renderer* renderer, RenderText& textRenderer,
                                        const std::vector<SDL_Rect>& opponentSlots,
                                        TTF_Font* fontSmall) {
    if (!renderer || !fontSmall || opponentSlots.empty()) return;

    const SDL_Rect& first = opponentSlots.front();

    // ── Label ────────────────────────────────────────────────────────────
    const int labelY = std::max(Theme::Board::LABEL_MIN_Y,
                                first.y - Theme::Board::LABEL_OFFSET_Y);
    textRenderer.drawText(renderer, "Opponent Play Zone", fontSmall,
                          Theme::Board::OPPONENT_LABEL, first.x, labelY);

    // ── Accent dimensions (scale with slot size) ──────────────────────────
    int accentLen = 0, accentThick = 0;
    scaledAccentDims(first.w,
                     Theme::Board::ZONE_CORNER_ACCENT_LENGTH,
                     Theme::Board::ZONE_CORNER_ACCENT_THICKNESS,
                     accentLen, accentThick);

    // ── Get (or rebuild) cached texture ──────────────────────────────────
    SDL_Texture* tex = getOrRebuild(
        renderer, s_opponentSlot,
        first.w, first.h,
        Theme::Board::OPPONENT_ZONE_FILL,
        Theme::Board::OPPONENT_ZONE_BORDER,
        Theme::Board::ZONE_BORDER_THICKNESS,
        Theme::Board::OPPONENT_ZONE_ACCENT,
        accentLen, accentThick);

    // ── Blit at every slot position ───────────────────────────────────────
    for (const auto& slot : opponentSlots) {
        blitZone(renderer, tex, slot);
    }
}

void RenderBoard::drawOpponentDiscardZone(SDL_Renderer*   renderer,
                                          RenderText&     textRenderer,
                                          const SDL_Rect& discardZone,
                                          TTF_Font*       fontSmall)
{
    if (!renderer || !fontSmall) return;

    int accentLen = 0, accentThick = 0;
    {
        const float ratio = static_cast<float>(discardZone.w) /
                            static_cast<float>(Theme::Playing::CARD_WIDTH);
        accentLen   = std::max(4, static_cast<int>(
                          Theme::Board::DISCARD_CORNER_ACCENT_LENGTH    * ratio));
        accentThick = std::max(1, static_cast<int>(
                          Theme::Board::DISCARD_CORNER_ACCENT_THICKNESS * ratio));
    }

    SDL_Texture* tex = getOrRebuild(
        renderer, s_opponentDiscardNormal,
        discardZone.w, discardZone.h,
        Theme::Board::DISCARD_FILL,
        Theme::Board::DISCARD_BORDER,
        Theme::Board::DISCARD_BORDER_THICKNESS,
        Theme::Board::DISCARD_ACCENT,
        accentLen, accentThick);

    blitZone(renderer, tex, discardZone);

    const int textPadding  = Theme::Board::DISCARD_TEXT_PADDING;
    const int maxTextWidth = discardZone.w - (textPadding * 2);

    textRenderer.drawWrappedText(renderer, "Opponent Discard", fontSmall,
        Theme::Board::DISCARD_DESCRIPTION_TEXT,
        discardZone.x + textPadding,
        discardZone.y + textPadding,
        maxTextWidth);
}

void RenderBoard::drawPlayZones(SDL_Renderer* renderer, RenderText& textRenderer,
                                const std::vector<SDL_Rect>& playSlots,
                                TTF_Font* fontSmall) {
    if (!renderer || !fontSmall || playSlots.empty()) return;

    const SDL_Rect& first = playSlots.front();

    // ── Label ────────────────────────────────────────────────────────────
    const int labelY = std::max(Theme::Board::LABEL_MIN_Y,
                                first.y - Theme::Board::LABEL_OFFSET_Y);
    textRenderer.drawText(renderer, "Play Zone", fontSmall,
                          Theme::Board::PLAYER_LABEL, first.x, labelY);

    // ── Accent dimensions ─────────────────────────────────────────────────
    int accentLen = 0, accentThick = 0;
    scaledAccentDims(first.w,
                     Theme::Board::ZONE_CORNER_ACCENT_LENGTH,
                     Theme::Board::ZONE_CORNER_ACCENT_THICKNESS,
                     accentLen, accentThick);

    // ── Get (or rebuild) cached texture ──────────────────────────────────
    SDL_Texture* tex = getOrRebuild(
        renderer, s_playerSlot,
        first.w, first.h,
        Theme::Board::PLAYER_ZONE_FILL,
        Theme::Board::PLAYER_ZONE_BORDER,
        Theme::Board::ZONE_BORDER_THICKNESS,
        Theme::Board::PLAYER_ZONE_ACCENT,
        accentLen, accentThick);

    // ── Blit at every slot position ───────────────────────────────────────
    for (const auto& slot : playSlots) {
        blitZone(renderer, tex, slot);
    }
}

void RenderBoard::drawDiscardZone(SDL_Renderer* renderer, RenderText& textRenderer,
                                  const SDL_Rect& discardZone, bool hovering,
                                  TTF_Font* fontSmall) {
    if (!renderer || !fontSmall) return;

    // ── Accent dimensions (scale relative to discard width vs. reference) ─
    int accentLen = 0, accentThick = 0;
    {
        // Use the discard width as the reference dimension.
        // Theme::Playing::CARD_WIDTH is the reference slot width we authored for.
        const float ratio = static_cast<float>(discardZone.w) /
                            static_cast<float>(Theme::Playing::CARD_WIDTH);
        accentLen   = std::max(4, static_cast<int>(
                          Theme::Board::DISCARD_CORNER_ACCENT_LENGTH    * ratio));
        accentThick = std::max(1, static_cast<int>(
                          Theme::Board::DISCARD_CORNER_ACCENT_THICKNESS * ratio));
    }

    // ── Hover selects the right cache slot ───────────────────────────────
    CachedZoneTex& cache = hovering ? s_discardHover : s_discardNormal;

    const SDL_Color fill   = hovering ? Theme::Board::DISCARD_FILL_HOVER  : Theme::Board::DISCARD_FILL;
    const SDL_Color border = hovering ? Theme::Board::DISCARD_BORDER_HOVER : Theme::Board::DISCARD_BORDER;
    const SDL_Color accent = hovering ? Theme::Board::DISCARD_ACCENT_HOVER : Theme::Board::DISCARD_ACCENT;
    const int borderThick  = hovering ? Theme::Board::DISCARD_HOVER_BORDER_THICKNESS
                                      : Theme::Board::DISCARD_BORDER_THICKNESS;

    SDL_Texture* tex = getOrRebuild(
        renderer, cache,
        discardZone.w, discardZone.h,
        fill, border, borderThick,
        accent, accentLen, accentThick);

    blitZone(renderer, tex, discardZone);

    // ── Text (drawn live — cheap and must stay sharp at any position) ─────
    const int textPadding  = Theme::Board::DISCARD_TEXT_PADDING;
    const int maxTextWidth = discardZone.w - (textPadding * 2);

    textRenderer.drawWrappedText(renderer, "Discard", fontSmall,
        Theme::Board::DISCARD_DESCRIPTION_TEXT,
        discardZone.x + textPadding,
        discardZone.y + textPadding,
        maxTextWidth);

    textRenderer.drawWrappedText(renderer, "Drop cards to gain mana", fontSmall,
        Theme::Board::DISCARD_DESCRIPTION_TEXT,
        discardZone.x + textPadding,
        discardZone.y + 26,
        maxTextWidth);
}

void RenderBoard::drawBoardState(SDL_Renderer* renderer, RenderText& textRenderer,
                                 const Board& board,
                                 const std::vector<SDL_Rect>& playSlots,
                                 const std::vector<SDL_Rect>& opponentSlots,
                                 TTF_Font* fontTitle, TTF_Font* fontBody,
                                 const std::set<std::pair<int, int>>* skippedSlots) {
    if (!renderer || !fontTitle || !fontBody || playSlots.empty()) return;

    const std::size_t laneCount = std::min(playSlots.size(),
                                           static_cast<std::size_t>(board.getLaneCount()));

    for (int boardIndex = 0; boardIndex <= 1; ++boardIndex) {
        for (std::size_t lane = 0; lane < laneCount; ++lane) {
            if (skippedSlots &&
                skippedSlots->count({static_cast<int>(lane), boardIndex}) > 0) {
                continue;
            }

            const auto& optCard = board.getZone(static_cast<int>(lane), boardIndex);
            if (!optCard || !*optCard) continue;

            const Card* card = optCard->get();
            if (!card) continue;

            const SDL_Rect& rect = (boardIndex == 1) ? opponentSlots[lane] : playSlots[lane];
            RenderCard::drawBoardCard(renderer, textRenderer, *card, rect, fontTitle, fontBody);
        }
    }
}
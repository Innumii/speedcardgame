#include "render/RenderCardOverlay.hpp"
#include "render/Theme.hpp"

#include <algorithm>
#include <cmath>
#include <functional>

namespace {


    struct DimSpan { int xOffset, spanW; };  // relative to panel.x
    struct SizeKey { int w, h; };

    struct SizeKeyHash {
        std::size_t operator()(const SizeKey& k) const noexcept {
            return std::hash<int>{}(k.w) ^ (std::hash<int>{}(k.h) << 16);
        }
    };
    struct SizeKeyEq {
        bool operator()(const SizeKey& a, const SizeKey& b) const noexcept {
            return a.w == b.w && a.h == b.h;
        }
    };

    using SpanRow  = DimSpan;                         // one span per row
    using SpanGrid = std::vector<SpanRow>;            // indexed by (y - panel.y)

    static std::unordered_map<SizeKey, SpanGrid, SizeKeyHash, SizeKeyEq> sDimSpanCache;

    const SpanGrid& getDimSpans(int w, int h) {
        const SizeKey key{w, h};
        auto it = sDimSpanCache.find(key);
        if (it != sDimSpanCache.end()) return it->second;

        const int r = std::max(Theme::Card::MIN_CORNER_RADIUS, w / 20);
        SpanGrid grid;
        grid.reserve(h);

        for (int row = 0; row < h; ++row) {
            int xStart = 0;
            int xEnd   = w;

            const int cy = (row < r)       ? r
                         : (row >= h - r)  ? h - r
                                           : row;
            if (cy != row) {
                const int dy = row - cy;
                const int dx = static_cast<int>(std::sqrt(static_cast<float>(r * r - dy * dy)));
                xStart = r - dx;
                xEnd   = w - r + dx;
            }

            if (xEnd > xStart)
                grid.push_back({ xStart, xEnd - xStart });
            else
                grid.push_back({ 0, 0 });   // fully clipped corner pixel
        }

        return sDimSpanCache.emplace(key, std::move(grid)).first->second;
    }
    
    struct RGB { Uint8 r, g, b; };

    // Converts a hue in [0, 360) to an RGB colour (full saturation and value).
    RGB hueToRGB(float hue) {
        hue = std::fmod(hue, 360.0F);
        if (hue < 0.0F) hue += 360.0F;
        const float s = 1.0F, v = 1.0F;
        const int   hi = static_cast<int>(hue / 60.0F) % 6;
        const float f  = hue / 60.0F - std::floor(hue / 60.0F);
        const float p  = v * (1.0F - s);
        const float q  = v * (1.0F - f * s);
        const float t  = v * (1.0F - (1.0F - f) * s);
        float r = 0, g = 0, b = 0;
        switch (hi) {
            case 0: r = v; g = t; b = p; break;
            case 1: r = q; g = v; b = p; break;
            case 2: r = p; g = v; b = t; break;
            case 3: r = p; g = q; b = v; break;
            case 4: r = t; g = p; b = v; break;
            case 5: r = v; g = p; b = q; break;
        }
        return { static_cast<Uint8>(r * 255), static_cast<Uint8>(g * 255), static_cast<Uint8>(b * 255) };
    }

    int cornerRadiusFormula(int panelWidth) {
        return std::max(Theme::Card::MIN_CORNER_RADIUS, panelWidth / 20);
    }

} // namespace

void RenderCardOverlay::overlayShimmer(SDL_Renderer* renderer, const SDL_Rect& panel, Uint32 now) {
    if (!renderer) return;

    constexpr Uint32 shimmerPeriodMs = 450U;
    constexpr int    shimmerWidth    = 100;
    constexpr Uint8  shimmerAlpha    = 100;
    const int        cornerRadius    = cornerRadiusFormula(panel.w);


    const float t = static_cast<float>(now % shimmerPeriodMs)
                  / static_cast<float>(shimmerPeriodMs);

    const int diagMin  = panel.x + panel.y;
    const int diagMax  = panel.x + panel.w + panel.y + panel.h;
    const int shimmerD = diagMin
                       + static_cast<int>(t * static_cast<float>(diagMax - diagMin + shimmerWidth))
                       - shimmerWidth;

    const int r = cornerRadius;
    auto insideRoundedRect = [&](int x, int y) -> bool {
        if (x < panel.x || x >= panel.x + panel.w) return false;
        if (y < panel.y || y >= panel.y + panel.h) return false;
        const int cx = (x < panel.x + r)            ? panel.x + r
                     : (x >= panel.x + panel.w - r) ? panel.x + panel.w - r
                                                     : x;
        const int cy = (y < panel.y + r)            ? panel.y + r
                     : (y >= panel.y + panel.h - r) ? panel.y + panel.h - r
                                                     : y;
        const int dx = x - cx, dy = y - cy;
        return dx * dx + dy * dy <= r * r;
    };

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, shimmerAlpha);

    for (int i = 0; i < shimmerWidth; ++i) {
        const int d      = shimmerD + i;
        const int xStart = std::max(panel.x,               d - (panel.y + panel.h - 1));
        const int xEnd   = std::min(panel.x + panel.w - 1,  d -  panel.y);
        if (xStart > xEnd) continue;

        for (int x = xStart; x <= xEnd; ++x) {
            if (insideRoundedRect(x, d - x))
                SDL_RenderDrawPoint(renderer, x, d - x);
        }
    }
}

void RenderCardOverlay::overlaySSR(SDL_Renderer* renderer, const SDL_Rect& panel, Uint32 now) {
    if (!renderer) return;

    // One "pixel" block size — larger = chunkier/more retro.
    constexpr int   pixelSize      = 12;
    // How many degrees of hue shift per pixel travelled along the diagonal.
    constexpr float huePerDiagPx   = 5.0F;
    // Full hue cycle period in milliseconds.
    constexpr Uint32 cyclePeriodMs = 2000U;
    constexpr Uint8  ssrAlpha      = 15;

    const int cornerRadius = cornerRadiusFormula(panel.w);

    // Time-driven hue offset so the gradient slides diagonally over time.
    const float timeHueOffset = (static_cast<float>(now % cyclePeriodMs)
                               / static_cast<float>(cyclePeriodMs)) * 360.0F;

    const int r = cornerRadius;
    auto insideRoundedRect = [&](int x, int y) -> bool {
        if (x < panel.x || x >= panel.x + panel.w) return false;
        if (y < panel.y || y >= panel.y + panel.h) return false;
        const int cx = (x < panel.x + r)            ? panel.x + r
                     : (x >= panel.x + panel.w - r) ? panel.x + panel.w - r
                                                     : x;
        const int cy = (y < panel.y + r)            ? panel.y + r
                     : (y >= panel.y + panel.h - r) ? panel.y + panel.h - r
                                                     : y;
        const int dx = x - cx, dy = y - cy;
        return dx * dx + dy * dy <= r * r;
    };

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // Iterate over the panel in pixel-sized blocks.
    for (int by = panel.y; by < panel.y + panel.h; by += pixelSize) {
        for (int bx = panel.x; bx < panel.x + panel.w; bx += pixelSize) {

            // Use the block's top-left corner as the representative point for
            // the rounded-rect test and for hue computation.
            if (!insideRoundedRect(bx, by)) continue;

            // Hue is driven by diagonal position (bx + by) so bands run at 45°,
            // minus the time offset so they scroll down-right over time.
            const float diag = static_cast<float>((bx - panel.x) + (by - panel.y));
            const float hue  = diag * huePerDiagPx - timeHueOffset;
            const RGB   col  = hueToRGB(hue);

            SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, ssrAlpha);

            // Clamp the block to the panel bounds before filling.
            const SDL_Rect block {
                bx,
                by,
                std::min(pixelSize, panel.x + panel.w - bx),
                std::min(pixelSize, panel.y + panel.h - by)
            };
            SDL_RenderFillRect(renderer, &block);
        }
    }
}

void RenderCardOverlay::overlayDim(SDL_Renderer* renderer, const SDL_Rect& panel, Uint8 alpha) {
    if (!renderer) return;

    const SpanGrid& spans = getDimSpans(panel.w, panel.h);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, alpha);

    for (int row = 0; row < static_cast<int>(spans.size()); ++row) {
        const auto& span = spans[row];
        if (span.spanW <= 0) continue;
        const SDL_Rect rect{ panel.x + span.xOffset, panel.y + row, span.spanW, 1 };
        SDL_RenderFillRect(renderer, &rect);
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}
#include "animation/SummonAnimation.hpp"
#include <algorithm>
#include <cmath>

namespace {
    // Same blue used by DiscardAnimation's charge phase
    constexpr SDL_Color kGlowColor = {160, 220, 255, 255};
}

SummonAnimation::SummonAnimation(const SDL_Rect& cardRect, Uint32 durationMs)
    : rect(cardRect),
      durationSeconds(static_cast<float>(std::max<Uint32>(durationMs, 1U)) / 1000.0F) {}

void SummonAnimation::start() {
    elapsedSeconds = 0.0F;
    finished = false;
}

void SummonAnimation::update(float dt) {
    if (finished) return;
    elapsedSeconds += std::max(dt, 0.0F);
    finished = (elapsedSeconds >= durationSeconds);
}

bool SummonAnimation::isFinished() const { return finished; }

float SummonAnimation::getProgress() const {
    return std::clamp(elapsedSeconds / durationSeconds, 0.0F, 1.0F);
}

void SummonAnimation::draw(SDL_Renderer* renderer) const {
    if (!renderer) return;

    const float t = getProgress();
    const float peakT = 0.4F;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    if (t < peakT) {
        // ── Ramp up ──────────────────────────────────────────────────
        const float envelope = t / peakT;

        const Uint8 blueAlpha = static_cast<Uint8>(envelope * 180.0F);
        SDL_SetRenderDrawColor(renderer, kGlowColor.r, kGlowColor.g, kGlowColor.b, blueAlpha);
        SDL_RenderFillRect(renderer, &rect);

        for (int g = 4; g >= 1; --g) {
            const int   expand    = static_cast<int>(g * 6 * envelope);
            const Uint8 glowAlpha = static_cast<Uint8>((6.0F / static_cast<float>(g)) * envelope);
            SDL_SetRenderDrawColor(renderer,
                kGlowColor.r - 20, kGlowColor.g, kGlowColor.b, glowAlpha);
            const SDL_Rect glowRect{
                rect.x - expand, rect.y - expand,
                rect.w + expand * 2, rect.h + expand * 2
            };
            SDL_RenderFillRect(renderer, &glowRect);
        }
    } else {
        // ── Ring dissipation outward ──────────────────────────────────
        const float dissolveT = (t - peakT) / (1.0F - peakT);

        constexpr int BLOCK = 6;
        const int seed = rect.x ^ (rect.y << 8);

        auto blockHash2 = [](int bx, int by, int seed) -> float {
            Uint32 h = static_cast<Uint32>(bx) * 2654435761U
                     ^ static_cast<Uint32>(by) * 1234567891U
                     ^ static_cast<Uint32>(seed) * 40503U;
            h ^= h >> 13; h *= 0x9e3779b9U; h ^= h >> 16;
            return static_cast<float>(h & 0xFFFFU) / 65536.0F;
        };

        // Ring expands from the card boundary outward
        const float maxRadius  = static_cast<float>(std::max(rect.w, rect.h)) * 0.4F;
        const float ringRadius = dissolveT * maxRadius;
        // Half-thickness of the ring in pixels; jitter will blur this edge
        const float ringHalf   = static_cast<float>(BLOCK) * 1.1F;

        // Iterate over a bounding area large enough to contain the full ring
        const int margin = static_cast<int>(maxRadius + ringHalf) + BLOCK * 2;
        const int x0 = rect.x - margin;
        const int y0 = rect.y - margin;
        const int x1 = rect.x + rect.w + margin;
        const int y1 = rect.y + rect.h + margin;

        for (int px = x0; px < x1; px += BLOCK) {
            const int bx = (px - x0) / BLOCK;
            for (int py = y0; py < y1; py += BLOCK) {
                const int by = (py - y0) / BLOCK;

                // Centre of this block
                const float cx = static_cast<float>(px) + BLOCK * 0.5F;
                const float cy = static_cast<float>(py) + BLOCK * 0.5F;

                // Signed distance from the card boundary:
                //   negative  → inside the card (distance to nearest edge, negated)
                //   positive  → outside the card (distance to nearest edge point)
                const float nearX = std::clamp(cx,
                    static_cast<float>(rect.x),
                    static_cast<float>(rect.x + rect.w));
                const float nearY = std::clamp(cy,
                    static_cast<float>(rect.y),
                    static_cast<float>(rect.y + rect.h));
                const float dx = cx - nearX;
                const float dy = cy - nearY;
                const float outsideDist = std::sqrt(dx * dx + dy * dy);

                float signedDist;
                if (outsideDist > 0.0F) {
                    signedDist = outsideDist;
                } else {
                    // inside card — measure inward depth as a negative value
                    const float fromLeft   = cx - static_cast<float>(rect.x);
                    const float fromRight  = static_cast<float>(rect.x + rect.w) - cx;
                    const float fromTop    = cy - static_cast<float>(rect.y);
                    const float fromBottom = static_cast<float>(rect.y + rect.h) - cy;
                    signedDist = -std::min({fromLeft, fromRight, fromTop, fromBottom});
                }

                // Per-block hash jitter to produce a jagged, pixelated ring edge
                const float jitter = (blockHash2(bx, by, seed) - 0.5F)
                                   * static_cast<float>(BLOCK) * 5.0F;
                const float effDist = signedDist + jitter;

                const float distFromRing = effDist - ringRadius;

                // Skip blocks that the ring hasn't reached yet
                if (distFromRing > ringHalf) continue;

                const int bw = std::min(BLOCK, x1 - px);
                const int bh = std::min(BLOCK, y1 - py);
                if (bw <= 0 || bh <= 0) continue;
                const SDL_Rect block{ px, py, bw, bh };

                if (distFromRing < -ringHalf) {
                    // Behind the ring — draw fading card interior only
                    if (signedDist >= 0.0F) continue; // outside card: already gone
                    const Uint8 interiorAlpha =
                        static_cast<Uint8>((1.0F - dissolveT) * 160.0F);
                    if (interiorAlpha == 0) continue;
                    SDL_SetRenderDrawColor(renderer,
                        kGlowColor.r, kGlowColor.g, kGlowColor.b, interiorAlpha);
                    SDL_RenderFillRect(renderer, &block);
                } else {
                    // On the ring — glow that also fades as it expands
                    const float ringT    = (distFromRing + ringHalf) / (2.0F * ringHalf);
                    const float glowPeak = 1.0F - std::abs(ringT - 0.5F) * 2.0F;
                    const float fade     = 1.0F - dissolveT; // dim as ring expands
                    const Uint8 alpha    = static_cast<Uint8>(
                        std::clamp(glowPeak * fade * 255.0F, 0.0F, 255.0F));
                    if (alpha == 0) continue;

                    // Soft halo pass behind the bright block
                    for (int g = 3; g >= 1; --g) {
                        const int   expand    = g * BLOCK;
                        const Uint8 haloAlpha = static_cast<Uint8>(
                            std::clamp(static_cast<float>(alpha) * (0.12F / static_cast<float>(g)),
                                       0.0F, 255.0F));
                        SDL_SetRenderDrawColor(renderer,
                            kGlowColor.r, kGlowColor.g, kGlowColor.b, haloAlpha);
                        const SDL_Rect halo{
                            block.x - expand, block.y - expand,
                            block.w + expand * 2, block.h + expand * 2
                        };
                        SDL_RenderFillRect(renderer, &halo);
                    }

                    SDL_SetRenderDrawColor(renderer,
                        kGlowColor.r, kGlowColor.g, kGlowColor.b, alpha);
                    SDL_RenderFillRect(renderer, &block);
                }
            }
        }
    }
}
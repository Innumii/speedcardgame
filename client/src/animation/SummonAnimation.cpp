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
        // ── Pixel disintegration fade out ────────────────────────────
        const float dissolveT = (t - peakT) / (1.0F - peakT);
        const float time      = static_cast<float>(SDL_GetTicks()) / 300.0F;

        constexpr int BLOCK = 6;
        const int blocksX = (rect.w + BLOCK - 1) / BLOCK;
        const int blocksY = (rect.h + BLOCK - 1) / BLOCK;

        // Same hash functions as DiscardAnimation
        auto blockHash = [](int bx, int seed) -> float {
            Uint32 h = static_cast<Uint32>(bx) * 2654435761U
                     ^ static_cast<Uint32>(seed) * 40503U;
            h ^= h >> 13; h *= 0x9e3779b9U; h ^= h >> 16;
            return static_cast<float>(h & 0xFFFFU) / 65536.0F;
        };

        auto blockHash2 = [](int bx, int by, int seed) -> float {
            Uint32 h = static_cast<Uint32>(bx) * 2654435761U
                     ^ static_cast<Uint32>(by) * 1234567891U
                     ^ static_cast<Uint32>(seed) * 40503U;
            h ^= h >> 13; h *= 0x9e3779b9U; h ^= h >> 16;
            return static_cast<float>(h & 0xFFFFU) / 65536.0F;
        };

        // Use a fixed seed so the pattern is stable per card instance
        const int seed = rect.x ^ (rect.y << 8);

        for (int bx = 0; bx < blocksX; ++bx) {
            const float xNorm   = static_cast<float>(bx) / static_cast<float>(blocksX);
            const float colHash = blockHash(bx, seed);

            const float flame =
                0.12F * std::sin(xNorm * 8.0F  + time + colHash * 6.28F) +
                0.07F * std::sin(xNorm * 15.0F - time * 1.3F + colHash * 3.14F) +
                0.05F * std::sin(xNorm * 25.0F + time * 0.7F) +
                (colHash - 0.5F) * 0.08F;

            const float cutoffNorm  = dissolveT * 1.6F - 0.2F + flame;
            const int   cutoffBlock = static_cast<int>(cutoffNorm * static_cast<float>(blocksY));

            for (int by = 0; by < blocksY; ++by) {
                // Skip blocks that have already disintegrated
                const float scatter      = blockHash2(bx, by, seed) * 0.18F;
                const float blockCutoff  = dissolveT * 1.6F - 0.2F + flame + scatter;
                const int   blockCutoffRow = static_cast<int>(blockCutoff * static_cast<float>(blocksY));
                if (by < blockCutoffRow) continue;
                (void)cutoffBlock;

                const int px = rect.x + bx * BLOCK;
                const int py = rect.y + by * BLOCK;
                const int bw = std::min(BLOCK, rect.x + rect.w - px);
                const int bh = std::min(BLOCK, rect.y + rect.h - py);
                if (bw <= 0 || bh <= 0) continue;

                const SDL_Rect block{ px, py, bw, bh };

                // Edge blocks glow bright, interior blocks are solid
                const float distFromEdge = static_cast<float>(by - blockCutoffRow)
                                         / static_cast<float>(blocksY);
                const bool nearEdge = distFromEdge < 0.12F;

                if (nearEdge) {
                    for (int g = 2; g >= 1; --g) {
                        const int   expand    = g * BLOCK;
                        const Uint8 glowAlpha = static_cast<Uint8>(25 / g);
                        SDL_SetRenderDrawColor(renderer,
                            kGlowColor.r - 20, kGlowColor.g, kGlowColor.b, glowAlpha);
                        const SDL_Rect glowBlock{
                            block.x - expand, block.y - expand,
                            block.w + expand * 2, block.h + expand * 2
                        };
                        SDL_RenderFillRect(renderer, &glowBlock);
                    }
                    SDL_SetRenderDrawColor(renderer,
                        kGlowColor.r - 20, kGlowColor.g, kGlowColor.b, 255);
                } else {
                    SDL_SetRenderDrawColor(renderer,
                        kGlowColor.r, kGlowColor.g, kGlowColor.b, 180);
                }

                SDL_RenderFillRect(renderer, &block);
            }
        }
    }
}
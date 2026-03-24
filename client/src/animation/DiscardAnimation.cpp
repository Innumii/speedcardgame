#include "animation/DiscardAnimation.hpp"
#include <algorithm>
#include <iostream>
#include <render/Theme.hpp>

namespace {
        constexpr SDL_Color PLAYER_MANA_GLOW_BORDER    = {180, 220, 255, 150};
        constexpr int PIXEL_BLOCK_SIZE = 6;  // size of each "pixel" chunk in the disintegration
}

std::unordered_map<int, std::pair<std::shared_ptr<DiscardAnimation>, Uint32>> DiscardAnimation::sPending;

DiscardAnimation::DiscardAnimation(const SDL_Rect& cardRect, int cardId, Uint32 durationMs)
    : rect(cardRect),
      durationSeconds(static_cast<float>(std::max<Uint32>(durationMs, 1U)) / 1000.0F),
      cardId(cardId) {}

DiscardAnimation::~DiscardAnimation() {
    if (cachedTexture) {
        SDL_DestroyTexture(cachedTexture);
        cachedTexture = nullptr;
    }
}

void DiscardAnimation::drawDisintegration(SDL_Renderer* renderer, Uint32 now) const {
    if (!renderer) return;

    const float progress = getProgress();
    const float time     = static_cast<float>(now) / 300.0F;

    // ── Phase split ───────────────────────────────────────────────────
    constexpr float chargeEnd = 0.3F;
    const bool  charging      = progress < chargeEnd;
    const float chargeT       = std::min(1.0F, progress / chargeEnd);
    const float dissolveT     = charging ? 0.0F
                              : (progress - chargeEnd) / (1.0F - chargeEnd);

    // Hash per block column (not per pixel column)
    auto blockHash = [](int bx, int id) -> float {
        Uint32 h = static_cast<Uint32>(bx) * 2654435761U
                 ^ static_cast<Uint32>(id) * 40503U;
        h ^= h >> 13; h *= 0x9e3779b9U;
        h ^= h >> 16;
        return static_cast<float>(h & 0xFFFFU) / 65536.0F;
    };

    // Hash per individual block (for staggered drop timing)
    auto blockHash2 = [](int bx, int by, int id) -> float {
        Uint32 h = static_cast<Uint32>(bx) * 2654435761U
                 ^ static_cast<Uint32>(by) * 1234567891U
                 ^ static_cast<Uint32>(id) * 40503U;
        h ^= h >> 13; h *= 0x9e3779b9U;
        h ^= h >> 16;
        return static_cast<float>(h & 0xFFFFU) / 65536.0F;
    };

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    if (charging) {
        // ── Charge phase: card texture + blue overlay + glow ─────────
        if (cachedTexture) {
            SDL_SetTextureAlphaMod(cachedTexture, 255);
            SDL_SetTextureColorMod(cachedTexture, 255, 255, 255);
            SDL_RenderCopy(renderer, cachedTexture, nullptr, &rect);
        }

        const Uint8 blueAlpha = static_cast<Uint8>(chargeT * 255.0F);
        SDL_SetRenderDrawColor(renderer, PLAYER_MANA_GLOW_BORDER.r, PLAYER_MANA_GLOW_BORDER.g, PLAYER_MANA_GLOW_BORDER.b, blueAlpha);
        SDL_RenderFillRect(renderer, &rect);

        for (int g = 4; g >= 1; --g) {
            const int   expand    = static_cast<int>(g * 5 * chargeT);
            const Uint8 glowAlpha = static_cast<Uint8>((5.0F / g) * chargeT);
            SDL_SetRenderDrawColor(renderer, PLAYER_MANA_GLOW_BORDER.r - 20, PLAYER_MANA_GLOW_BORDER.g, PLAYER_MANA_GLOW_BORDER.b, glowAlpha);
            const SDL_Rect glowRect{
                rect.x - expand, rect.y - expand,
                rect.w + expand * 2, rect.h + expand * 2
            };
            SDL_RenderFillRect(renderer, &glowRect);
        }

    } else {
        // ── Disintegrate phase: chunky pixel blocks ───────────────────
        const int blocksX = (rect.w + PIXEL_BLOCK_SIZE - 1) / PIXEL_BLOCK_SIZE;
        const int blocksY = (rect.h + PIXEL_BLOCK_SIZE - 1) / PIXEL_BLOCK_SIZE;

        for (int bx = 0; bx < blocksX; ++bx) {
            const float xNorm   = static_cast<float>(bx) / static_cast<float>(blocksX);
            const float colHash = blockHash(bx, cardId);

            // Wavy flame front per column, same as before but in block space
            const float flame =
                0.12F * std::sin(xNorm * 8.0F  + time + colHash * 6.28F) +
                0.07F * std::sin(xNorm * 15.0F - time * 1.3F + colHash * 3.14F) +
                0.05F * std::sin(xNorm * 25.0F + time * 0.7F) +
                (colHash - 0.5F) * 0.08F;

            const float cutoffNorm = dissolveT * 1.6F - 0.2F + flame;
            // cutoff in block rows
            const int cutoffBlock = static_cast<int>(cutoffNorm * static_cast<float>(blocksY));

            for (int by = 0; by < blocksY; ++by) {
                if (by < cutoffBlock) continue;  // this block has disintegrated

                // Each block has a slight individual delay based on its own hash,
                // so blocks near the cutoff edge pop off in a scattered pattern
                // rather than a clean horizontal line.
                const float scatter = blockHash2(bx, by, cardId) * 0.18F;
                const float blockCutoff = dissolveT * 1.6F - 0.2F + flame + scatter;
                const int blockCutoffRow = static_cast<int>(blockCutoff * static_cast<float>(blocksY));
                if (by < blockCutoffRow) continue;

                const int px = rect.x + bx * PIXEL_BLOCK_SIZE;
                const int py = rect.y + by * PIXEL_BLOCK_SIZE;
                // Clamp to card bounds
                const int bw = std::min(PIXEL_BLOCK_SIZE, rect.x + rect.w - px);
                const int bh = std::min(PIXEL_BLOCK_SIZE, rect.y + rect.h - py);
                if (bw <= 0 || bh <= 0) continue;

                const SDL_Rect block{px, py, bw, bh};

                // Blocks near the cutoff edge glow bright blue; ones further up are solid
                const float distFromEdge = static_cast<float>(by - blockCutoffRow) / static_cast<float>(blocksY);
                const bool nearEdge = distFromEdge < 0.12F;

                if (nearEdge) {
                    // Glow layers around edge blocks
                    constexpr int glowLayers = 2;
                    for (int g = glowLayers; g >= 1; --g) {
                        const int   expand    = g * PIXEL_BLOCK_SIZE;
                        const Uint8 glowAlpha = static_cast<Uint8>(25 / g);
                        SDL_SetRenderDrawColor(renderer,
                            PLAYER_MANA_GLOW_BORDER.r - 20,
                            PLAYER_MANA_GLOW_BORDER.g,
                            PLAYER_MANA_GLOW_BORDER.b,
                            glowAlpha);
                        const SDL_Rect glowBlock{
                            block.x - expand, block.y - expand,
                            block.w + expand * 2, block.h + expand * 2
                        };
                        SDL_RenderFillRect(renderer, &glowBlock);
                    }
                    // Bright blue edge block
                SDL_SetRenderDrawColor(renderer, PLAYER_MANA_GLOW_BORDER.r - 20, PLAYER_MANA_GLOW_BORDER.g, PLAYER_MANA_GLOW_BORDER.b, 255);
                } else {
                    // Interior blocks: solid mid-blue
                SDL_SetRenderDrawColor(renderer, PLAYER_MANA_GLOW_BORDER.r - 20, PLAYER_MANA_GLOW_BORDER.g, PLAYER_MANA_GLOW_BORDER.b, 255);
                }

                SDL_RenderFillRect(renderer, &block);
            }
        }
    }
}

void DiscardAnimation::start() {
    elapsedSeconds = 0.0F;
    finished = false;
}

void DiscardAnimation::update(float dt) {
    if (finished) return;
    elapsedSeconds += std::max(dt, 0.0F);
    finished = (elapsedSeconds >= durationSeconds);
}

bool DiscardAnimation::isFinished() const { return finished; }
SDL_Rect DiscardAnimation::getRect() const { return rect; }
int DiscardAnimation::getCardId() const { return cardId; }

float DiscardAnimation::getProgress() const {
    return std::clamp(elapsedSeconds / durationSeconds, 0.0F, 1.0F);
}

void DiscardAnimation::stagePending(const SDL_Rect& rect, int cardId, Uint32 durationMs, std::unique_ptr<Card> card) {
    auto anim = std::make_shared<DiscardAnimation>(rect, cardId, durationMs);
    anim->ownedCard = std::move(card);
    sPending[cardId] = {anim, SDL_GetTicks()};
}

bool DiscardAnimation::hasPending(int cardId) {
    return sPending.find(cardId) != sPending.end();
}

const Card* DiscardAnimation::getOwnedCard() const {
    return ownedCard.get();
}

std::shared_ptr<DiscardAnimation> DiscardAnimation::takePending(int cardId) {
    auto it = sPending.find(cardId);
    if (it == sPending.end()) return nullptr;
    auto anim = it->second.first;
    sPending.erase(it);
    return anim;
}

void DiscardAnimation::cleanupStalePending(Uint32 now) {
    constexpr Uint32 ttlMs = 2000U;
    for (auto it = sPending.begin(); it != sPending.end(); ) {
        if (now - it->second.second > ttlMs) {
            it = sPending.erase(it);
        } else {
            ++it;
        }
    }
}
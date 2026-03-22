#include "animation/DiscardAnimation.hpp"
#include <algorithm>
#include <iostream>
#include <render/Theme.hpp>

namespace {
        constexpr SDL_Color PLAYER_MANA_GLOW_BORDER    = {100, 160, 255, 150};
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
    constexpr float chargeEnd = 0.3F;  // first 30% = charge up, rest = disintegrate
    const bool  charging      = progress < chargeEnd;
    const float chargeT       = std::min(1.0F, progress / chargeEnd);
    const float dissolveT     = charging ? 0.0F
                              : (progress - chargeEnd) / (1.0F - chargeEnd);

    auto colHash = [](int x, int id) -> float {
        Uint32 h = static_cast<Uint32>(x) * 2654435761U
                 ^ static_cast<Uint32>(id) * 40503U;
        h ^= h >> 13; h *= 0x9e3779b9U;
        h ^= h >> 16;
        return static_cast<float>(h & 0xFFFFU) / 65536.0F;
    };

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    if (charging) {
        // ── Charge phase: draw cached card texture, then overlay blue on top ──
        if (cachedTexture) {
            SDL_SetTextureAlphaMod(cachedTexture, 255);
            SDL_SetTextureColorMod(cachedTexture, 255, 255, 255);
            SDL_RenderCopy(renderer, cachedTexture, nullptr, &rect);
        }

        // Blue overlay fades in over the card
        const Uint8 blueAlpha = static_cast<Uint8>(chargeT * 255.0F);

        SDL_SetRenderDrawColor(renderer, PLAYER_MANA_GLOW_BORDER.r, PLAYER_MANA_GLOW_BORDER.g, PLAYER_MANA_GLOW_BORDER.b, blueAlpha);
        SDL_RenderFillRect(renderer, &rect);

        // Glow expands as charge builds
        const int maxGlow = 20;
        for (int g = 4; g >= 1; --g) {
            const int   expand    = static_cast<int>(g * 5 * chargeT);
            const Uint8 glowAlpha = static_cast<Uint8>((20.0F / g) * chargeT);
            SDL_SetRenderDrawColor(renderer, PLAYER_MANA_GLOW_BORDER.r - 20, PLAYER_MANA_GLOW_BORDER.g, PLAYER_MANA_GLOW_BORDER.b, glowAlpha);
            const SDL_Rect glowRect{
                rect.x - expand, rect.y - expand,
                rect.w + expand * 2, rect.h + expand * 2
            };
            SDL_RenderFillRect(renderer, &glowRect);
        }
        (void)maxGlow;

    } else {
        // ── Disintegrate phase: column-by-column with per-column glow ────
        for (int x = 0; x < rect.w; ++x) {
            const float xNorm   = static_cast<float>(x) / static_cast<float>(rect.w);
            const float hashVal = colHash(x, cardId);

            const float flame =
                0.12F * std::sin(xNorm * 8.0F  + time + hashVal * 6.28F) +
                0.07F * std::sin(xNorm * 15.0F - time * 1.3F + hashVal * 3.14F) +
                0.05F * std::sin(xNorm * 25.0F + time * 0.7F) +
                (hashVal - 0.5F) * 0.08F;

            const float cutoffNorm = dissolveT * 1.6F - 0.2F + flame;  // was 1.3F and -0.15F
            const int   cutoffY    = static_cast<int>(cutoffNorm * static_cast<float>(rect.h));

            if (cutoffY >= rect.h) continue;
            const int startY = std::max(0, cutoffY);
            const int height = rect.h - startY;
            if (height <= 0) continue;

            // Glow — only around remaining columns
            constexpr int glowLayers = 3;
            for (int g = glowLayers; g >= 1; --g) {
                const int   expand    = g * 4;
                const Uint8 glowAlpha = static_cast<Uint8>(20 / g);
                SDL_SetRenderDrawColor(renderer, PLAYER_MANA_GLOW_BORDER.r - 20, PLAYER_MANA_GLOW_BORDER.g, PLAYER_MANA_GLOW_BORDER.b, glowAlpha);
                // draw glow as wider/taller version of this column's remaining strip
                const SDL_Rect glowCol{
                    rect.x + x - expand,
                    rect.y + startY - expand,
                    1 + expand * 2,
                    height + expand * 2
                };
                SDL_RenderFillRect(renderer, &glowCol);
            }

            // Solid blue column
            SDL_SetRenderDrawColor(renderer, 60, 140, 255, 255);
            const SDL_Rect col{rect.x + x, rect.y + startY, 1, height};
            SDL_RenderFillRect(renderer, &col);
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
    constexpr Uint32 ttlMs = 2000U;  // if no DISCARD arrives within 2s, remove it
    for (auto it = sPending.begin(); it != sPending.end(); ) {
        if (now - it->second.second > ttlMs) {
            it = sPending.erase(it);
        } else {
            ++it;
        }
    }
}
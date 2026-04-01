#include "animation/FatigueAnimation.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <algorithm>
#include <cmath>
#include <sstream>

namespace {

SDL_Color lerpColor(SDL_Color a, SDL_Color b, float t) {
    t = std::clamp(t, 0.f, 1.f);
    return SDL_Color{
        static_cast<Uint8>(a.r + static_cast<int>((b.r - a.r) * t)),
        static_cast<Uint8>(a.g + static_cast<int>((b.g - a.g) * t)),
        static_cast<Uint8>(a.b + static_cast<int>((b.b - a.b) * t)),
        static_cast<Uint8>(a.a + static_cast<int>((b.a - a.a) * t))
    };
}

SDL_Color fireColor(float lifeNorm) {
    static const SDL_Color kYellow{ 255, 220,  50, 230 };
    static const SDL_Color kOrange{ 255, 100,  10, 200 };
    static const SDL_Color kRed   { 180,  20,   0,   0 };

    if (lifeNorm > 0.5f)
        return lerpColor(kOrange, kYellow, (lifeNorm - 0.5f) * 2.f);
    else
        return lerpColor(kRed, kOrange, lifeNorm * 2.f);
}

void fillCircle(SDL_Renderer* r, int cx, int cy, int radius) {
    for (int dy = -radius; dy <= radius; ++dy) {
        const int dx = static_cast<int>(
            std::sqrt(static_cast<double>(radius * radius - dy * dy)));
        SDL_RenderDrawLine(r, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────

FatigueAnimation::FatigueAnimation(const SDL_Rect& deckRect_,
                                   int             fatigueDamage_,
                                   Uint32          durationMs,
                                   TTF_Font*       font_)
    : deckRect(deckRect_)
    , fatigueDamage(fatigueDamage_)
    , durationSeconds(static_cast<float>(std::max<Uint32>(durationMs, 1U)) / 1000.f)
    , font(font_)
    , rng(std::random_device{}())
    , perimDist(0.f, 1.f)   // placeholder — reset to correct range in spawnParticle
{
    std::ostringstream oss;
    oss << '-' << fatigueDamage_;
    damageStr = oss.str();

    particles.reserve(256);
}

FatigueAnimation::~FatigueAnimation() {
    destroyLabelTexture();
}

void FatigueAnimation::destroyLabelTexture() const {
    if (labelTexture) {
        SDL_DestroyTexture(labelTexture);
        labelTexture = nullptr;
    }
}

// ─────────────────────────────────────────────────────────────────────────────

void FatigueAnimation::start() {
    elapsedSeconds = 0.f;
    finished       = false;
    spawnAccum     = 0.f;
    particles.clear();

    // Invalidate cached texture so it's rebuilt on first render() after restart
    destroyLabelTexture();

    label.x     = static_cast<float>(deckRect.x + deckRect.w / 2);
    label.y     = static_cast<float>(deckRect.y + deckRect.h / 2);
    label.alpha = 255.f;
}

void FatigueAnimation::update(float dt) {
    if (finished) return;
    dt = std::max(dt, 0.f);

    elapsedSeconds += dt;
    const float progress = elapsedSeconds / durationSeconds;

    if (progress < 0.8f) {
        spawnAccum += SPAWN_RATE_PER_SEC * dt;
        while (spawnAccum >= 1.f) {
            spawnParticle();
            spawnAccum -= 1.f;
        }
    }

    // Update particles — swap-and-pop dead ones to avoid O(n) shifts
    for (std::size_t i = 0; i < particles.size(); ) {
        auto& p = particles[i];
        p.x    += p.vx * dt;
        p.y    += p.vy * dt;
        p.life -= dt / p.maxLife;

        if (p.life <= 0.f) {
            // Swap with last and pop — O(1), order doesn't matter for particles
            particles[i] = particles.back();
            particles.pop_back();
        } else {
            ++i;
        }
    }

    label.y -= LABEL_RISE_PX_S * dt;
    if (progress >= LABEL_FADE_START) {
        const float fadeT = (progress - LABEL_FADE_START) / (1.f - LABEL_FADE_START);
        label.alpha = 255.f * std::clamp(1.f - fadeT, 0.f, 1.f);
    }

    if (progress >= 1.f && particles.empty()) {
        finished = true;
    }
}

bool FatigueAnimation::isFinished() const {
    return finished;
}

// ─────────────────────────────────────────────────────────────────────────────

void FatigueAnimation::render(SDL_Renderer* renderer) const {
    if (!renderer || finished) return;

    SDL_BlendMode prevBlend{};
    SDL_GetRenderDrawBlendMode(renderer, &prevBlend);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    for (const auto& p : particles) {
        const SDL_Color c = fireColor(p.life);
        SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
        const int r = std::max(1, static_cast<int>(p.radius * p.life));
        fillCircle(renderer, static_cast<int>(p.x), static_cast<int>(p.y), r);
    }

    const Uint8 alpha = static_cast<Uint8>(std::clamp(label.alpha, 0.f, 255.f));
    if (alpha > 0) {
        if (font) {
            // Build the texture exactly once; reuse every subsequent frame
            if (!labelTexture) {
                const SDL_Color white{255, 255, 255, 255};
                SDL_Surface* surf = TTF_RenderUTF8_Blended(font, damageStr.c_str(), white);
                if (surf) {
                    labelTexture = SDL_CreateTextureFromSurface(renderer, surf);
                    if (labelTexture) {
                        labelTexW = surf->w;
                        labelTexH = surf->h;
                        // Tint the texture red-orange once at creation
                        SDL_SetTextureColorMod(labelTexture, 220, 60, 30);
                        SDL_SetTextureBlendMode(labelTexture, SDL_BLENDMODE_BLEND);
                    }
                    SDL_FreeSurface(surf);
                }
            }

            if (labelTexture) {
                SDL_SetTextureAlphaMod(labelTexture, alpha);
                const SDL_Rect dst{
                    static_cast<int>(label.x) - labelTexW / 2,
                    static_cast<int>(label.y) - labelTexH / 2,
                    labelTexW,
                    labelTexH
                };
                SDL_RenderCopy(renderer, labelTexture, nullptr, &dst);
            }
        } else {
            SDL_SetRenderDrawColor(renderer, 220, 60, 30, alpha);
            const SDL_Rect sq{
                static_cast<int>(label.x) - 8,
                static_cast<int>(label.y) - 8,
                16, 16
            };
            SDL_RenderFillRect(renderer, &sq);
        }
    }

    SDL_SetRenderDrawBlendMode(renderer, prevBlend);
}

// ─────────────────────────────────────────────────────────────────────────────

int FatigueAnimation::cornerRadius() const {
    return std::max(4, deckRect.w / 7);
}

void FatigueAnimation::samplePerimeterPoint(int r, float& outX, float& outY) const {
    const float W  = static_cast<float>(deckRect.w);
    const float H  = static_cast<float>(deckRect.h);
    const float fr = static_cast<float>(r);

    const float topLen    = W - 2.f * fr;
    const float rightLen  = H - 2.f * fr;
    const float bottomLen = topLen;
    const float leftLen   = rightLen;
    const float arcLen    = (3.14159265f / 2.f) * fr;
    const float totalPerim = 2.f * (topLen + rightLen) + 4.f * arcLen;

    // Reuse perimDist with updated range rather than constructing a new one
    perimDist = std::uniform_real_distribution<float>(0.f, totalPerim);
    float t = perimDist(rng);

    const float ox = static_cast<float>(deckRect.x);
    const float oy = static_cast<float>(deckRect.y);

    auto consumeArc = [&](float cx, float cy, float startAngleDeg) {
        const float a = startAngleDeg * (3.14159265f / 180.f)
                      + (t / arcLen) * (3.14159265f / 2.f);
        outX = cx + fr * std::cos(a);
        outY = cy + fr * std::sin(a);
        t    = -1.f;
    };

    if (t < topLen)    { outX = ox + fr + t; outY = oy; return; }
    t -= topLen;
    if (t < arcLen)    { consumeArc(ox + W - fr, oy + fr,      -90.f); return; }
    t -= arcLen;
    if (t < rightLen)  { outX = ox + W; outY = oy + fr + t; return; }
    t -= rightLen;
    if (t < arcLen)    { consumeArc(ox + W - fr, oy + H - fr,    0.f); return; }
    t -= arcLen;
    if (t < bottomLen) { outX = ox + W - fr - t; outY = oy + H; return; }
    t -= bottomLen;
    if (t < arcLen)    { consumeArc(ox + fr,     oy + H - fr,   90.f); return; }
    t -= arcLen;
    if (t < leftLen)   { outX = ox; outY = oy + H - fr - t; return; }
    t -= leftLen;
    consumeArc(ox + fr, oy + fr, 180.f);
}

void FatigueAnimation::spawnParticle() {
    const int r = cornerRadius();
    float px = 0.f, py = 0.f;
    samplePerimeterPoint(r, px, py);

    Particle p;
    p.x       = px;
    p.y       = py;
    p.vx      = vxDist(rng);
    p.vy      = vyDist(rng);
    p.life    = 1.f;
    p.maxLife = lifeDist(rng);
    p.radius  = sizeDist(rng);

    particles.push_back(p);
}
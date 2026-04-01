#include "animation/FatigueAnimation.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <algorithm>
#include <cmath>
#include <sstream>

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────────────────────────────────────

namespace {

// Lerp between two SDL_Colors by t in [0,1].
SDL_Color lerpColor(SDL_Color a, SDL_Color b, float t) {
    t = std::clamp(t, 0.f, 1.f);
    return SDL_Color{
        static_cast<Uint8>(a.r + static_cast<int>((b.r - a.r) * t)),
        static_cast<Uint8>(a.g + static_cast<int>((b.g - a.g) * t)),
        static_cast<Uint8>(a.b + static_cast<int>((b.b - a.b) * t)),
        static_cast<Uint8>(a.a + static_cast<int>((b.a - a.a) * t))
    };
}

// Fire palette: yellow (hot, just-spawned) → orange → deep red (dying).
SDL_Color fireColor(float lifeNorm) {
    // lifeNorm: 1.0 = freshly spawned, 0.0 = about to die
    static const SDL_Color kYellow { 255, 220,  50, 230 };
    static const SDL_Color kOrange { 255, 100,  10, 200 };
    static const SDL_Color kRed    { 180,  20,   0,   0 };   // alpha→0 when dead

    if (lifeNorm > 0.5f)
        return lerpColor(kOrange, kYellow, (lifeNorm - 0.5f) * 2.f);
    else
        return lerpColor(kRed,    kOrange, lifeNorm * 2.f);
}

// Draw a soft filled circle (solid, no blending artefacts).
void fillCircle(SDL_Renderer* r, int cx, int cy, int radius) {
    for (int dy = -radius; dy <= radius; ++dy) {
        const int dx = static_cast<int>(
            std::sqrt(static_cast<double>(radius * radius - dy * dy)));
        SDL_RenderDrawLine(r, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
//  Construction
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
{
    std::ostringstream oss;
    oss << '-' << fatigueDamage_;
    damageStr = oss.str();

    particles.reserve(256);
}

// ─────────────────────────────────────────────────────────────────────────────
//  AnimationInterface
// ─────────────────────────────────────────────────────────────────────────────

void FatigueAnimation::start() {
    elapsedSeconds = 0.f;
    finished       = false;
    spawnAccum     = 0.f;
    particles.clear();

    // Initialise the floating label at the horizontal centre, vertical centre
    // of the deck rect.
    label.x     = static_cast<float>(deckRect.x + deckRect.w / 2);
    label.y     = static_cast<float>(deckRect.y + deckRect.h / 2);
    label.alpha = 255.f;
}

void FatigueAnimation::update(float dt) {
    if (finished) return;
    dt = std::max(dt, 0.f);

    elapsedSeconds += dt;
    const float progress = elapsedSeconds / durationSeconds;

    // ── Spawn new particles during first 80 % of the animation ───────────────
    if (progress < 0.8f) {
        spawnAccum += SPAWN_RATE_PER_SEC * dt;
        while (spawnAccum >= 1.f) {
            spawnParticle();
            spawnAccum -= 1.f;
        }
    }

    // ── Update existing particles ─────────────────────────────────────────────
    for (auto& p : particles) {
        p.x    += p.vx * dt;
        p.y    += p.vy * dt;
        p.life -= dt / p.maxLife;   // normalised drain
    }
    particles.erase(
        std::remove_if(particles.begin(), particles.end(),
                       [](const Particle& p) { return p.life <= 0.f; }),
        particles.end());

    // ── Floating damage label ─────────────────────────────────────────────────
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
//  Render
// ─────────────────────────────────────────────────────────────────────────────

void FatigueAnimation::render(SDL_Renderer* renderer) const {
    if (!renderer || finished) return;

    SDL_BlendMode prevBlend{};
    SDL_GetRenderDrawBlendMode(renderer, &prevBlend);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // ── Fire particles ────────────────────────────────────────────────────────
    for (const auto& p : particles) {
        const SDL_Color c = fireColor(p.life);
        SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
        const int r = std::max(1, static_cast<int>(p.radius * p.life));
        fillCircle(renderer,
                   static_cast<int>(p.x),
                   static_cast<int>(p.y),
                   r);
    }

    // ── Floating damage label ─────────────────────────────────────────────────
    const Uint8 alpha = static_cast<Uint8>(std::clamp(label.alpha, 0.f, 255.f));
    if (alpha > 0 && font) {
        const SDL_Color textColor{220, 60, 30, alpha};   // fiery red
        SDL_Surface* surf = TTF_RenderUTF8_Blended(font, damageStr.c_str(), textColor);
        if (surf) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
            if (tex) {
                SDL_SetTextureAlphaMod(tex, alpha);
                const SDL_Rect dst{
                    static_cast<int>(label.x) - surf->w / 2,
                    static_cast<int>(label.y) - surf->h / 2,
                    surf->w, surf->h
                };
                SDL_RenderCopy(renderer, tex, nullptr, &dst);
                SDL_DestroyTexture(tex);
            }
            SDL_FreeSurface(surf);
        }
    } else if (alpha > 0 && !font) {
        // Fallback: draw a small coloured square so the effect is still visible
        // even when no font is provided.
        SDL_SetRenderDrawColor(renderer, 220, 60, 30, alpha);
        const SDL_Rect sq{
            static_cast<int>(label.x) - 8,
            static_cast<int>(label.y) - 8,
            16, 16
        };
        SDL_RenderFillRect(renderer, &sq);
    }

    SDL_SetRenderDrawBlendMode(renderer, prevBlend);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Private helpers
// ─────────────────────────────────────────────────────────────────────────────

int FatigueAnimation::cornerRadius() const {
    // Mirror RenderCard::drawCardBack: r = max(MIN_RADIUS, w/7).
    // MIN_RADIUS is Theme::Card::CARD_BACK_MIN_RADIUS (commonly 4).
    return std::max(4, deckRect.w / 7);
}

void FatigueAnimation::samplePerimeterPoint(int r, float& outX, float& outY) const {
    // Compute the perimeter of a rounded rect as four straight edges + four arcs.
    const float W  = static_cast<float>(deckRect.w);
    const float H  = static_cast<float>(deckRect.h);
    const float fr = static_cast<float>(r);

    // Straight-edge lengths
    const float topLen    = W - 2.f * fr;
    const float rightLen  = H - 2.f * fr;
    const float bottomLen = topLen;
    const float leftLen   = rightLen;

    // Arc length = pi/2 * r  (one quarter-circle)
    const float arcLen    = (3.14159265f / 2.f) * fr;

    const float totalPerim = 2.f * (topLen + rightLen) + 4.f * arcLen;

    // Uniformly pick a point on the perimeter.
    std::uniform_real_distribution<float> dist(0.f, totalPerim);
    float t = dist(rng);

    const float ox = static_cast<float>(deckRect.x);
    const float oy = static_cast<float>(deckRect.y);

    // Walk the segments: top edge → top-right arc → right edge → bottom-right arc
    //                  → bottom edge → bottom-left arc → left edge → top-left arc
    auto consumeArc = [&](float cx, float cy, float startAngleDeg) {
        const float a = startAngleDeg * (3.14159265f / 180.f)
                      + (t / arcLen) * (3.14159265f / 2.f);
        outX = cx + fr * std::cos(a);
        outY = cy + fr * std::sin(a);
        t    = -1.f;   // consumed
    };

    // Top straight
    if (t < topLen) {
        outX = ox + fr + t;
        outY = oy;
        return;
    }
    t -= topLen;

    // Top-right arc  (centre: ox+W-fr, oy+fr), start angle = -90°
    if (t < arcLen) { consumeArc(ox + W - fr, oy + fr, -90.f); return; }
    t -= arcLen;

    // Right straight
    if (t < rightLen) {
        outX = ox + W;
        outY = oy + fr + t;
        return;
    }
    t -= rightLen;

    // Bottom-right arc  (centre: ox+W-fr, oy+H-fr), start angle = 0°
    if (t < arcLen) { consumeArc(ox + W - fr, oy + H - fr, 0.f); return; }
    t -= arcLen;

    // Bottom straight  (right→left)
    if (t < bottomLen) {
        outX = ox + W - fr - t;
        outY = oy + H;
        return;
    }
    t -= bottomLen;

    // Bottom-left arc  (centre: ox+fr, oy+H-fr), start angle = 90°
    if (t < arcLen) { consumeArc(ox + fr, oy + H - fr, 90.f); return; }
    t -= arcLen;

    // Left straight  (bottom→top)
    if (t < leftLen) {
        outX = ox;
        outY = oy + H - fr - t;
        return;
    }
    t -= leftLen;

    // Top-left arc  (centre: ox+fr, oy+fr), start angle = 180°
    consumeArc(ox + fr, oy + fr, 180.f);
}

void FatigueAnimation::spawnParticle() {
    const int r = cornerRadius();

    float px = 0.f, py = 0.f;
    samplePerimeterPoint(r, px, py);

    // Velocity: upward bias with horizontal spread, small inward nudge.
    std::uniform_real_distribution<float> vxDist(-18.f, 18.f);
    std::uniform_real_distribution<float> vyDist(-55.f, -25.f);
    std::uniform_real_distribution<float> lifeDist(0.35f, 0.75f);
    std::uniform_real_distribution<float> sizeDist(2.5f, 4.5f);

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
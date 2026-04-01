#include "render/RenderCard.hpp"

#include "objects/Card.h"
#include "core/NetworkClient.hpp"
#include "objects/CreatureCard.h"
#include "render/RenderText.hpp"
#include "render/Theme.hpp"
#include "utils/RenderUtil.hpp"
#include "utils/EnvUtil.hpp"
#include "utils/StringUtil.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib/httplib.h"

// ─────────────────────────────────────────────────────────────────────────────
// NOTE – Card.h / CreatureCard.h recommended change (not required, but cheap):
//
//   In Card:        virtual const CreatureCard* asCreature() const { return nullptr; }
//   In CreatureCard: const CreatureCard* asCreature() const override { return this; }
//
// Then replace every dynamic_cast<const CreatureCard*>(&card) in this file with
// card.asCreature().  This removes RTTI overhead from the hot draw path entirely.
// ─────────────────────────────────────────────────────────────────────────────

namespace {

// ═════════════════════════════════════════════════════════════════════════════
//  Frame & render-target state
// ═════════════════════════════════════════════════════════════════════════════

uint32_t gCurrentFrame          = 0;
bool     gRenderTargetChecked   = false;
bool     gRenderTargetSupported = false;

// ═════════════════════════════════════════════════════════════════════════════
//  Image cache  (identical to original – HTTP fetch already happens at startup)
// ═════════════════════════════════════════════════════════════════════════════

std::unordered_map<int, SDL_Texture*> gCardImageCache;
std::unordered_map<int, int>          gCardImageFailureCount;
std::unordered_map<int, Uint32>       gCardImageNextRetryTick;
Uint32 gNextCardImageFetchTick = 0;

constexpr Uint32 kBaseCardImageRetryDelayMs = 120;
constexpr Uint32 kMaxCardImageRetryDelayMs  = 1500;

void scheduleCardImageRetry(int cardId, Uint32 now) {
    int& failureCount = gCardImageFailureCount[cardId];
    ++failureCount;
    Uint32 delay = kBaseCardImageRetryDelayMs;
    const int steps = std::max(0, std::min(failureCount - 1, 4));
    for (int i = 0; i < steps; ++i) {
        if (delay >= kMaxCardImageRetryDelayMs / 2) { delay = kMaxCardImageRetryDelayMs; break; }
        delay *= 2;
    }
    gCardImageNextRetryTick[cardId] = now + std::min(delay, kMaxCardImageRetryDelayMs);
}

bool downloadImageBody(const std::string& host, int port, int cardId,
                       const std::string& extension, std::string& responseBody) {
    const std::string path = "/cards/images/" + std::to_string(cardId) + "." + extension;
    std::string normalizedHost = host;
    if (normalizedHost.rfind("https://", 0) == 0) {
        normalizedHost = normalizedHost.substr(8);
    }

    const bool useHttps = true;
    const bool verifyTlsCerts = EnvUtil::getEnvBoolOrDefault("TLS_VERIFY_CERTS", EnvUtil::isAwsEnabled());
    httplib::Result res;
    if (useHttps) {
        httplib::SSLClient client(normalizedHost.c_str(), port);
        client.enable_server_certificate_verification(verifyTlsCerts);
        client.set_follow_location(true);
        client.set_connection_timeout(0, 150000);
        client.set_read_timeout(0, 250000);
        client.set_write_timeout(0, 250000);
        res = client.Get(path.c_str());
    }
    if (!res || res->status != 200) { responseBody.clear(); return false; }
    responseBody = res->body;
    return !responseBody.empty();
}

SDL_Texture* getCardImageTexture(SDL_Renderer* renderer, int cardId,
                                  bool throttleFetches = true) {
    if (!renderer || cardId <= 0) return nullptr;

    const auto cacheIt = gCardImageCache.find(cardId);
    if (cacheIt != gCardImageCache.end()) return cacheIt->second;

    const Uint32 now = SDL_GetTicks();
    const auto retryIt = gCardImageNextRetryTick.find(cardId);
    if (throttleFetches && retryIt != gCardImageNextRetryTick.end()
            && now < retryIt->second) return nullptr;
    if (throttleFetches) {
        if (now < gNextCardImageFetchTick) return nullptr;
        gNextCardImageFetchTick = now + 40;
    }

    const std::string host         = EnvUtil::getCardsServiceHost();
    const int         port         = EnvUtil::getCardsServicePort();
    const std::string preferredExt = EnvUtil::getEnvOrDefault("CARD_IMAGE_EXT", "");
    const std::array<std::string, 4> defaultExts{{"png", "jpg", "jpeg", "bmp"}};

    std::string imageBytes;
    bool loaded = false;
    if (!preferredExt.empty())
        loaded = downloadImageBody(host, port, cardId, preferredExt, imageBytes);
    if (!loaded) {
        for (const std::string& ext : defaultExts) {
            if (!preferredExt.empty() && preferredExt == ext) continue;
            if (downloadImageBody(host, port, cardId, ext, imageBytes)) { loaded = true; break; }
        }
    }
    if (!loaded) { scheduleCardImageRetry(cardId, now); return nullptr; }

    SDL_RWops* rw = SDL_RWFromConstMem(imageBytes.data(), static_cast<int>(imageBytes.size()));
    if (!rw)                         { scheduleCardImageRetry(cardId, now); return nullptr; }
    SDL_Surface* surface = IMG_Load_RW(rw, 1);
    if (!surface)                    { scheduleCardImageRetry(cardId, now); return nullptr; }
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    if (!texture)                    { scheduleCardImageRetry(cardId, now); return nullptr; }

    gCardImageCache[cardId] = texture;
    gCardImageFailureCount.erase(cardId);
    gCardImageNextRetryTick.erase(cardId);
    return texture;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Text cache
//
//  RenderText is a stateless utility (all-static), so the cache lives here.
//  Each unique (text, font, color, wrapWidth, wrapAlign) triple is rasterised
//  once and stored as an SDL_Texture*.  Subsequent draws are a single
//  SDL_RenderCopy.  Entries are evicted by RenderCard::evictTextCache().
// ═════════════════════════════════════════════════════════════════════════════

struct TextCacheKey {
    std::string text;
    TTF_Font*   font      = nullptr;
    SDL_Color   color     = {};
    int         wrapWidth = 0;             // 0 → single-line (no wrap)
    int         wrapAlign = TTF_WRAPPED_ALIGN_LEFT;

    bool operator==(const TextCacheKey& o) const noexcept {
        return font      == o.font
            && color.r   == o.color.r  && color.g == o.color.g
            && color.b   == o.color.b  && color.a == o.color.a
            && wrapWidth == o.wrapWidth && wrapAlign == o.wrapAlign
            && text      == o.text;
    }
};

struct TextCacheKeyHash {
    size_t operator()(const TextCacheKey& k) const noexcept {
        size_t h = std::hash<std::string>{}(k.text);
        auto mix = [&](size_t v) {
            h ^= v + 0x9e3779b9u + (h << 6) + (h >> 2);
        };
        mix(std::hash<void*>{}(static_cast<void*>(k.font)));
        mix(static_cast<size_t>(k.wrapWidth));
        mix(static_cast<size_t>(k.wrapAlign));
        mix(  static_cast<size_t>(k.color.r)
            | (static_cast<size_t>(k.color.g) <<  8)
            | (static_cast<size_t>(k.color.b) << 16)
            | (static_cast<size_t>(k.color.a) << 24));
        return h;
    }
};

struct TextCacheEntry {
    SDL_Texture* texture   = nullptr;
    int          w         = 0;
    int          h         = 0;
    uint32_t     lastFrame = 0;
};

std::unordered_map<TextCacheKey, TextCacheEntry, TextCacheKeyHash> gTextCache;

// Returns a resident (or freshly-rasterised) cache entry, or nullptr on error.
const TextCacheEntry* getOrCreateText(SDL_Renderer* renderer, TextCacheKey key) {
    auto& entry = gTextCache[key];
    if (entry.texture) {
        entry.lastFrame = gCurrentFrame;
        return &entry;
    }
    SDL_Surface* surf = nullptr;
    if (key.wrapWidth > 0) {
        TTF_SetFontWrappedAlign(key.font, key.wrapAlign);
        surf = TTF_RenderUTF8_Blended_Wrapped(key.font, key.text.c_str(), key.color,
                                               static_cast<Uint32>(key.wrapWidth));
    } else {
        surf = TTF_RenderUTF8_Blended(key.font, key.text.c_str(), key.color);
    }
    if (!surf) return nullptr;
    entry.texture   = SDL_CreateTextureFromSurface(renderer, surf);
    entry.w         = surf->w;
    entry.h         = surf->h;
    entry.lastFrame = gCurrentFrame;
    SDL_FreeSurface(surf);
    return entry.texture ? &entry : nullptr;
}

// Draws single-line text at (x, y) from cache.
void drawTextCached(SDL_Renderer* renderer, const std::string& text,
                    TTF_Font* font, SDL_Color color, int x, int y) {
    if (text.empty() || !font) return;
    const auto* e = getOrCreateText(renderer, {text, font, color, 0, TTF_WRAPPED_ALIGN_LEFT});
    if (e && e->texture) {
        const SDL_Rect dst{x, y, e->w, e->h};
        SDL_RenderCopy(renderer, e->texture, nullptr, &dst);
    }
}

// Draws centre-aligned text inside `rect` from cache.
void drawCenteredTextCached(SDL_Renderer* renderer, const std::string& text,
                             TTF_Font* font, SDL_Color color, const SDL_Rect& rect) {
    if (text.empty() || !font) return;
    const auto* e = getOrCreateText(renderer, {text, font, color, 0, TTF_WRAPPED_ALIGN_LEFT});
    if (e && e->texture) {
        const SDL_Rect dst{
            rect.x + (rect.w - e->w) / 2,
            rect.y + (rect.h - e->h) / 2,
            e->w, e->h
        };
        SDL_RenderCopy(renderer, e->texture, nullptr, &dst);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  Render-string cache
//
//  Avoids per-frame std::to_string / StringUtil::toUpper calls.
//  Strings are regenerated only when the underlying value actually changes.
// ═════════════════════════════════════════════════════════════════════════════

struct CardRenderStrings {
    std::string manaCost;
    std::string manaValue;
    std::string power;
    std::string toughness;
    std::string typeLabel;

    // Sentinels guarantee first-call population.
    int cachedManaCost  = INT_MIN;
    int cachedManaValue = INT_MIN;
    int cachedPower     = INT_MIN;
    int cachedToughness = INT_MIN;

    void refresh(const Card& card, const CreatureCard* creature) {
        if (card.getManaCost() != cachedManaCost) {
            manaCost       = std::to_string(card.getManaCost());
            cachedManaCost = card.getManaCost();
        }
        const int mv = std::max(0, card.getManaValue());
        if (mv != cachedManaValue) {
            manaValue       = std::to_string(mv);
            cachedManaValue = mv;
        }
        typeLabel = (card.getType() == CardType::Creature) ? "CREATURE" : "SPELL";
        if (creature) {
            if (creature->getPower() != cachedPower) {
                power       = std::to_string(creature->getPower());
                cachedPower = creature->getPower();
            }
            if (creature->getToughness() != cachedToughness) {
                toughness       = std::to_string(creature->getToughness());
                cachedToughness = creature->getToughness();
            }
        }
    }
};

std::unordered_map<int, CardRenderStrings> gCardStrings;

// ═════════════════════════════════════════════════════════════════════════════
//  Card render-to-texture cache
//
//  Each card is rendered into an offscreen SDL_Texture once and reused every
//  subsequent frame.  A fingerprint of every visually-relevant field is
//  compared before each draw; only a mismatch triggers a re-render.
// ═════════════════════════════════════════════════════════════════════════════

// Layout mode must be defined here so the fingerprint can encode it.
enum class CardLayoutMode : uint8_t { Hand = 0, Expanded = 1, Board = 2 };

struct CardVisualFingerprint {
    int      w             = -1;
    int      h             = -1;
    int      manaCost      = 0;
    int      manaValue     = 0;
    int      power         = 0;
    int      toughness     = 0;
    uint64_t effectsHash   = 0;
    size_t   textHash      = 0;
    bool     hasGranted    = false;
    bool     dimmed        = false;
    bool     artCached     = false;
    int      scrollOffset  = 0;
    uint8_t  mode          = 255;   // 255 = uninitialised sentinel

    bool operator==(const CardVisualFingerprint& o) const noexcept {
        return w == o.w && h == o.h
            && manaCost == o.manaCost && manaValue == o.manaValue
            && power    == o.power    && toughness == o.toughness
            && effectsHash == o.effectsHash && textHash == o.textHash
            && hasGranted  == o.hasGranted
            && dimmed      == o.dimmed   && artCached == o.artCached
            && scrollOffset == o.scrollOffset && mode == o.mode;
    }
    bool operator!=(const CardVisualFingerprint& o) const noexcept { return !(*this == o); }
};

struct CardRenderEntry {
    SDL_Texture*          texture = nullptr;
    CardVisualFingerprint lastFP;
    bool                  dirty   = true;   // true → force rebuild on next draw
};

std::unordered_map<int, CardRenderEntry> gCardRenderCache;

CardVisualFingerprint computeFingerprint(const Card& card, int w, int h,
                                          bool dimmed, int scrollOffset,
                                          CardLayoutMode mode) {
    CardVisualFingerprint fp;
    fp.w            = w;
    fp.h            = h;
    fp.manaCost     = card.getManaCost();
    fp.manaValue    = card.getManaValue();
    fp.dimmed       = dimmed;
    fp.artCached    = (gCardImageCache.find(card.getId()) != gCardImageCache.end());
    fp.scrollOffset = scrollOffset;
    fp.mode         = static_cast<uint8_t>(mode);
    fp.hasGranted   = card.hasGrantedEffects();
    fp.textHash     = std::hash<std::string>{}(card.getText());

    // NOTE: replace with card.asCreature() once the virtual accessor is added.
    if (const auto* c = dynamic_cast<const CreatureCard*>(&card)) {
        fp.power     = c->getPower();
        fp.toughness = c->getToughness();
        uint64_t h64 = 0;
        for (const auto& e : c->getActiveEffects())
            h64 ^= std::hash<std::string>{}(e) + 0x9e3779b97f4a7c15ULL
                   + (h64 << 12) + (h64 >> 4);
        fp.effectsHash = h64;
    }
    return fp;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Forward declarations
// ═════════════════════════════════════════════════════════════════════════════

void drawTemplateLayout(SDL_Renderer*, const Card&, const SDL_Rect&,
                        TTF_Font*, TTF_Font*, bool, int, CardLayoutMode);

// ═════════════════════════════════════════════════════════════════════════════
//  Render-to-texture wrapper
//
//  Returns a cached SDL_Texture* containing the fully-composited card face.
//  On cache miss (or first call) it renders into the offscreen texture and
//  stores it.  Returns nullptr only if render-to-texture is unsupported, in
//  which case drawTemplateLayout has already been called directly.
// ═════════════════════════════════════════════════════════════════════════════

SDL_Texture* getOrRenderCard(SDL_Renderer* renderer,
                              const Card& card, const SDL_Rect& rect,
                              TTF_Font* titleFont, TTF_Font* bodyFont,
                              bool dimmed, int scrollOffset, CardLayoutMode mode) {
    // Lazy check for render-target support (hardware renderer required).
    if (!gRenderTargetChecked) {
        gRenderTargetSupported = (SDL_RenderTargetSupported(renderer) == SDL_TRUE);
        gRenderTargetChecked   = true;
        if (!gRenderTargetSupported)
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                "RenderCard: render targets unsupported – card cache disabled.");
    }

    // Software renderer fallback: draw directly every frame.
    if (!gRenderTargetSupported) {
        drawTemplateLayout(renderer, card, rect, titleFont, bodyFont,
                           dimmed, scrollOffset, mode);
        return nullptr;
    }

    const int id = card.getId();
    if (id <= 0) {
        // Invalid ID – no cache possible.
        drawTemplateLayout(renderer, card, rect, titleFont, bodyFont,
                           dimmed, scrollOffset, mode);
        return nullptr;
    }

    const CardVisualFingerprint fp =
        computeFingerprint(card, rect.w, rect.h, dimmed, scrollOffset, mode);

    auto& entry = gCardRenderCache[id];

    const bool sizeChanged  = entry.texture
                              && (entry.lastFP.w != rect.w || entry.lastFP.h != rect.h);
    const bool needsRebuild = entry.dirty || !entry.texture
                              || sizeChanged || (fp != entry.lastFP);

    if (!needsRebuild) return entry.texture;

    // (Re)create the offscreen texture when dimensions change or it's absent.
    if (!entry.texture || sizeChanged) {
        SDL_DestroyTexture(entry.texture);
        entry.texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                          SDL_TEXTUREACCESS_TARGET, rect.w, rect.h);
        if (!entry.texture) {
            // Allocation failed – direct draw fallback.
            drawTemplateLayout(renderer, card, rect, titleFont, bodyFont,
                               dimmed, scrollOffset, mode);
            return nullptr;
        }
        SDL_SetTextureBlendMode(entry.texture, SDL_BLENDMODE_BLEND);
    }

    // Render the card into the offscreen texture.
    SDL_Texture* const previousTarget = SDL_GetRenderTarget(renderer);
    SDL_SetRenderTarget(renderer, entry.texture);

    // Clear to transparent (BLENDMODE_NONE so we overwrite, not blend-over).
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);
    // Restore blend mode for card drawing operations.
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // All coordinates are origin-relative inside the offscreen texture.
    const SDL_Rect localRect{0, 0, rect.w, rect.h};
    drawTemplateLayout(renderer, card, localRect, titleFont, bodyFont,
                       dimmed, scrollOffset, mode);

    SDL_SetRenderTarget(renderer, previousTarget);

    entry.lastFP = fp;
    entry.dirty  = false;
    return entry.texture;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Art panel
//
//  Row-clip loops are now limited to (cornerRadius) rows per corner instead
//  of the full panel height.  The centre strip is drawn with a single
//  SDL_RenderSetClipRect + SDL_RenderCopy.
// ═════════════════════════════════════════════════════════════════════════════

void drawArtPanel(SDL_Renderer* renderer, const SDL_Rect& artRect,
                  int cardId, SDL_Color fallbackColor) {
    const int inset = Theme::Card::ART_INSET;
    const SDL_Rect clipRect{artRect.x + inset, artRect.y + inset,
                            artRect.w - inset * 2, artRect.h - inset * 2};
    const int r = std::max(2, clipRect.w / 20);   // corner radius

    SDL_Texture* cardImage = getCardImageTexture(renderer, cardId);
    if (!cardImage) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        RenderUtil::fillRoundedRect(renderer, clipRect, r, fallbackColor);
        return;
    }

    // Compute "cover" scale (fill panel, crop overflow axis).
    int imgW = 0, imgH = 0;
    SDL_QueryTexture(cardImage, nullptr, nullptr, &imgW, &imgH);
    SDL_Rect imgRect = clipRect;
    if (imgW > 0 && imgH > 0) {
        const float scaleX = static_cast<float>(clipRect.w) / static_cast<float>(imgW);
        const float scaleY = static_cast<float>(clipRect.h) / static_cast<float>(imgH);
        const float scale  = std::max(scaleX, scaleY);
        const int scaledW  = static_cast<int>(imgW * scale);
        const int scaledH  = static_cast<int>(imgH * scale);
        imgRect = {
            clipRect.x + (clipRect.w - scaledW) / 2,
            clipRect.y + (clipRect.h - scaledH) / 2,
            scaledW, scaledH
        };
    }

    // Precompute per-row corner indents (avoids sqrt in the draw loop).
    // cornerIndent[i] = horizontal indent for the i-th row from a corner edge.
    std::vector<int> cornerIndent(static_cast<size_t>(r));
    for (int i = 0; i < r; ++i) {
        const int dy = r - i;   // distance from circle centre (top corners)
        cornerIndent[static_cast<size_t>(i)] =
            r - static_cast<int>(std::sqrt(static_cast<double>(r * r - dy * dy)));
    }

    // ── Centre strip: single clip + single RenderCopy ─────────────────────
    const int stripY = clipRect.y + r;
    const int stripH = clipRect.h - r * 2;
    if (stripH > 0) {
        const SDL_Rect stripClip{clipRect.x, stripY, clipRect.w, stripH};
        SDL_RenderSetClipRect(renderer, &stripClip);
        SDL_RenderCopy(renderer, cardImage, nullptr, &imgRect);
    }

    // ── Top corners: r rows ───────────────────────────────────────────────
    for (int i = 0; i < r; ++i) {
        const int indent = cornerIndent[static_cast<size_t>(i)];
        const SDL_Rect rowClip{clipRect.x + indent, clipRect.y + i,
                               std::max(1, clipRect.w - indent * 2), 1};
        SDL_RenderSetClipRect(renderer, &rowClip);
        SDL_RenderCopy(renderer, cardImage, nullptr, &imgRect);
    }

    // ── Bottom corners: r rows ────────────────────────────────────────────
    // dy for bottom rows runs 1 … r (same formula, mirrored axis).
    const int bottomStart = clipRect.h - r;
    for (int i = 0; i < r; ++i) {
        const int dy     = i + 1;
        const int indent = r - static_cast<int>(
            std::sqrt(static_cast<double>(r * r - dy * dy)));
        const SDL_Rect rowClip{clipRect.x + indent, clipRect.y + bottomStart + i,
                               std::max(1, clipRect.w - indent * 2), 1};
        SDL_RenderSetClipRect(renderer, &rowClip);
        SDL_RenderCopy(renderer, cardImage, nullptr, &imgRect);
    }

    SDL_RenderSetClipRect(renderer, nullptr);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Circular badge (mana cost / mana value)
//  textRenderer removed – uses the text cache directly.
// ═════════════════════════════════════════════════════════════════════════════

void drawCircularBadge(SDL_Renderer* renderer, TTF_Font* font,
                       int cx, int cy, int radius,
                       SDL_Color fill, SDL_Color border,
                       const std::string& value) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
    RenderUtil::fillCircle(renderer, cx, cy, radius);
    SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);
    RenderUtil::fillCircle(renderer, cx, cy, radius - 2);

    const auto* e = getOrCreateText(renderer, {value, font, Theme::Card::BADGE_TEXT, 0});
    if (e && e->texture) {
        const SDL_Rect dst{cx - e->w / 2, cy - e->h / 2, e->w, e->h};
        SDL_RenderCopy(renderer, e->texture, nullptr, &dst);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  Hexagonal effects badge  (unchanged)
// ═════════════════════════════════════════════════════════════════════════════

void drawEffectsBadge(SDL_Renderer* renderer, const std::string& effect,
                      int cx, int cy, int radius, float scale) {
    (void)scale;
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    RenderUtil::drawHexagon(renderer, cx, cy, radius + 2, Theme::BANNER_BORDER);
    RenderUtil::drawHexagon(renderer, cx, cy, radius,     Theme::BANNER_FILL);
    SDL_RenderSetClipRect(renderer, nullptr);
    std::string effectRaw = StringUtil::toLower(effect);
    if (effect.find("regen")) effectRaw = "regen";
    SDL_Texture* icon = RenderUtil::getIcon(renderer, effectRaw);
    if (icon) {
        int iw = 0, ih = 0;
        SDL_QueryTexture(icon, nullptr, nullptr, &iw, &ih);
        const int   iconSize = static_cast<int>(radius * 1.5f);
        const float iconScale = std::min(static_cast<float>(iconSize) / iw,
                                         static_cast<float>(iconSize) / ih);
        const int scaledW = static_cast<int>(iw * iconScale);
        const int scaledH = static_cast<int>(ih * iconScale);
        const SDL_Rect dst{cx - scaledW / 2, cy - scaledH / 2, scaledW, scaledH};
        SDL_RenderCopy(renderer, icon, nullptr, &dst);
    } else {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "drawEffectsBadge: missing icon for '%s'", effect.c_str());
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  Master card layout
//
//  Key changes vs. original:
//   • All text drawn via drawTextCached / drawCenteredTextCached (no per-frame
//     TTF rasterisation or SDL_CreateTextureFromSurface calls).
//   • String values (ATK, DEF, mana, type) pulled from gCardStrings which
//     only recomputes when the underlying value changes.
//   • Bottom-bar row loop reduced from bottomRect.h iterations to
//     bottomCornerRadius iterations (straight portion filled with one rect).
//   • dynamic_cast is the only remaining per-frame RTTI call; replace with
//     card.asCreature() once the virtual accessor is added to Card.h.
// ═════════════════════════════════════════════════════════════════════════════

void drawTemplateLayout(SDL_Renderer* renderer, const Card& card, const SDL_Rect& rect,
                        TTF_Font* titleFont, TTF_Font* bodyFont,
                        bool dimmed, int scrollOffset, CardLayoutMode mode) {

    const bool showTextBox   = (mode == CardLayoutMode::Expanded);
    const bool showManaValue = (mode != CardLayoutMode::Board);
    const bool expandedMode  = (mode == CardLayoutMode::Expanded);
    const bool compactText   = (mode == CardLayoutMode::Hand);
    const bool boardMode     = (mode == CardLayoutMode::Board);

    TTF_Font* uiFont      = bodyFont ? bodyFont : titleFont;
    TTF_Font* compactFont = uiFont;
    if (titleFont && TTF_FontHeight(titleFont) < TTF_FontHeight(compactFont))
        compactFont = titleFont;

    int screenW = 0, screenH = 0;
    SDL_GetRendererOutputSize(renderer, &screenW, &screenH);
    const float scale = (screenW > 0 && screenH > 0)
        ? std::min(static_cast<float>(screenW) / 1200.0F,
                   static_cast<float>(screenH) /  850.0F)
        : 1.0F;
    auto sc = [scale](int v) { return std::max(1, static_cast<int>(v * scale)); };

    // NOTE: replace with card.asCreature() once added to Card.h.
    const auto* creature = dynamic_cast<const CreatureCard*>(&card);

    // Refresh cached render strings for this card.
    const int id = card.getId();
    auto& strs = gCardStrings[id];
    strs.refresh(card, creature);

    // ── Border & colour ───────────────────────────────────────────────────────
    const int cornerRadius      = std::max(sc(Theme::Card::MIN_CORNER_RADIUS), rect.w / 20);
    const int borderThickness   = expandedMode
        ? sc(Theme::Card::EXPANDED_BORDER_THICKNESS)
        : sc(Theme::Card::BORDER_THICKNESS);

    SDL_Color borderColor = card.getType() == CardType::Creature
        ? Theme::Card::CREATURE_BORDER : Theme::Card::SPELL_BORDER;
    SDL_Color baseColor   = card.getType() == CardType::Creature
        ? Theme::Card::CREATURE_BASE   : Theme::Card::SPELL_BASE;
    if (dimmed) { borderColor = Theme::Card::DIMMED_BORDER; baseColor = Theme::Card::DIMMED_BASE; }

    // ── Card background ───────────────────────────────────────────────────────
    RenderUtil::fillRoundedRect(renderer, rect, cornerRadius, baseColor);
    RenderUtil::drawRoundedBorder(renderer, rect, cornerRadius, Theme::Card::OUTER_BORDER, 1);
    RenderUtil::drawRoundedBorder(renderer,
        {rect.x + 1, rect.y + 1, rect.w - 2, rect.h - 2},
        std::max(2, cornerRadius - 1), borderColor, borderThickness);

    // ── Layout geometry ───────────────────────────────────────────────────────
    const int innerPad = borderThickness + sc(Theme::Card::INNER_PADDING);
    SDL_Rect inner{rect.x + innerPad, rect.y + innerPad,
                   rect.w - innerPad * 2, rect.h - innerPad * 2};

    int artH    = std::max(sc(Theme::Card::MIN_ART_HEIGHT),
                           inner.h * (showTextBox ? (expandedMode ? 33 : 40) : 52) / 100);
    int nameH   = std::max(sc(Theme::Card::MIN_NAME_HEIGHT),
                           inner.h * (expandedMode ? 13 : 10) / 100);
    int typeH   = std::max(sc(Theme::Card::MIN_TYPE_HEIGHT),   inner.h * 8  / 100);
    int bottomH = std::max(sc(Theme::Card::MIN_BOTTOM_HEIGHT),
                           inner.h * (expandedMode ? 16 : 19) / 100);
    int textH   = showTextBox ? inner.h - artH - nameH - typeH - bottomH : 0;

    if (!showTextBox)
        bottomH = std::max(sc(Theme::Card::MIN_COLLAPSED_BOTTOM_HEIGHT),
                           inner.h - artH - nameH - typeH);

    if (showTextBox && textH < sc(Theme::Card::MIN_TEXT_HEIGHT)) {
        const int needed = sc(Theme::Card::MIN_TEXT_HEIGHT) - textH;
        artH  = std::max(sc(Theme::Card::MIN_TEXT_HEIGHT), artH - needed);
        textH = inner.h - artH - nameH - typeH - bottomH;
    }

    if (expandedMode && showTextBox) {
        const int halvedNameH = std::max(1, nameH / 2);
        const int halvedTextH = std::max(1, textH);
        artH  += (nameH - halvedNameH) + (textH - halvedTextH);
        nameH  = halvedNameH;
        textH  = halvedTextH;
    }

    const SDL_Rect artRect   {inner.x, inner.y,                                      inner.w, artH};
    const SDL_Rect nameRect  {inner.x, artRect.y  + artRect.h,                       inner.w, nameH};
    const SDL_Rect typeRect  {inner.x, nameRect.y + nameRect.h,                      inner.w, typeH};
    const SDL_Rect textRect  {inner.x, typeRect.y + typeRect.h,                      inner.w, std::max(0, textH)};
    const SDL_Rect bottomRect{
        inner.x,
        showTextBox ? textRect.y + textRect.h : typeRect.y + typeRect.h,
        inner.w, bottomH
    };

    // ── Art panel ─────────────────────────────────────────────────────────────
    const SDL_Color artFallback = dimmed
        ? Theme::Card::ART_DIMMED_FALLBACK
        : (card.getType() == CardType::Creature
            ? Theme::Card::ART_CREATURE_FALLBACK
            : Theme::Card::ART_SPELL_FALLBACK);
    drawArtPanel(renderer, artRect, card.getId(), artFallback);

    // ── Mana-cost badge (top-right) ───────────────────────────────────────────
    const int manaRadius = std::max(sc(Theme::Card::MIN_MANA_RADIUS),
                                    std::min(rect.w, rect.h) / 10);
    const int manaCx = rect.x + rect.w - manaRadius - 2;
    const int manaCy = rect.y + manaRadius + 2;
    drawCircularBadge(renderer, uiFont, manaCx, manaCy, manaRadius,
        card.getType() == CardType::Creature
            ? Theme::Card::MANA_BADGE_CREATURE_FILL
            : Theme::Card::MANA_BADGE_SPELL_FILL,
        card.getType() == CardType::Creature
            ? Theme::Card::MANA_BADGE_CREATURE_BORDER
            : Theme::Card::MANA_BADGE_SPELL_BORDER,
        strs.manaCost);

    // ── Name plate ────────────────────────────────────────────────────────────
    SDL_SetRenderDrawColor(renderer,
        Theme::Card::NAME_PLATE_FILL.r, Theme::Card::NAME_PLATE_FILL.g,
        Theme::Card::NAME_PLATE_FILL.b, Theme::Card::NAME_PLATE_FILL.a);
    SDL_RenderFillRect(renderer, &nameRect);

    const int namePadX = std::max(sc(Theme::Card::MIN_NAME_PADDING), nameRect.w / 18);
    TTF_Font* nameFont = boardMode ? compactFont : uiFont;

    if (!expandedMode) {
        // Hand / Board: single-line, truncated with ellipsis.
        const std::string nameText =
            RenderText::truncateWithEllipsis(nameFont, card.getName(),
                                             nameRect.w - namePadX * 2);
        const auto* ne = getOrCreateText(renderer, {nameText, nameFont, Theme::Card::NAME_TEXT, 0});
        if (ne && ne->texture) {
            const int nx = nameRect.x + std::max(namePadX, (nameRect.w - ne->w) / 2);
            const int ny = nameRect.y + (nameRect.h - ne->h) / 2;
            const SDL_Rect dst{nx, ny, ne->w, ne->h};
            SDL_RenderCopy(renderer, ne->texture, nullptr, &dst);
        }
    } else {
        // Expanded: wrapped, centred.
        const SDL_Rect nameClip{nameRect.x + namePadX, nameRect.y + 2,
                                std::max(1, nameRect.w - namePadX * 2),
                                std::max(1, nameRect.h - 4)};
        SDL_RenderSetClipRect(renderer, &nameClip);
        const auto* ne = getOrCreateText(renderer,
            {card.getName(), uiFont, Theme::Card::NAME_TEXT,
             nameClip.w, TTF_WRAPPED_ALIGN_CENTER});
        if (ne && ne->texture) {
            const SDL_Rect dst{
                nameClip.x + std::max(0, (nameClip.w - ne->w) / 2),
                nameClip.y + std::max(0, (nameClip.h - ne->h) / 2),
                ne->w, ne->h
            };
            SDL_RenderCopy(renderer, ne->texture, nullptr, &dst);
        }
        SDL_RenderSetClipRect(renderer, nullptr);
    }

    // ── Type line ─────────────────────────────────────────────────────────────
    const SDL_Color typeFill = card.getType() == CardType::Creature
        ? Theme::Card::TYPE_LINE_CREATURE_FILL : Theme::Card::TYPE_LINE_SPELL_FILL;
    SDL_SetRenderDrawColor(renderer, typeFill.r, typeFill.g, typeFill.b, typeFill.a);
    SDL_RenderFillRect(renderer, &typeRect);

    const int pillPadX = std::max(sc(Theme::Card::MIN_TYPE_PILL_PADDING), typeRect.w / 16);
    const SDL_Rect typePill{typeRect.x + pillPadX, typeRect.y + 2,
                            typeRect.w - pillPadX * 2, std::max(2, typeRect.h - 4)};
    RenderUtil::fillRoundedRect(renderer, typePill, std::max(4, typePill.h / 3),
        card.getType() == CardType::Creature
            ? Theme::Card::TYPE_PILL_CREATURE_FILL
            : Theme::Card::TYPE_PILL_SPELL_FILL);
    drawCenteredTextCached(renderer, strs.typeLabel, compactFont, Theme::Card::TYPE_TEXT, typePill);

    // ── Ability text box (Expanded only) ──────────────────────────────────────
    if (showTextBox && textRect.h > 0 && !compactText) {
        SDL_SetRenderDrawColor(renderer,
            Theme::Card::TEXT_BOX_FILL.r,   Theme::Card::TEXT_BOX_FILL.g,
            Theme::Card::TEXT_BOX_FILL.b,   Theme::Card::TEXT_BOX_FILL.a);
        SDL_RenderFillRect(renderer, &textRect);
        SDL_SetRenderDrawColor(renderer,
            Theme::Card::TEXT_BOX_BORDER.r, Theme::Card::TEXT_BOX_BORDER.g,
            Theme::Card::TEXT_BOX_BORDER.b, Theme::Card::TEXT_BOX_BORDER.a);
        SDL_RenderDrawRect(renderer, &textRect);

        const int tcPadH = sc(Theme::Card::TEXT_CLIP_HORIZONTAL_PADDING);
        const int tcPadV = sc(Theme::Card::TEXT_CLIP_VERTICAL_PADDING);
        const SDL_Rect textClip{
            textRect.x + tcPadH, textRect.y + tcPadV,
            std::max(1, textRect.w - tcPadH * 2),
            std::max(1, textRect.h - tcPadV * 2)
        };
        SDL_RenderSetClipRect(renderer, &textClip);

        std::vector<std::string> abilities;
        std::string flavor;
        StringUtil::splitAbilityTextAndFlavor(card.getText(), abilities, flavor);

        std::vector<std::string> textLines;
        for (const auto& line : abilities)
            if (!line.empty()) textLines.push_back(line);
        if (!flavor.empty()) textLines.push_back(flavor);

        const SDL_Color textBodyColor = card.hasGrantedEffects()
            ? Theme::Card::STAT_VALUE_BUFFED : Theme::Card::TEXT_BODY;
        const int lineGap = sc(Theme::Card::TEXT_LINE_GAP);

        // Retrieve (or create) all cached entries first, then measure total height.
        std::vector<const TextCacheEntry*> entries;
        entries.reserve(textLines.size());
        int totalH = 0;
        for (const auto& line : textLines) {
            const auto* e = getOrCreateText(renderer,
                {line, uiFont, textBodyColor, textClip.w, TTF_WRAPPED_ALIGN_CENTER});
            entries.push_back(e);
            if (e) totalH += e->h;
        }
        if (!entries.empty())
            totalH += static_cast<int>(entries.size() - 1) * lineGap;

        int cursorY = textClip.y
            + std::max(0, (textClip.h - totalH) / 2)
            - std::max(0, scrollOffset);

        for (const auto* e : entries) {
            if (e && e->texture) {
                const int drawX = textClip.x + std::max(0, (textClip.w - e->w) / 2);
                const SDL_Rect dst{drawX, cursorY, e->w, e->h};
                SDL_RenderCopy(renderer, e->texture, nullptr, &dst);
                cursorY += e->h + lineGap;
            } else {
                cursorY += lineGap;
            }
        }
        SDL_RenderSetClipRect(renderer, nullptr);
    }

    // ── Bottom bar ────────────────────────────────────────────────────────────
    // Optimised: fill the straight (non-rounded) portion with a single
    // SDL_RenderFillRect, then only loop the bottomCornerRadius corner rows.
    const int bottomCornerRadius = std::max(2, bottomRect.w / 20);

    SDL_SetRenderDrawColor(renderer,
        Theme::Card::BOTTOM_BAR_FILL.r, Theme::Card::BOTTOM_BAR_FILL.g,
        Theme::Card::BOTTOM_BAR_FILL.b, Theme::Card::BOTTOM_BAR_FILL.a);

    const int straightH = bottomRect.h - bottomCornerRadius;
    if (straightH > 0) {
        const SDL_Rect straightPart{bottomRect.x, bottomRect.y, bottomRect.w, straightH};
        SDL_RenderSetClipRect(renderer, nullptr);
        SDL_RenderFillRect(renderer, &straightPart);
    }

    // Only the bottom `bottomCornerRadius` rows need rounded clipping.
    for (int i = 0; i < bottomCornerRadius; ++i) {
        const int dy     = i + 1;
        const int indent = bottomCornerRadius - static_cast<int>(
            std::sqrt(static_cast<double>(
                bottomCornerRadius * bottomCornerRadius - dy * dy)));
        const int rowY   = bottomRect.y + (bottomRect.h - bottomCornerRadius) + i;
        const SDL_Rect rowClip{bottomRect.x + indent, rowY,
                               std::max(1, bottomRect.w - indent * 2), 1};
        SDL_RenderSetClipRect(renderer, &rowClip);
        SDL_RenderFillRect(renderer, &bottomRect);
    }
    SDL_RenderSetClipRect(renderer, nullptr);

    // ── ATK / DEF stat labels ─────────────────────────────────────────────────
    const int valueRadius = showManaValue
        ? std::max(sc(11), std::min(bottomRect.h / 2 - 3, rect.w / 10)) : 0;
    const int valueCx       = bottomRect.x + bottomRect.w / 2;
    const int leftBlockX    = bottomRect.x + sc(Theme::Card::BOTTOM_SECTION_PADDING);
    const int leftBlockW    = std::max(1, bottomRect.w / 2
                                       - sc(Theme::Card::BOTTOM_SECTION_PADDING)
                                       - sc(Theme::Card::BOTTOM_COMPACT_SECTION_GAP));
    const int rightBlockX   = bottomRect.x + bottomRect.w / 2
                              + sc(Theme::Card::BOTTOM_COMPACT_SECTION_GAP);
    const int rightBlockW   = std::max(1, (bottomRect.x + bottomRect.w
                                           - sc(Theme::Card::BOTTOM_SECTION_PADDING))
                                       - rightBlockX);
    const int statLabelY    = bottomRect.y
        + std::max(sc(Theme::Card::MIN_STAT_LABEL_TOP_PADDING), bottomRect.h / 8);
    const int statValueY    = std::min(
        bottomRect.y + bottomRect.h
            - std::max(sc(Theme::Card::MIN_STAT_BASELINE_OFFSET), bottomRect.h / 2),
        bottomRect.y + bottomRect.h - TTF_FontHeight(uiFont) - 2);

    if (creature) {
        TTF_Font* statLabelFont = compactFont;
        TTF_Font* statValueFont = uiFont;

        // Measure labels for positioning (measureText is cheap – no texture).
        int atkLabelW = 0, atkLabelH = 0, defLabelW = 0, defLabelH = 0;
        int atkValueW = 0, defValueW = 0, dummy = 0;
        RenderText::measureText(statLabelFont, "ATK", atkLabelW, atkLabelH);
        RenderText::measureText(statLabelFont, "DEF", defLabelW, defLabelH);
        RenderText::measureText(statValueFont, strs.power,     atkValueW, dummy);
        RenderText::measureText(statValueFont, strs.toughness, defValueW, dummy);

        const int atkValueX  = leftBlockX  + atkLabelW / 2 - atkValueW / 2;
        const int defLabelX  = rightBlockX + std::max(0, rightBlockW - defLabelW);
        const int defValueX  = defLabelX   + defLabelW / 2 - defValueW / 2;
        const int atkDrawY   = std::max(statValueY, bottomRect.y + 1 + atkLabelH);
        const int defDrawY   = std::max(statValueY, bottomRect.y + 1 + defLabelH);

        // ATK column (clipped to left half).
        SDL_Rect leftClip{leftBlockX, bottomRect.y, leftBlockW, bottomRect.h};
        SDL_RenderSetClipRect(renderer, &leftClip);

        drawTextCached(renderer, "ATK", statLabelFont, Theme::Card::STAT_LABEL,
                       leftBlockX, statLabelY);

        SDL_Color atkColor = Theme::Card::STAT_VALUE;
        if (creature->getPower() > creature->getBasePower())
            atkColor = Theme::Card::STAT_VALUE_BUFFED;
        else if (creature->getPower() < creature->getBasePower())
            atkColor = Theme::Card::STAT_VALUE_DEBUFFED;

        drawTextCached(renderer, strs.power, statValueFont, atkColor, atkValueX, atkDrawY);

        // DEF column (clipped to right half).
        SDL_Rect rightClip{rightBlockX, bottomRect.y, rightBlockW, bottomRect.h};
        SDL_RenderSetClipRect(renderer, &rightClip);

        drawTextCached(renderer, "DEF", statLabelFont, Theme::Card::STAT_LABEL,
                       defLabelX, statLabelY);

        SDL_Color defColor = Theme::Card::STAT_VALUE;
        if (creature->getToughness() > creature->getBaseToughness())
            defColor = Theme::Card::STAT_VALUE_BUFFED;
        else if (creature->getToughness() < creature->getBaseToughness())
            defColor = Theme::Card::STAT_VALUE_DEBUFFED;

        drawTextCached(renderer, strs.toughness, statValueFont, defColor, defValueX, defDrawY);

        SDL_RenderSetClipRect(renderer, nullptr);
    }

    // ── Mana-value badge (bottom-centre, non-board) ───────────────────────────
    if (showManaValue) {
        const int valueCy = bottomRect.y + bottomRect.h / 2;
        drawCircularBadge(renderer, uiFont, valueCx, valueCy, valueRadius,
            card.getType() == CardType::Creature
                ? Theme::Card::VALUE_BADGE_CREATURE_FILL
                : Theme::Card::VALUE_BADGE_SPELL_FILL,
            card.getType() == CardType::Creature
                ? Theme::Card::VALUE_BADGE_CREATURE_BORDER
                : Theme::Card::VALUE_BADGE_SPELL_BORDER,
            strs.manaValue);
    }

    // ── Effects badges (top-left, board + expanded) ───────────────────────────
    if ((boardMode || expandedMode) && creature) {
        const auto& effects = creature->getActiveEffects();
        if (!effects.empty()) {
            SDL_RenderSetClipRect(renderer, nullptr);
            const int badgeRadius = manaRadius;
            int badgeCx = rect.x + badgeRadius + 2;
            int badgeCy = rect.y + badgeRadius + borderThickness;
            for (const auto& effect : effects) {
                drawEffectsBadge(renderer, effect, badgeCx, badgeCy, badgeRadius, scale);
                badgeCy += static_cast<int>(badgeRadius * 1.4f);
            }
        }
    }
}

} // anonymous namespace

// ─────────────────────────────────────────────────────────────────────────────
//  Public API
// ─────────────────────────────────────────────────────────────────────────────

void RenderCard::beginFrame() {
    ++gCurrentFrame;
}

void RenderCard::evictTextCache(uint32_t maxAge) {
    for (auto it = gTextCache.begin(); it != gTextCache.end(); ) {
        if (gCurrentFrame - it->second.lastFrame > maxAge) {
            SDL_DestroyTexture(it->second.texture);
            it = gTextCache.erase(it);
        } else {
            ++it;
        }
    }
}

void RenderCard::invalidateCardCache(int cardId) {
    const auto it = gCardRenderCache.find(cardId);
    if (it != gCardRenderCache.end()) it->second.dirty = true;
}

void RenderCard::clearRenderCache() {
    for (auto& [id, entry] : gCardRenderCache)
        SDL_DestroyTexture(entry.texture);
    gCardRenderCache.clear();
    gCardStrings.clear();
}

// ── drawCardFace ──────────────────────────────────────────────────────────────
void RenderCard::drawCardFace(SDL_Renderer* renderer, RenderText& /*textRenderer*/,
                               const Card& card, const SDL_Rect& rect,
                               TTF_Font* titleFont, TTF_Font* bodyFont,
                               bool dimmed, bool compact, int scrollOffset) {
    if (!renderer || !titleFont || !bodyFont) return;
    const CardLayoutMode mode = compact ? CardLayoutMode::Hand : CardLayoutMode::Expanded;
    SDL_Texture* tex = getOrRenderCard(renderer, card, rect,
                                        titleFont, bodyFont, dimmed, scrollOffset, mode);
    if (tex) SDL_RenderCopy(renderer, tex, nullptr, &rect);
}

// ── drawHandCard ──────────────────────────────────────────────────────────────
void RenderCard::drawHandCard(SDL_Renderer* renderer, RenderText& /*textRenderer*/,
                               const Card& card, const SDL_Rect& cardRect,
                               TTF_Font* titleFont, TTF_Font* bodyFont) {
    if (!renderer || !titleFont || !bodyFont) return;
    SDL_Texture* tex = getOrRenderCard(renderer, card, cardRect,
                                        titleFont, bodyFont, false, 0,
                                        CardLayoutMode::Hand);
    if (tex) SDL_RenderCopy(renderer, tex, nullptr, &cardRect);
}

// ── drawBoardCard ─────────────────────────────────────────────────────────────
void RenderCard::drawBoardCard(SDL_Renderer* renderer, RenderText& /*textRenderer*/,
                                const Card& card, const SDL_Rect& cardRect,
                                TTF_Font* titleFont, TTF_Font* bodyFont) {
    if (!renderer || !titleFont || !bodyFont) return;
    SDL_Texture* tex = getOrRenderCard(renderer, card, cardRect,
                                        titleFont, bodyFont, false, 0,
                                        CardLayoutMode::Board);
    if (tex) SDL_RenderCopy(renderer, tex, nullptr, &cardRect);
}

// ── drawPreview ───────────────────────────────────────────────────────────────
void RenderCard::drawPreview(SDL_Renderer* renderer, RenderText& /*textRenderer*/,
                              const Card& card, const SDL_Rect& previewRect,
                              TTF_Font* bodyFont, TTF_Font* titleFont, int scrollOffset) {
    if (!renderer || !bodyFont || !titleFont) return;
    SDL_Texture* tex = getOrRenderCard(renderer, card, previewRect,
                                        titleFont, bodyFont, false, scrollOffset,
                                        CardLayoutMode::Expanded);
    if (tex) SDL_RenderCopy(renderer, tex, nullptr, &previewRect);
}

// ── drawCardBack ──────────────────────────────────────────────────────────────
void RenderCard::drawCardBack(SDL_Renderer* renderer, const SDL_Rect& cardRect) {
    if (!renderer) return;
    const int r = std::max(Theme::Card::CARD_BACK_MIN_RADIUS, cardRect.w / 7);
    RenderUtil::fillRoundedRect(renderer, cardRect, r, Theme::Card::CARD_BACK_OUTER_FILL);
    RenderUtil::drawRoundedBorder(renderer, cardRect, r, Theme::Card::CARD_BACK_OUTER_BORDER, 1);
    RenderUtil::drawRoundedBorder(renderer,
        {cardRect.x + 1, cardRect.y + 1, cardRect.w - 2, cardRect.h - 2},
        r - 1, Theme::Card::CARD_BACK_INNER_BORDER, 2);
    const SDL_Rect inset{
        cardRect.x + Theme::Card::CARD_BACK_INSET,
        cardRect.y + Theme::Card::CARD_BACK_INSET,
        cardRect.w - Theme::Card::CARD_BACK_INSET * 2,
        cardRect.h - Theme::Card::CARD_BACK_INSET * 2
    };
    if (inset.w > 4 && inset.h > 4) {
        const int ir = std::max(Theme::Card::CARD_BACK_MIN_INSET_RADIUS, r - 5);
        RenderUtil::fillRoundedRect(renderer, inset, ir, Theme::Card::CARD_BACK_INSET_FILL);
        RenderUtil::drawRoundedBorder(renderer, inset, ir, Theme::Card::CARD_BACK_INSET_BORDER, 1);
    }
}

// ── Art cache helpers ─────────────────────────────────────────────────────────
bool RenderCard::preloadCardArt(SDL_Renderer* renderer, int cardId) {
    return getCardImageTexture(renderer, cardId, false) != nullptr;
}

bool RenderCard::isCardArtCached(int cardId) {
    return gCardImageCache.find(cardId) != gCardImageCache.end();
}

void RenderCard::clearImageCache() {
    for (auto& [id, tex] : gCardImageCache)
        if (tex) SDL_DestroyTexture(tex);
    gCardImageCache.clear();
    gCardImageFailureCount.clear();
    gCardImageNextRetryTick.clear();
    gNextCardImageFetchTick = 0;
}
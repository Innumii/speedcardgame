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
#include <cstdlib>
#include <array>
#include <cctype>
#include <cmath>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib/httplib.h"


namespace {
    std::unordered_map<int, SDL_Texture*> gCardImageCache;
    std::unordered_map<int, int> gCardImageFailureCount;
    std::unordered_map<int, Uint32> gCardImageNextRetryTick;
    Uint32 gNextCardImageFetchTick = 0;

    constexpr Uint32 kBaseCardImageRetryDelayMs = 120;
    constexpr Uint32 kMaxCardImageRetryDelayMs = 1500;

    void scheduleCardImageRetry(int cardId, Uint32 now) {
        int& failureCount = gCardImageFailureCount[cardId];
        ++failureCount;

        Uint32 delay = kBaseCardImageRetryDelayMs;
        int backoffSteps = std::max(0, std::min(failureCount - 1, 4));
        for (int i = 0; i < backoffSteps; ++i) {
            if (delay >= kMaxCardImageRetryDelayMs / 2) {
                delay = kMaxCardImageRetryDelayMs;
                break;
            }
            delay *= 2;
        }

        gCardImageNextRetryTick[cardId] = now + std::min(delay, kMaxCardImageRetryDelayMs);
    }

    //switch out
    bool downloadImageBody(const std::string& host, int port, int cardId,
                       const std::string& extension, std::string& responseBody) {

    // Determine if HTTPS is needed
    bool useHttps = (port == 443);

    httplib::Result res;

    std::string path = "/cards/images/" + std::to_string(cardId) + "." + extension;

    if (useHttps) {
        httplib::SSLClient client(host.c_str(), port);
        client.enable_server_certificate_verification(false); // allow self-signed certs
        client.set_follow_location(true);
        client.set_connection_timeout(0, 150000);
        client.set_read_timeout(0, 250000);
        client.set_write_timeout(0, 250000);

        res = client.Get(path.c_str());

    } else {
        httplib::Client client(host.c_str(), port);
        client.set_follow_location(true);
        client.set_connection_timeout(0, 150000);
        client.set_read_timeout(0, 250000);
        client.set_write_timeout(0, 250000);

        res = client.Get(path.c_str());
    }

    if (!res) {
        responseBody.clear();
        return false; // network error
    }

    // Check HTTP status
    if (res->status != 200) {
        responseBody.clear();
        return false;
    }

    responseBody = res->body;
    return !responseBody.empty();
}

    SDL_Texture* getCardImageTexture(SDL_Renderer* renderer, int cardId, bool throttleFetches = true) {
        if (!renderer || cardId <= 0) return nullptr;

        const auto cacheIt = gCardImageCache.find(cardId);
        if (cacheIt != gCardImageCache.end()) {
            return cacheIt->second;
        }

        const Uint32 now = SDL_GetTicks();

        const auto retryIt = gCardImageNextRetryTick.find(cardId);
        if (throttleFetches && retryIt != gCardImageNextRetryTick.end() && now < retryIt->second) {
            return nullptr;
        }

        if (throttleFetches) {
            if (now < gNextCardImageFetchTick) {
                return nullptr;
            }
            // Limit synchronous network fetches to keep frame pacing smooth.
            gNextCardImageFetchTick = now + 40;
        }

        const std::string host = EnvUtil::getCardsServiceHost();
        const int port = EnvUtil::getCardsServicePort();
        const std::string preferredExt = EnvUtil::getEnvOrDefault("CARD_IMAGE_EXT", "");
        const std::array<std::string, 4> defaultExts{{"png", "jpg", "jpeg", "bmp"}};

        std::string imageBytes;
        bool loaded = false;

        if (!preferredExt.empty()) {
            loaded = downloadImageBody(host, port, cardId, preferredExt, imageBytes);
        }

        if (!loaded) {
            for (const std::string& ext : defaultExts) {
                if (!preferredExt.empty() && preferredExt == ext) {
                    continue;
                }
                if (downloadImageBody(host, port, cardId, ext, imageBytes)) {
                    loaded = true;
                    break;
                }
            }
        }

        if (!loaded) {
            scheduleCardImageRetry(cardId, now);
            return nullptr;
        }

        SDL_RWops* rw = SDL_RWFromConstMem(imageBytes.data(), static_cast<int>(imageBytes.size()));
        if (!rw) {
            scheduleCardImageRetry(cardId, now);
            return nullptr;
        }

        SDL_Surface* surface = IMG_Load_RW(rw, 1);
        if (!surface) {
            scheduleCardImageRetry(cardId, now);
            return nullptr;
        }

        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_FreeSurface(surface);
        if (!texture) {
            scheduleCardImageRetry(cardId, now);
            return nullptr;
        }

        gCardImageCache[cardId] = texture;
        gCardImageFailureCount.erase(cardId);
        gCardImageNextRetryTick.erase(cardId);
        return texture;
    }

    std::string cardTypeLabel(const Card& card) {
        return card.getType() == CardType::Creature ? "CREATURE" : "SPELL";
    }

   void drawArtPanel(SDL_Renderer* renderer, const SDL_Rect& artRect, int cardId,
                  SDL_Color fallbackColor) {
    const int inset = Theme::Card::ART_INSET;
    const SDL_Rect clipRect{artRect.x + inset, artRect.y + inset,
                            artRect.w - inset * 2, artRect.h - inset * 2};

    const int cornerRadius = std::max(2, clipRect.w / 20);

    SDL_Texture* cardImage = getCardImageTexture(renderer, cardId);
    if (cardImage) {
        int imgW = 0;
        int imgH = 0;
        SDL_QueryTexture(cardImage, nullptr, nullptr, &imgW, &imgH);

        // "Cover" scaling — fill the panel entirely, cropping whichever axis overflows.
        SDL_Rect imgRect = clipRect;
        if (imgW > 0 && imgH > 0) {
            const float scaleX = static_cast<float>(clipRect.w) / static_cast<float>(imgW);
            const float scaleY = static_cast<float>(clipRect.h) / static_cast<float>(imgH);
            const float scale  = std::max(scaleX, scaleY);   // max = cover, min = contain
            const int scaledW  = static_cast<int>(imgW * scale);
            const int scaledH  = static_cast<int>(imgH * scale);
            imgRect = {
                clipRect.x + (clipRect.w - scaledW) / 2,
                clipRect.y + (clipRect.h - scaledH) / 2,
                scaledW,
                scaledH
            };
        }

        // Row-by-row rounded clip mask.
        for (int row = 0; row < clipRect.h; ++row) {
            const int y = clipRect.y + row;

            int indent = 0;
            if (row < cornerRadius) {
                const int dy = cornerRadius - row;
                indent = cornerRadius - static_cast<int>(
                    std::sqrt(static_cast<double>(cornerRadius * cornerRadius - dy * dy)));
            } else if (row >= clipRect.h - cornerRadius) {
                const int dy = row - (clipRect.h - cornerRadius - 1);
                indent = cornerRadius - static_cast<int>(
                    std::sqrt(static_cast<double>(cornerRadius * cornerRadius - dy * dy)));
            }

            const SDL_Rect rowClip{clipRect.x + indent, y,
                                   std::max(1, clipRect.w - indent * 2), 1};
            SDL_RenderSetClipRect(renderer, &rowClip);
            SDL_RenderCopy(renderer, cardImage, nullptr, &imgRect);
        }

        SDL_RenderSetClipRect(renderer, nullptr);
    } else {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        RenderUtil::fillRoundedRect(renderer, clipRect, cornerRadius, fallbackColor);
    }
    // Border draw removed.
}

    void drawCircularBadge(SDL_Renderer* renderer, RenderText& textRenderer, TTF_Font* font,
                           int cx, int cy, int radius, SDL_Color fill, SDL_Color border,
                           const std::string& value, float scale) {
        (void)scale;
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
        RenderUtil::fillCircle(renderer, cx, cy, radius);
        int borderThickness = 2;
        SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);

        RenderUtil::fillCircle(renderer, cx, cy, radius - borderThickness);

        int tw = 0;
        int th = 0;
        RenderText::measureText(font, value, tw, th);
        textRenderer.drawText(renderer, value, font, Theme::Card::BADGE_TEXT, cx - tw / 2, cy - th / 2);
    }

    enum class CardLayoutMode {
        Hand,
        Expanded,
        Board
    };

    void drawTemplateLayout(SDL_Renderer* renderer, RenderText& textRenderer,
                            const Card& card, const SDL_Rect& rect,
                            TTF_Font* titleFont, TTF_Font* bodyFont,
                            bool dimmed, int scrollOffset, CardLayoutMode mode) {
        const bool showTextBox = (mode == CardLayoutMode::Expanded);
        const bool showManaValue = (mode != CardLayoutMode::Board);
        const bool expandedMode = (mode == CardLayoutMode::Expanded);
        const bool compactText = (mode == CardLayoutMode::Hand);
        const bool boardMode = (mode == CardLayoutMode::Board);
        TTF_Font* uiFont = bodyFont ? bodyFont : titleFont;
        TTF_Font* compactFont = uiFont;
        if (titleFont && TTF_FontHeight(titleFont) < TTF_FontHeight(compactFont)) {
            compactFont = titleFont;
        }

        // Derive scale factor from the current renderer output size,
        // mirroring the approach used in Playing.cpp.
        int screenW = 0, screenH = 0;
        SDL_GetRendererOutputSize(renderer, &screenW, &screenH);
        const float scale = (screenW > 0 && screenH > 0)
            ? std::min(static_cast<float>(screenW) / 1200.0F,
                       static_cast<float>(screenH) / 850.0F)
            : 1.0F;

        // Helper: scale a theme constant and clamp to at least 1.
        auto sc = [scale](int v) {
            return std::max(1, static_cast<int>(v * scale));
        };

        const int cornerRadius = std::max(sc(Theme::Card::MIN_CORNER_RADIUS), rect.w / 20);
        const int borderThickness = (mode == CardLayoutMode::Expanded)
            ? sc(Theme::Card::EXPANDED_BORDER_THICKNESS)
            : sc(Theme::Card::BORDER_THICKNESS);

        SDL_Color borderColor = card.getType() == CardType::Creature
            ? Theme::Card::CREATURE_BORDER
            : Theme::Card::SPELL_BORDER;
        SDL_Color baseColor = card.getType() == CardType::Creature
            ? Theme::Card::CREATURE_BASE
            : Theme::Card::SPELL_BASE;

        if (dimmed) {
            borderColor = Theme::Card::DIMMED_BORDER;
            baseColor = Theme::Card::DIMMED_BASE;
        }

        RenderUtil::fillRoundedRect(renderer, rect, cornerRadius, baseColor);
        RenderUtil::drawRoundedBorder(renderer, rect, cornerRadius, Theme::Card::OUTER_BORDER, 1);
        RenderUtil::drawRoundedBorder(renderer, {rect.x + 1, rect.y + 1, rect.w - 2, rect.h - 2},
                                      std::max(2, cornerRadius - 1), borderColor, borderThickness);

        const int innerPad = borderThickness + sc(Theme::Card::INNER_PADDING);
        SDL_Rect inner{rect.x + innerPad, rect.y + innerPad, rect.w - (innerPad * 2), rect.h - (innerPad * 2)};

        int artH    = std::max(sc(Theme::Card::MIN_ART_HEIGHT),    inner.h * (showTextBox ? (expandedMode ? 33 : 40) : 52) / 100);
        int nameH   = std::max(sc(Theme::Card::MIN_NAME_HEIGHT),   inner.h * (expandedMode ? 13 : 10) / 100);
        int typeH   = std::max(sc(Theme::Card::MIN_TYPE_HEIGHT),   inner.h * 8 / 100);
        int bottomH = std::max(sc(Theme::Card::MIN_BOTTOM_HEIGHT), inner.h * (expandedMode ? 16 : 19) / 100);
        int textH   = showTextBox ? inner.h - artH - nameH - typeH - bottomH : 0;

        if (!showTextBox) {
            bottomH = std::max(sc(Theme::Card::MIN_COLLAPSED_BOTTOM_HEIGHT), inner.h - artH - nameH - typeH);
        }

        if (showTextBox && textH < sc(Theme::Card::MIN_TEXT_HEIGHT)) {
            const int needed = sc(Theme::Card::MIN_TEXT_HEIGHT) - textH;
            artH  = std::max(sc(Theme::Card::MIN_TEXT_HEIGHT), artH - needed);
            textH = inner.h - artH - nameH - typeH - bottomH;
        }

        if (expandedMode && showTextBox) {
            const int halvedNameH = std::max(1, nameH / 2);
            const int halvedTextH = std::max(1, textH);
            const int reclaimedHeight = (nameH - halvedNameH) + (textH - halvedTextH);
            nameH = halvedNameH;
            textH = halvedTextH;
            artH += reclaimedHeight;
        }

        const SDL_Rect artRect{inner.x, inner.y, inner.w, artH};
        const SDL_Rect nameRect{inner.x, artRect.y + artRect.h, inner.w, nameH};
        const SDL_Rect typeRect{inner.x, nameRect.y + nameRect.h, inner.w, typeH};
        const SDL_Rect textRect{inner.x, typeRect.y + typeRect.h, inner.w, std::max(0, textH)};
        const SDL_Rect bottomRect{
            inner.x,
            showTextBox ? textRect.y + textRect.h : typeRect.y + typeRect.h,
            inner.w,
            bottomH
        };

        const auto* creature = dynamic_cast<const CreatureCard*>(&card);
        const SDL_Color artFallback = dimmed
            ? Theme::Card::ART_DIMMED_FALLBACK
            : (card.getType() == CardType::Creature
                ? Theme::Card::ART_CREATURE_FALLBACK
                : Theme::Card::ART_SPELL_FALLBACK);
        drawArtPanel(renderer, artRect, card.getId(), artFallback);

        const int manaRadius = std::max(sc(Theme::Card::MIN_MANA_RADIUS), std::min(rect.w, rect.h) / 10);
        const int manaCx = rect.x + rect.w - manaRadius - 2;
        const int manaCy = rect.y + manaRadius + 2;
        drawCircularBadge(renderer, textRenderer, uiFont,
                          manaCx, manaCy, manaRadius,
                          card.getType() == CardType::Creature
                              ? Theme::Card::MANA_BADGE_CREATURE_FILL
                              : Theme::Card::MANA_BADGE_SPELL_FILL,
                          card.getType() == CardType::Creature
                              ? Theme::Card::MANA_BADGE_CREATURE_BORDER
                              : Theme::Card::MANA_BADGE_SPELL_BORDER,
                          std::to_string(card.getManaCost()), scale);

        SDL_SetRenderDrawColor(renderer, Theme::Card::NAME_PLATE_FILL.r, Theme::Card::NAME_PLATE_FILL.g, Theme::Card::NAME_PLATE_FILL.b, Theme::Card::NAME_PLATE_FILL.a);
        SDL_RenderFillRect(renderer, &nameRect);

        const int namePadX = std::max(sc(Theme::Card::MIN_NAME_PADDING), nameRect.w / 18);
        TTF_Font* nameFont = boardMode ? compactFont : uiFont;
        if (!expandedMode) {
            const std::string nameText = RenderText::truncateWithEllipsis(nameFont, card.getName(), nameRect.w - namePadX * 2);
            int nameW = 0;
            int nameTextH = 0;
            RenderText::measureText(nameFont, nameText, nameW, nameTextH);
            textRenderer.drawText(renderer, nameText, nameFont, Theme::Card::NAME_TEXT,
                                  nameRect.x + std::max(namePadX, (nameRect.w - nameW) / 2),
                                  nameRect.y + (nameRect.h - nameTextH) / 2);
        } else {
            SDL_Rect nameClip{nameRect.x + namePadX, nameRect.y + 2,
                              std::max(1, nameRect.w - (namePadX * 2)), std::max(1, nameRect.h - 4)};
            SDL_RenderSetClipRect(renderer, &nameClip);
            SDL_Surface* nameSurface = TTF_RenderUTF8_Blended_Wrapped(
                uiFont, card.getName().c_str(), Theme::Card::NAME_TEXT, static_cast<Uint32>(nameClip.w));
            if (nameSurface) {
                SDL_Texture* nameTexture = SDL_CreateTextureFromSurface(renderer, nameSurface);
                if (nameTexture) {
                    const int drawX = nameClip.x + std::max(0, (nameClip.w - nameSurface->w) / 2);
                    const int drawY = nameClip.y + std::max(0, (nameClip.h - nameSurface->h) / 2);
                    SDL_Rect nameDst{drawX, drawY, nameSurface->w, nameSurface->h};
                    SDL_RenderCopy(renderer, nameTexture, nullptr, &nameDst);
                    SDL_DestroyTexture(nameTexture);
                }
                SDL_FreeSurface(nameSurface);
            }
            SDL_RenderSetClipRect(renderer, nullptr);
        }

        const SDL_Color typeFill = card.getType() == CardType::Creature
            ? Theme::Card::TYPE_LINE_CREATURE_FILL
            : Theme::Card::TYPE_LINE_SPELL_FILL;
        SDL_SetRenderDrawColor(renderer, typeFill.r, typeFill.g, typeFill.b, typeFill.a);
        SDL_RenderFillRect(renderer, &typeRect);

        const int pillPadX = std::max(sc(Theme::Card::MIN_TYPE_PILL_PADDING), typeRect.w / 16);
        SDL_Rect typePill{typeRect.x + pillPadX, typeRect.y + 2, typeRect.w - (pillPadX * 2), std::max(2, typeRect.h - 4)};
        RenderUtil::fillRoundedRect(renderer, typePill, std::max(4, typePill.h / 3),
                                    card.getType() == CardType::Creature
                                        ? Theme::Card::TYPE_PILL_CREATURE_FILL
                                        : Theme::Card::TYPE_PILL_SPELL_FILL);
        const std::string typeText = StringUtil::toUpper(cardTypeLabel(card));
        TTF_Font* typeFont = compactFont;
        RenderUtil::drawCenteredText(renderer, typeFont, typeText, typePill,
                                     Theme::Card::TYPE_TEXT);

        if (showTextBox && textRect.h > 0 && !compactText) {
            SDL_SetRenderDrawColor(renderer, Theme::Card::TEXT_BOX_FILL.r, Theme::Card::TEXT_BOX_FILL.g, Theme::Card::TEXT_BOX_FILL.b, Theme::Card::TEXT_BOX_FILL.a);
            SDL_RenderFillRect(renderer, &textRect);
            SDL_SetRenderDrawColor(renderer, Theme::Card::TEXT_BOX_BORDER.r, Theme::Card::TEXT_BOX_BORDER.g, Theme::Card::TEXT_BOX_BORDER.b, Theme::Card::TEXT_BOX_BORDER.a);
            SDL_RenderDrawRect(renderer, &textRect);

            const int tcPadH = sc(Theme::Card::TEXT_CLIP_HORIZONTAL_PADDING);
            const int tcPadV = sc(Theme::Card::TEXT_CLIP_VERTICAL_PADDING);
            SDL_Rect textClip{
                textRect.x + tcPadH,
                textRect.y + tcPadV,
                std::max(1, textRect.w - tcPadH * 2),
                std::max(1, textRect.h - tcPadV * 2)};
            SDL_RenderSetClipRect(renderer, &textClip);

            std::vector<std::string> abilities;
            std::string flavor;
            StringUtil::splitAbilityTextAndFlavor(card.getText(), abilities, flavor);

            std::vector<std::string> textLines;
            if (compactText) {
                for (const std::string& line : abilities) {
                    if (textLines.size() >= 3) break;
                    if (!line.empty()) textLines.push_back(line);
                }
                if (textLines.empty()) {
                    const std::string fallback = StringUtil::trim(card.getText());
                    if (!fallback.empty()) textLines.push_back(fallback);
                }
            } else {
                for (const std::string& line : abilities) {
                    if (!line.empty()) textLines.push_back(line);
                }
                if (!flavor.empty()) {
                    textLines.push_back(flavor);
                }
            }

            std::vector<SDL_Surface*> surfaces;
            surfaces.reserve(textLines.size());
            int totalTextHeight = 0;
            const int lineGap = sc(Theme::Card::TEXT_LINE_GAP);
            const SDL_Color textBodyColor = card.hasGrantedEffects()
                ? Theme::Card::STAT_VALUE_BUFFED
                : Theme::Card::TEXT_BODY;
            TTF_SetFontWrappedAlign(uiFont, TTF_WRAPPED_ALIGN_CENTER);
            for (const std::string& line : textLines) {
                SDL_Surface* s = TTF_RenderUTF8_Blended_Wrapped(
                    uiFont, line.c_str(), textBodyColor, static_cast<Uint32>(textClip.w));
                if (!s) continue;
                surfaces.push_back(s);
                totalTextHeight += s->h;
            }
            if (!surfaces.empty()) {
                totalTextHeight += static_cast<int>(surfaces.size() - 1) * lineGap;
            }

            int cursorY = textClip.y + std::max(0, (textClip.h - totalTextHeight) / 2) - std::max(0, scrollOffset);

            for (SDL_Surface* surface : surfaces) {
                if (!surface) continue;
                SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
                if (texture) {
                    const int drawX = textClip.x + std::max(0, (textClip.w - surface->w) / 2);
                    SDL_Rect dst{drawX, cursorY, surface->w, surface->h};
                    SDL_RenderCopy(renderer, texture, nullptr, &dst);
                    SDL_DestroyTexture(texture);
                }
                cursorY += surface->h + lineGap;
                SDL_FreeSurface(surface);
            }

            SDL_RenderSetClipRect(renderer, nullptr);
        }

        // SDL_SetRenderDrawColor(renderer, Theme::Card::BOTTOM_BAR_FILL.r, Theme::Card::BOTTOM_BAR_FILL.g, Theme::Card::BOTTOM_BAR_FILL.b, Theme::Card::BOTTOM_BAR_FILL.a);
        // SDL_RenderFillRect(renderer, &bottomRect);
        const int bottomCornerRadius = std::max(2, bottomRect.w / 20);

        for (int row = 0; row < bottomRect.h; ++row) {
            const int y = bottomRect.y + row;
            int indent = 0;
            if (row >= bottomRect.h - bottomCornerRadius) {  // only round the bottom corners
                const int dy = row - (bottomRect.h - bottomCornerRadius - 1);
                indent = bottomCornerRadius - static_cast<int>(
                    std::sqrt(static_cast<double>(bottomCornerRadius * bottomCornerRadius - dy * dy)));
            }

            const SDL_Rect rowClip{bottomRect.x + indent, y,
                                std::max(1, bottomRect.w - indent * 2), 1};
            SDL_RenderSetClipRect(renderer, &rowClip);
            SDL_SetRenderDrawColor(renderer, Theme::Card::BOTTOM_BAR_FILL.r,
                                            Theme::Card::BOTTOM_BAR_FILL.g,
                                            Theme::Card::BOTTOM_BAR_FILL.b,
                                            Theme::Card::BOTTOM_BAR_FILL.a);
            SDL_RenderFillRect(renderer, &bottomRect);
        }

        SDL_RenderSetClipRect(renderer, nullptr);

        const int valueRadius = showManaValue
            ? std::max(sc(11), std::min(bottomRect.h / 2 - 3, rect.w / 10))
            : 0;
        const int valueCx = bottomRect.x + bottomRect.w / 2;
        const int leftBlockX  = bottomRect.x + sc(Theme::Card::BOTTOM_SECTION_PADDING);
        const int leftBlockW  = std::max(1, bottomRect.w / 2 - sc(Theme::Card::BOTTOM_SECTION_PADDING) - sc(Theme::Card::BOTTOM_COMPACT_SECTION_GAP));
        const int rightBlockX = bottomRect.x + bottomRect.w / 2 + sc(Theme::Card::BOTTOM_COMPACT_SECTION_GAP);
        const int rightBlockW = std::max(1, (bottomRect.x + bottomRect.w - sc(Theme::Card::BOTTOM_SECTION_PADDING)) - rightBlockX);

        const int statLabelY = bottomRect.y + std::max(sc(Theme::Card::MIN_STAT_LABEL_TOP_PADDING), bottomRect.h / 8);
        const int statValueY = std::min(
            bottomRect.y + bottomRect.h - std::max(sc(Theme::Card::MIN_STAT_BASELINE_OFFSET), bottomRect.h / 2),
            bottomRect.y + bottomRect.h - TTF_FontHeight(uiFont) - 2);
        if (creature) {
            TTF_Font* statLabelFont = compactFont;
            TTF_Font* statValueFont = uiFont;
            int atkLabelW = 0;
            int atkLabelH = 0;
            int defLabelW = 0;
            int defLabelH = 0;
            int atkValueW = 0;
            int atkValueH = 0;
            int defValueW = 0;
            int defValueH = 0;
            const std::string atkValue = std::to_string(creature->getPower());
            const std::string defValue = std::to_string(creature->getToughness());

            RenderText::measureText(statLabelFont, "ATK", atkLabelW, atkLabelH);
            RenderText::measureText(statLabelFont, "DEF", defLabelW, defLabelH);
            RenderText::measureText(statValueFont, atkValue, atkValueW, atkValueH);
            RenderText::measureText(statValueFont, defValue, defValueW, defValueH);

            SDL_Rect leftClip{leftBlockX, bottomRect.y, leftBlockW, bottomRect.h};
            SDL_Rect rightClip{rightBlockX, bottomRect.y, rightBlockW, bottomRect.h};

            SDL_RenderSetClipRect(renderer, &leftClip);

            textRenderer.drawText(renderer, "ATK", statLabelFont, Theme::Card::STAT_LABEL, leftBlockX, statLabelY);

            // get the colour for attack value
            SDL_Color atkColor = Theme::Card::STAT_VALUE;
            if (creature->getPower() > creature->getBasePower()) {
                atkColor = Theme::Card::STAT_VALUE_BUFFED;
            } else if (creature->getPower() < creature->getBasePower()) {
                atkColor = Theme::Card::STAT_VALUE_DEBUFFED;
            }

            const int atkValueX = leftBlockX + atkLabelW / 2 - atkValueW / 2;

            // DEF value x — centered under "DEF" label
            const int defLabelX = rightBlockX + std::max(0, rightBlockW - defLabelW);
            const int defValueX = defLabelX + defLabelW / 2 - defValueW / 2;

            textRenderer.drawText(renderer, atkValue, statValueFont, atkColor,
                atkValueX, std::max(statValueY, bottomRect.y + 1 + atkLabelH));
            SDL_RenderSetClipRect(renderer, nullptr);

            SDL_RenderSetClipRect(renderer, &rightClip);

            textRenderer.drawText(renderer, "DEF", statLabelFont, Theme::Card::STAT_LABEL, rightBlockX + std::max(0, rightBlockW - defLabelW), statLabelY);

            // get the colour for defense value
            SDL_Color defColor = Theme::Card::STAT_VALUE;
            if (creature->getToughness() > creature->getBaseToughness()) {
                defColor = Theme::Card::STAT_VALUE_BUFFED;
            } else if (creature->getToughness() < creature->getBaseToughness()) {
                defColor = Theme::Card::STAT_VALUE_DEBUFFED;
            }

            textRenderer.drawText(renderer, defValue, statValueFont, defColor,
                defValueX, std::max(statValueY, bottomRect.y + 1 + defLabelH));

            SDL_RenderSetClipRect(renderer, nullptr);
        }

        if (showManaValue) {
            const int valueCy = bottomRect.y + bottomRect.h / 2;
            drawCircularBadge(renderer, textRenderer, uiFont,
                              valueCx, valueCy, valueRadius,
                              card.getType() == CardType::Creature
                                  ? Theme::Card::VALUE_BADGE_CREATURE_FILL
                                  : Theme::Card::VALUE_BADGE_SPELL_FILL,
                              card.getType() == CardType::Creature
                                  ? Theme::Card::VALUE_BADGE_CREATURE_BORDER
                                  : Theme::Card::VALUE_BADGE_SPELL_BORDER,
                              std::to_string(std::max(0, card.getManaValue())), scale);
        }
    }

    void drawHandLayout(SDL_Renderer* renderer, RenderText& textRenderer,
                        const Card& card, const SDL_Rect& rect,
                        TTF_Font* titleFont, TTF_Font* bodyFont, bool dimmed) {
        drawTemplateLayout(renderer, textRenderer, card, rect, titleFont, bodyFont,
                           dimmed, 0, CardLayoutMode::Hand);
    }

    void drawExpandedLayout(SDL_Renderer* renderer, RenderText& textRenderer,
                            const Card& card, const SDL_Rect& rect,
                            TTF_Font* titleFont, TTF_Font* bodyFont,
                            bool dimmed, int scrollOffset) {
        drawTemplateLayout(renderer, textRenderer, card, rect, titleFont, bodyFont,
                           dimmed, scrollOffset, CardLayoutMode::Expanded);
    }

    void drawBoardLayout(SDL_Renderer* renderer, RenderText& textRenderer,
                         const Card& card, const SDL_Rect& rect,
                         TTF_Font* titleFont, TTF_Font* bodyFont, bool dimmed) {
        drawTemplateLayout(renderer, textRenderer, card, rect, titleFont, bodyFont,
                           dimmed, 0, CardLayoutMode::Board);
    }

} // namespace

// ── public API ───────────────────────────────────────────────────────────────

void RenderCard::drawCardFace(SDL_Renderer* renderer, RenderText& textRenderer, const Card& card,
                              const SDL_Rect& rect, TTF_Font* titleFont, TTF_Font* bodyFont,
                              bool dimmed, bool compact, int scrollOffset) {
    if (!renderer || !titleFont || !bodyFont) return;
    if (compact)
        drawHandLayout(renderer, textRenderer, card, rect, titleFont, bodyFont, dimmed);
    else
        drawExpandedLayout(renderer, textRenderer, card, rect, titleFont, bodyFont, dimmed, scrollOffset);
}

void RenderCard::drawHandCard(SDL_Renderer* renderer, RenderText& textRenderer, const Card& card,
                              const SDL_Rect& cardRect, TTF_Font* titleFont, TTF_Font* bodyFont) {
    if (!renderer || !titleFont || !bodyFont) return;
    drawCardFace(renderer, textRenderer, card, cardRect, titleFont, bodyFont, false, true);
}

void RenderCard::drawBoardCard(SDL_Renderer* renderer, RenderText& textRenderer, const Card& card,
                               const SDL_Rect& cardRect, TTF_Font* titleFont, TTF_Font* bodyFont) {
    if (!renderer || !titleFont || !bodyFont) return;
    drawBoardLayout(renderer, textRenderer, card, cardRect, titleFont, bodyFont, false);
}

void RenderCard::drawPreview(SDL_Renderer* renderer, RenderText& textRenderer, const Card& card,
                             const SDL_Rect& previewRect, TTF_Font* bodyFont, TTF_Font* titleFont, int scrollOffset) {
    if (!renderer || !bodyFont || !titleFont) return;
    drawCardFace(renderer, textRenderer, card, previewRect, titleFont, bodyFont, false, false, scrollOffset);
}

void RenderCard::drawCardBack(SDL_Renderer* renderer, const SDL_Rect& cardRect) {
    if (!renderer) return;
    const int r = std::max(Theme::Card::CARD_BACK_MIN_RADIUS, cardRect.w / 7);
    RenderUtil::fillRoundedRect(renderer, cardRect, r, Theme::Card::CARD_BACK_OUTER_FILL);
    RenderUtil::drawRoundedBorder(renderer, cardRect, r, Theme::Card::CARD_BACK_OUTER_BORDER, 1);
    RenderUtil::drawRoundedBorder(renderer, {cardRect.x+1, cardRect.y+1, cardRect.w-2, cardRect.h-2},
                      r-1, Theme::Card::CARD_BACK_INNER_BORDER, 2);
    SDL_Rect inset{cardRect.x + Theme::Card::CARD_BACK_INSET, cardRect.y + Theme::Card::CARD_BACK_INSET,
                   cardRect.w - Theme::Card::CARD_BACK_INSET * 2, cardRect.h - Theme::Card::CARD_BACK_INSET * 2};
    if (inset.w > 4 && inset.h > 4) {
        RenderUtil::fillRoundedRect(renderer, inset, std::max(Theme::Card::CARD_BACK_MIN_INSET_RADIUS, r - 5), Theme::Card::CARD_BACK_INSET_FILL);
        RenderUtil::drawRoundedBorder(renderer, inset, std::max(Theme::Card::CARD_BACK_MIN_INSET_RADIUS, r - 5), Theme::Card::CARD_BACK_INSET_BORDER, 1);
    }
}

bool RenderCard::preloadCardArt(SDL_Renderer* renderer, int cardId) {
    return getCardImageTexture(renderer, cardId, false) != nullptr;
}

bool RenderCard::isCardArtCached(int cardId) {
    return gCardImageCache.find(cardId) != gCardImageCache.end();
}

void RenderCard::clearImageCache() {
    for (auto& pair : gCardImageCache) {
        if (pair.second) {
            SDL_DestroyTexture(pair.second);
        }
    }
    gCardImageCache.clear();
    gCardImageFailureCount.clear();
    gCardImageNextRetryTick.clear();
    gNextCardImageFetchTick = 0;
}
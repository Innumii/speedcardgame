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
    constexpr SDL_Color kCardTextColor{0, 0, 0, 255};
    std::unordered_map<int, SDL_Texture*> gCardImageCache;
    std::unordered_set<int> gMissingCardImages;

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

        res = client.Get(path.c_str());

    } else {
        httplib::Client client(host.c_str(), port);
        client.set_follow_location(true);

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

    SDL_Texture* getCardImageTexture(SDL_Renderer* renderer, int cardId) {
        if (!renderer || cardId <= 0) return nullptr;

        const auto cacheIt = gCardImageCache.find(cardId);
        if (cacheIt != gCardImageCache.end()) {
            return cacheIt->second;
        }

        if (gMissingCardImages.find(cardId) != gMissingCardImages.end()) {
            return nullptr;
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
            gMissingCardImages.insert(cardId);
            return nullptr;
        }

        SDL_RWops* rw = SDL_RWFromConstMem(imageBytes.data(), static_cast<int>(imageBytes.size()));
        if (!rw) {
            gMissingCardImages.insert(cardId);
            return nullptr;
        }

        SDL_Surface* surface = IMG_Load_RW(rw, 1);
        if (!surface) {
            gMissingCardImages.insert(cardId);
            return nullptr;
        }

        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_FreeSurface(surface);
        if (!texture) {
            gMissingCardImages.insert(cardId);
            return nullptr;
        }

        gCardImageCache[cardId] = texture;
        return texture;
    }

    std::string cardTypeLabel(const Card& card) {
        return card.getType() == CardType::Creature ? "CREATURE" : "SPELL";
    }

    std::string cardSubtypeLabel(const Card& card) {
        return cardTypeLabel(card);
    }

    std::string rarityLabel(Rarity rarity) {
        switch (rarity) {
            case Rarity::Common: return "C";
            case Rarity::Uncommon: return "U";
            case Rarity::Rare: return "R *";
            case Rarity::VeryRare: return "VR *";
            case Rarity::SuperRare: return "SR #";
            default: return "C";
        }
    }

    SDL_Color rarityColor(Rarity rarity) {
        switch (rarity) {
            case Rarity::Rare: return SDL_Color{200, 200, 210, 255};
            case Rarity::VeryRare: return SDL_Color{235, 195, 90, 255};
            case Rarity::SuperRare: return SDL_Color{210, 120, 245, 255};
            case Rarity::Uncommon: return SDL_Color{155, 220, 155, 255};
            case Rarity::Common:
            default: return SDL_Color{220, 220, 220, 255};
        }
    }

    std::vector<std::string> splitLines(const std::string& text) {
        std::vector<std::string> lines;
        std::istringstream stream(text);
        std::string line;
        while (std::getline(stream, line)) {
            const std::string cleaned = StringUtil::trim(line);
            if (!cleaned.empty()) lines.push_back(cleaned);
        }
        return lines;
    }

    void splitAbilityAndFlavor(const std::string& text,
                               std::vector<std::string>& abilities,
                               std::string& flavor) {
        abilities.clear();
        flavor.clear();

        const std::string marker = "Flavor:";
        const auto markerPos = text.find(marker);
        if (markerPos != std::string::npos) {
            const std::string left = StringUtil::trim(text.substr(0, markerPos));
            abilities = splitLines(left);
            flavor = StringUtil::trim(text.substr(markerPos + marker.size()));
            return;
        }

        const std::string paragraphBreak = "\n\n";
        const auto breakPos = text.find(paragraphBreak);
        if (breakPos != std::string::npos) {
            const std::string left = StringUtil::trim(text.substr(0, breakPos));
            abilities = splitLines(left);
            flavor = StringUtil::trim(text.substr(breakPos + paragraphBreak.size()));
            return;
        }

        abilities = splitLines(text);
    }

    void drawArtPanel(SDL_Renderer* renderer, const SDL_Rect& artRect, int cardId,
                      SDL_Color fallbackColor) {
        SDL_Texture* cardImage = getCardImageTexture(renderer, cardId);
        if (cardImage) {
            SDL_Rect clipRect{artRect.x + 2, artRect.y + 2, artRect.w - 4, artRect.h - 4};
            SDL_RenderSetClipRect(renderer, &clipRect);

            int imgW = 0;
            int imgH = 0;
            SDL_QueryTexture(cardImage, nullptr, nullptr, &imgW, &imgH);

            if (imgW > 0 && imgH > 0) {
                const float scale = static_cast<float>(clipRect.w) / static_cast<float>(imgW);
                const int scaledH = static_cast<int>(imgH * scale);
                const int renderY = clipRect.y + std::max(0, (clipRect.h - scaledH) / 2);
                SDL_Rect imgRect{clipRect.x, renderY, clipRect.w, scaledH};
                SDL_RenderCopy(renderer, cardImage, nullptr, &imgRect);
            }

            SDL_RenderSetClipRect(renderer, nullptr);
        } else {
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, fallbackColor.r, fallbackColor.g, fallbackColor.b, fallbackColor.a);
            SDL_RenderFillRect(renderer, &artRect);
        }

        SDL_SetRenderDrawColor(renderer, 20, 20, 20, 220);
        SDL_RenderDrawRect(renderer, &artRect);
    }

    void drawCircularBadge(SDL_Renderer* renderer, RenderText& textRenderer, TTF_Font* font,
                           int cx, int cy, int radius, SDL_Color fill, SDL_Color border,
                           const std::string& value) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);
        RenderUtil::fillCircle(renderer, cx, cy, radius);

        SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
        for (int i = 0; i < 2; ++i) {
            int rr = std::max(1, radius - i);
            for (int dy = -rr; dy <= rr; ++dy) {
                const int dx = static_cast<int>(std::sqrt(static_cast<double>(rr * rr - dy * dy)));
                SDL_RenderDrawPoint(renderer, cx - dx, cy + dy);
                SDL_RenderDrawPoint(renderer, cx + dx, cy + dy);
            }
        }

        int tw = 0;
        int th = 0;
        RenderText::measureText(font, value, tw, th);
        textRenderer.drawText(renderer, value, font, SDL_Color{255, 255, 255, 255}, cx - tw / 2, cy - th / 2);
    }

    void drawDuelMastersLayout(SDL_Renderer* renderer, RenderText& textRenderer,
                               const Card& card, const SDL_Rect& rect,
                               TTF_Font* titleFont, TTF_Font* bodyFont,
                               bool dimmed, int scrollOffset, bool compact) {
        const int cornerRadius = std::max(compact ? 8 : 10, rect.w / 8);
        const int borderThick = compact ? 3 : 4;

        SDL_Color frameColor = card.getType() == CardType::Creature
            ? SDL_Color{118, 85, 150, 255}
            : SDL_Color{75, 105, 150, 255};
        if (dimmed) {
            frameColor = SDL_Color{90, 90, 90, 255};
        }

        RenderUtil::fillRoundedRect(renderer, rect, cornerRadius, SDL_Color{32, 32, 42, 255});
        RenderUtil::drawRoundedBorder(renderer, rect, cornerRadius, SDL_Color{0, 0, 0, 220}, 1);
        RenderUtil::drawRoundedBorder(renderer, {rect.x + 1, rect.y + 1, rect.w - 2, rect.h - 2},
                                      cornerRadius - 1, frameColor, borderThick);

        const int innerPad = borderThick + 3;
        SDL_Rect inner{rect.x + innerPad, rect.y + innerPad, rect.w - innerPad * 2, rect.h - innerPad * 2};

        int topBarH = std::max(26, inner.h * 18 / 100);
        int artBoxH = std::max(40, inner.h * 34 / 100);
        int typeLineH = std::max(18, inner.h * 8 / 100);
        int bottomBarH = std::max(24, inner.h * 13 / 100);
        int textBoxH = inner.h - topBarH - artBoxH - typeLineH - bottomBarH;

        if (textBoxH < 24) {
            const int deficit = 24 - textBoxH;
            artBoxH = std::max(24, artBoxH - deficit);
            textBoxH = inner.h - topBarH - artBoxH - typeLineH - bottomBarH;
        }

        const SDL_Rect topBar{inner.x, inner.y, inner.w, topBarH};
        const SDL_Rect artBox{inner.x, topBar.y + topBar.h, inner.w, artBoxH};
        const SDL_Rect typeLine{inner.x, artBox.y + artBox.h, inner.w, typeLineH};
        const SDL_Rect textBox{inner.x, typeLine.y + typeLine.h, inner.w, textBoxH};
        const SDL_Rect bottomBar{inner.x, textBox.y + textBox.h, inner.w, bottomBarH};

        SDL_SetRenderDrawColor(renderer, 46, 40, 58, 255);
        SDL_RenderFillRect(renderer, &topBar);

        const int topPad = std::max(4, topBarH / 10);
        const int badgeSize = std::max(16, topBarH - topPad * 2);
        SDL_Rect manaBadge{topBar.x + topPad, topBar.y + topPad, badgeSize, badgeSize};
        RenderUtil::fillRoundedRect(renderer, manaBadge, std::max(3, badgeSize / 5), SDL_Color{45, 80, 150, 255});
        RenderUtil::drawRoundedBorder(renderer, manaBadge, std::max(3, badgeSize / 5), SDL_Color{190, 220, 255, 255}, 1);

        const std::string manaText = std::to_string(card.getManaCost());
        int mw = 0;
        int mh = 0;
        RenderText::measureText(titleFont, manaText, mw, mh);
        textRenderer.drawText(renderer, manaText, titleFont, SDL_Color{255, 255, 255, 255},
                              manaBadge.x + (manaBadge.w - mw) / 2, manaBadge.y + (manaBadge.h - mh) / 2);

        const int nameX = manaBadge.x + manaBadge.w + topPad;
        const int nameMaxW = topBar.x + topBar.w - nameX - topPad;
        const std::string cardName = RenderText::truncateWithEllipsis(titleFont, card.getName(), nameMaxW);
        int nameW = 0;
        int nameH = 0;
        SDL_Rect topTextClip{nameX, topBar.y + topPad - 1, std::max(1, nameMaxW), std::max(1, topBar.h - topPad)};
        SDL_RenderSetClipRect(renderer, &topTextClip);

        RenderText::measureText(titleFont, cardName, nameW, nameH);
        textRenderer.drawText(renderer, cardName, titleFont, SDL_Color{255, 245, 210, 255},
                              nameX, topBar.y + topPad - 1);

        const std::string subtype = StringUtil::toUpper(cardSubtypeLabel(card));
        const std::string subtypeText = RenderText::truncateWithEllipsis(bodyFont, subtype, nameMaxW);
        int subtypeH = 0;
        int subtypeW = 0;
        RenderText::measureText(bodyFont, subtypeText, subtypeW, subtypeH);
        const int subtypeY = topBar.y + topPad + nameH - 1;
        if (!subtypeText.empty() && subtypeY + subtypeH <= topBar.y + topBar.h - 1) {
            textRenderer.drawText(renderer, subtypeText, bodyFont, SDL_Color{220, 220, 220, 255},
                                  nameX, subtypeY);
        }
        SDL_RenderSetClipRect(renderer, nullptr);

        const auto* creature = dynamic_cast<const CreatureCard*>(&card);
        const SDL_Color artFallback = dimmed
            ? SDL_Color{70, 70, 80, 255}
            : (creature ? SDL_Color{92, 74, 122, 255} : SDL_Color{70, 92, 124, 255});
        drawArtPanel(renderer, artBox, card.getId(), artFallback);

        SDL_SetRenderDrawColor(renderer, 74, 58, 102, 255);
        SDL_RenderFillRect(renderer, &typeLine);
        const std::string typeText = StringUtil::toUpper(cardTypeLabel(card));
        RenderUtil::drawCenteredText(renderer, bodyFont, typeText, typeLine, SDL_Color{250, 238, 205, 255});

        SDL_SetRenderDrawColor(renderer, 228, 220, 202, 255);
        SDL_RenderFillRect(renderer, &textBox);

        SDL_Rect textClip{textBox.x + 5, textBox.y + 4, textBox.w - 10, std::max(1, textBox.h - 8)};
        std::vector<std::string> abilities;
        std::string flavor;
        splitAbilityAndFlavor(card.getText(), abilities, flavor);
        TTF_Font* effectFont = compact ? titleFont : bodyFont;

        SDL_RenderSetClipRect(renderer, &textClip);
        int cursorY = textClip.y - std::max(0, scrollOffset);
        const std::size_t maxAbilityLines = compact ? 3U : abilities.size();
        std::size_t renderedAbilityLines = 0;

        for (const std::string& ability : abilities) {
            if (renderedAbilityLines >= maxAbilityLines) {
                break;
            }

            const std::string bullet = "• " + ability;

            if (compact) {
                const std::string compactLine = RenderText::truncateWithEllipsis(effectFont, bullet, textClip.w);
                int cw = 0;
                int ch = 0;
                RenderText::measureText(effectFont, compactLine, cw, ch);
                if (cursorY + ch > textClip.y + textClip.h) {
                    break;
                }
                textRenderer.drawText(renderer, compactLine, effectFont, kCardTextColor, textClip.x, cursorY);
                cursorY += std::max(ch, TTF_FontLineSkip(effectFont));
                renderedAbilityLines++;
                continue;
            }

            SDL_Surface* abilitySurface = TTF_RenderUTF8_Blended_Wrapped(
                effectFont, bullet.c_str(), kCardTextColor, static_cast<Uint32>(textClip.w));
            if (!abilitySurface) continue;

            SDL_Texture* abilityTexture = SDL_CreateTextureFromSurface(renderer, abilitySurface);
            if (abilityTexture) {
                SDL_Rect dst{textClip.x, cursorY, abilitySurface->w, abilitySurface->h};
                SDL_RenderCopy(renderer, abilityTexture, nullptr, &dst);
                SDL_DestroyTexture(abilityTexture);
            }
            cursorY += abilitySurface->h + 2;
            SDL_FreeSurface(abilitySurface);
            renderedAbilityLines++;
        }

        if (!flavor.empty()) {
            const int previousStyle = TTF_GetFontStyle(effectFont);
            TTF_SetFontStyle(effectFont, previousStyle | TTF_STYLE_ITALIC);

            if (compact) {
                const std::string compactFlavor = RenderText::truncateWithEllipsis(effectFont, flavor, textClip.w);
                int fw = 0;
                int fh = 0;
                RenderText::measureText(effectFont, compactFlavor, fw, fh);
                if (!compactFlavor.empty() && cursorY + fh <= textClip.y + textClip.h) {
                    textRenderer.drawText(renderer, compactFlavor, effectFont, SDL_Color{82, 76, 72, 255},
                                          textClip.x, cursorY + 1);
                }
                TTF_SetFontStyle(effectFont, previousStyle);
                SDL_RenderSetClipRect(renderer, nullptr);
            } else {

                SDL_Surface* flavorSurface = TTF_RenderUTF8_Blended_Wrapped(
                    effectFont, flavor.c_str(), SDL_Color{82, 76, 72, 255}, static_cast<Uint32>(textClip.w));
                if (flavorSurface) {
                    SDL_Texture* flavorTexture = SDL_CreateTextureFromSurface(renderer, flavorSurface);
                    if (flavorTexture) {
                        SDL_Rect dst{textClip.x, cursorY + 2, flavorSurface->w, flavorSurface->h};
                        SDL_RenderCopy(renderer, flavorTexture, nullptr, &dst);
                        SDL_DestroyTexture(flavorTexture);
                    }
                    SDL_FreeSurface(flavorSurface);
                }

                TTF_SetFontStyle(effectFont, previousStyle);
                SDL_RenderSetClipRect(renderer, nullptr);
            }
        } else {
            SDL_RenderSetClipRect(renderer, nullptr);
        }

        SDL_SetRenderDrawColor(renderer, 46, 40, 58, 255);
        SDL_RenderFillRect(renderer, &bottomBar);

        const int civRadius = compact
            ? std::max(7, std::min(bottomBar.h / 2 - 4, rect.w / 12))
            : std::max(8, std::min(bottomBar.h / 2 - 3, rect.w / 10));
        const int civCx = bottomBar.x + bottomBar.w / 2;
        const int civCy = bottomBar.y + bottomBar.h / 2;

        const int leftBlockX = bottomBar.x + 4;
        const int leftBlockW = std::max(1, civCx - civRadius - 10 - leftBlockX);
        const int rightBlockX = civCx + civRadius + 8;
        const int rightBlockW = std::max(1, bottomBar.x + bottomBar.w - 4 - rightBlockX);

        if (creature && leftBlockW > 2) {
            const std::string statsText = std::to_string(creature->getToughness()) + "/" +
                                          std::to_string(creature->getPower());
            TTF_Font* statsFont = compact ? titleFont : bodyFont;
            int pw = 0;
            int ph = 0;
            RenderText::measureText(statsFont, statsText, pw, ph);
            SDL_Rect leftClip{leftBlockX, bottomBar.y + 1, leftBlockW, std::max(1, bottomBar.h - 2)};
            SDL_RenderSetClipRect(renderer, &leftClip);
            textRenderer.drawText(renderer, statsText, statsFont, SDL_Color{255, 245, 210, 255},
                                  leftBlockX, bottomBar.y + (bottomBar.h - ph) / 2);
            SDL_RenderSetClipRect(renderer, nullptr);
        }

        drawCircularBadge(renderer, textRenderer, bodyFont,
                          civCx, civCy, civRadius,
                          SDL_Color{58, 96, 170, 255}, SDL_Color{210, 230, 255, 255},
                          std::to_string(std::max(0, card.getManaValue())));

        const std::string rarityText = rarityLabel(card.getRarity());
        const std::string drawRarity = RenderText::truncateWithEllipsis(bodyFont, rarityText, rightBlockW);
        int rw = 0;
        int rh = 0;
        RenderText::measureText(bodyFont, drawRarity, rw, rh);
        if (!drawRarity.empty() && rightBlockW > 2) {
            SDL_Rect rightClip{rightBlockX, bottomBar.y + 1, rightBlockW, std::max(1, bottomBar.h - 2)};
            SDL_RenderSetClipRect(renderer, &rightClip);
            textRenderer.drawText(renderer, drawRarity, bodyFont, rarityColor(card.getRarity()),
                                  rightBlockX + std::max(0, rightBlockW - rw),
                                  bottomBar.y + (bottomBar.h - rh) / 2);
            SDL_RenderSetClipRect(renderer, nullptr);
        }
    }

    // ── compact card (hand/board) ──────────────────────────────────────
    void drawCompact(SDL_Renderer* renderer, RenderText& textRenderer,
                     const Card& card, const SDL_Rect& rect,
                     TTF_Font* titleFont, TTF_Font* bodyFont, bool dimmed) {
        // Duel Masters layout refactor
        drawDuelMastersLayout(renderer, textRenderer, card, rect, titleFont, bodyFont, dimmed, 0, true);
    }

    // ── full preview card ──────────────────────────────────────────────
    void drawFull(SDL_Renderer* renderer, RenderText& textRenderer,
                  const Card& card, const SDL_Rect& rect,
                  TTF_Font* titleFont, TTF_Font* bodyFont, bool dimmed, int scrollOffset) {
        // Duel Masters layout refactor
        drawDuelMastersLayout(renderer, textRenderer, card, rect, titleFont, bodyFont, dimmed, scrollOffset, false);
    }

} // namespace

// ── public API ───────────────────────────────────────────────────────────────

void RenderCard::drawCardFace(SDL_Renderer* renderer, RenderText& textRenderer, const Card& card,
                              const SDL_Rect& rect, TTF_Font* titleFont, TTF_Font* bodyFont,
                              bool dimmed, bool compact, int scrollOffset) {
    if (!renderer || !titleFont || !bodyFont) return;
    if (compact)
        drawCompact(renderer, textRenderer, card, rect, titleFont, bodyFont, dimmed);
    else
        drawFull(renderer, textRenderer, card, rect, titleFont, bodyFont, dimmed, scrollOffset);
}

void RenderCard::drawHandCard(SDL_Renderer* renderer, RenderText& textRenderer, const Card& card,
                              const SDL_Rect& cardRect, TTF_Font* titleFont, TTF_Font* bodyFont) {
    if (!renderer || !titleFont || !bodyFont) return;
    drawCardFace(renderer, textRenderer, card, cardRect, titleFont, bodyFont, false, true);
}

void RenderCard::drawBoardCard(SDL_Renderer* renderer, RenderText& textRenderer, const Card& card,
                               const SDL_Rect& cardRect, TTF_Font* titleFont, TTF_Font* bodyFont) {
    if (!renderer || !titleFont || !bodyFont) return;
    drawCardFace(renderer, textRenderer, card, cardRect, titleFont, bodyFont, false, true);
}

void RenderCard::drawPreview(SDL_Renderer* renderer, RenderText& textRenderer, const Card& card,
                             const SDL_Rect& previewRect, TTF_Font* bodyFont, TTF_Font* titleFont, int scrollOffset) {
    if (!renderer || !bodyFont || !titleFont) return;
    drawCardFace(renderer, textRenderer, card, previewRect, titleFont, bodyFont, false, false, scrollOffset);
}

void RenderCard::drawCardBack(SDL_Renderer* renderer, const SDL_Rect& cardRect) {
    if (!renderer) return;
    const int r = std::max(6, cardRect.w / 7);
    RenderUtil::fillRoundedRect(renderer, cardRect, r, SDL_Color{65, 48, 95, 255});
    RenderUtil::drawRoundedBorder(renderer, cardRect, r, SDL_Color{35, 25, 55, 255}, 1);
    RenderUtil::drawRoundedBorder(renderer, {cardRect.x+1, cardRect.y+1, cardRect.w-2, cardRect.h-2},
                      r-1, SDL_Color{115, 85, 155, 255}, 2);
    SDL_Rect inset{cardRect.x+8, cardRect.y+8, cardRect.w-16, cardRect.h-16};
    if (inset.w > 4 && inset.h > 4) {
        RenderUtil::fillRoundedRect(renderer, inset, std::max(3, r-5), SDL_Color{85, 60, 120, 255});
        RenderUtil::drawRoundedBorder(renderer, inset, std::max(3, r-5), SDL_Color{125, 95, 165, 255}, 1);
    }
}

void RenderCard::clearImageCache() {
    for (auto& pair : gCardImageCache) {
        if (pair.second) {
            SDL_DestroyTexture(pair.second);
        }
    }
    gCardImageCache.clear();
    gMissingCardImages.clear();
}
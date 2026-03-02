#include "render/RenderCard.hpp"

#include "objects/Card.h"
#include "core/NetworkClient.hpp"
#include "objects/CreatureCard.h"
#include "render/RenderText.hpp"
#include "utils/EnvUtil.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <algorithm>
#include <cstdlib>
#include <array>
#include <sstream>
#include <string>
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

    std::string path = "/cardbase/images/" + std::to_string(cardId) + "." + extension;

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

        const std::string host = EnvUtil::getServiceHost("CARDS_SERVICE", "127.0.0.1", "cards.speedcardgame.aws");
        const int port = EnvUtil::getServicePort("CARDS_SERVICE", 8082, 443);
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

    bool measureText(TTF_Font* font, const std::string& text, int& width, int& height) {
        if (!font) return false;
        if (TTF_SizeUTF8(font, text.c_str(), &width, &height) != 0) {
            width = 0;
            height = 0;
            return false;
        }
        return true;
    }

    std::string truncateWithEllipsis(TTF_Font* font, const std::string& text, int maxWidth) {
        if (!font || maxWidth <= 0) return std::string();

        int width = 0;
        int height = 0;
        if (measureText(font, text, width, height) && width <= maxWidth) {
            return text;
        }

        const std::string ellipsis = "...";
        int ellipsisW = 0;
        int ellipsisH = 0;
        if (!measureText(font, ellipsis, ellipsisW, ellipsisH) || ellipsisW > maxWidth) {
            return std::string();
        }

        std::string trimmed = text;
        while (!trimmed.empty()) {
            trimmed.pop_back();
            std::string candidate = trimmed + ellipsis;
            if (measureText(font, candidate, width, height) && width <= maxWidth) {
                return candidate;
            }
        }

        return ellipsis;
    }

    TTF_Font* chooseFontForWidth(TTF_Font* primary, TTF_Font* fallback, const std::string& text, int maxWidth) {
        int width = 0;
        int height = 0;
        if (primary && measureText(primary, text, width, height) && width <= maxWidth) {
            return primary;
        }
        if (fallback && measureText(fallback, text, width, height) && width <= maxWidth) {
            return fallback;
        }
        return primary ? primary : fallback;
    }

    void drawInsetRect(SDL_Renderer* renderer, const SDL_Rect& rect, SDL_Color fill, SDL_Color border) {
        SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);
        SDL_RenderFillRect(renderer, &rect);
        SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
        SDL_RenderDrawRect(renderer, &rect);
    }
}

void RenderCard::drawCardFace(SDL_Renderer* renderer, RenderText& textRenderer, const Card& card, const SDL_Rect& rect,
                              TTF_Font* titleFont, TTF_Font* bodyFont, bool dimmed) {
    if (!renderer || !titleFont || !bodyFont) return;

    const SDL_Color frame = dimmed ? SDL_Color{120, 120, 120, 255} : SDL_Color{210, 200, 170, 255};
    const SDL_Color border = dimmed ? SDL_Color{60, 60, 60, 255} : SDL_Color{30, 24, 20, 255};
    const SDL_Color panel = dimmed ? SDL_Color{135, 135, 135, 255} : SDL_Color{235, 230, 215, 255};
    const SDL_Color ink = dimmed ? SDL_Color{80, 80, 80, 255} : SDL_Color{20, 18, 16, 255};

    drawInsetRect(renderer, rect, frame, border);

    const int innerPadding = std::max(6, rect.w / 14);
    const int titleBarHeight = std::max(22, rect.h / 8);
    const int artBoxHeight = std::max(68, rect.h / 3);

    SDL_Rect titleBar{rect.x + innerPadding, rect.y + innerPadding, rect.w - innerPadding * 2, titleBarHeight};
    drawInsetRect(renderer, titleBar, panel, border);

    SDL_Rect artBox{rect.x + innerPadding, titleBar.y + titleBar.h + innerPadding, rect.w - innerPadding * 2, artBoxHeight};
    drawInsetRect(renderer, artBox, dimmed ? SDL_Color{110, 110, 110, 255} : SDL_Color{160, 170, 190, 255}, border);

    SDL_Texture* imageTexture = getCardImageTexture(renderer, card.getId());
    if (imageTexture) {
        const Uint8 imageAlpha = dimmed ? static_cast<Uint8>(150) : static_cast<Uint8>(255);
        SDL_SetTextureAlphaMod(imageTexture, imageAlpha);
        SDL_Rect imageDst{artBox.x + 1, artBox.y + 1, std::max(1, artBox.w - 2), std::max(1, artBox.h - 2)};
        SDL_RenderCopy(renderer, imageTexture, nullptr, &imageDst);
        SDL_SetTextureAlphaMod(imageTexture, 255);

        if (dimmed) {
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 120, 120, 120, 95);
            SDL_RenderFillRect(renderer, &imageDst);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        }
    }

    SDL_Rect textBox{rect.x + innerPadding, artBox.y + artBox.h + innerPadding, rect.w - innerPadding * 2, rect.y + rect.h - (artBox.y + artBox.h + innerPadding) - innerPadding};
    drawInsetRect(renderer, textBox, panel, border);

    const std::string manaText = std::to_string(card.getManaCost());
    TTF_Font* manaFont = bodyFont;
    int manaTextW = 0;
    int manaTextH = 0;
    measureText(manaFont, manaText, manaTextW, manaTextH);

    const int manaPadding = 4;
    int manaBoxSize = std::max(16, titleBar.h - 4);
    if (manaTextW > 0 && manaTextH > 0) {
        manaBoxSize = std::max(manaBoxSize, std::max(manaTextW + manaPadding * 2, manaTextH + manaPadding * 2));
    }
    const int maxManaSize = rect.w - innerPadding * 2;
    if (manaBoxSize > maxManaSize) manaBoxSize = maxManaSize;

    const int manaX = rect.x + rect.w - manaBoxSize - innerPadding;
    int manaY = titleBar.y + (titleBar.h - manaBoxSize) / 2;
    if (manaY < rect.y + innerPadding) manaY = rect.y + innerPadding;

    SDL_Rect manaGem{manaX, manaY, manaBoxSize, manaBoxSize};
    drawInsetRect(renderer, manaGem, dimmed ? SDL_Color{120, 120, 120, 255} : SDL_Color{200, 170, 80, 255}, border);

    const int titleRightEdge = manaGem.x - 6;
    const int titleMaxWidth = std::max(0, titleRightEdge - (titleBar.x + 6));
    TTF_Font* titleUse = chooseFontForWidth(titleFont, bodyFont, card.getName(), titleMaxWidth);
    const std::string titleText = truncateWithEllipsis(titleUse, card.getName(), titleMaxWidth);

    SDL_Rect titleClip{titleBar.x + 4, titleBar.y + 2, titleMaxWidth, titleBar.h - 4};
    SDL_RenderSetClipRect(renderer, &titleClip);
    textRenderer.drawText(renderer, titleText, titleUse, ink, titleBar.x + 6, titleBar.y + 4);
    SDL_RenderSetClipRect(renderer, nullptr);

    if (manaTextW == 0 || manaTextH == 0) {
        measureText(manaFont, manaText, manaTextW, manaTextH);
    }
    const int manaTextX = manaGem.x + (manaGem.w - manaTextW) / 2;
    const int manaTextY = manaGem.y + (manaGem.h - manaTextH) / 2;
    textRenderer.drawText(renderer, manaText, manaFont, ink, manaTextX, manaTextY);

    textRenderer.drawText(renderer, "Value: " + std::to_string(card.getManaValue()), bodyFont, ink, textBox.x + 4, textBox.y + 4);

    const std::string effectText = card.getText();
    const bool hasEffect = effectText.find_first_not_of(" \t\n\r") != std::string::npos;
    if (hasEffect) {
        const int lineSkip = TTF_FontLineSkip(bodyFont);
        const int effectY = textBox.y + 4 + lineSkip;
        const int effectW = textBox.w - 8;
        const int effectH = textBox.h - (effectY - textBox.y) - 4;

        SDL_Rect clipRect{textBox.x + 4, effectY, effectW, effectH};
        SDL_RenderSetClipRect(renderer, &clipRect);

        SDL_Surface* effectSurface = TTF_RenderUTF8_Blended_Wrapped(bodyFont, effectText.c_str(), ink, effectW);
        if (effectSurface) {
            SDL_Texture* effectTexture = SDL_CreateTextureFromSurface(renderer, effectSurface);
            if (effectTexture) {
                SDL_Rect effectDst{clipRect.x, clipRect.y, effectSurface->w, effectSurface->h};
                SDL_RenderCopy(renderer, effectTexture, nullptr, &effectDst);
                SDL_DestroyTexture(effectTexture);
            }
            SDL_FreeSurface(effectSurface);
        }

        SDL_RenderSetClipRect(renderer, nullptr);
    }

    const auto* creature = dynamic_cast<const CreatureCard*>(&card);
    if (creature) {
        SDL_Rect statsBox{rect.x + rect.w - 38, rect.y + rect.h - 22, 30, 16};
        drawInsetRect(renderer, statsBox, panel, border);
        const std::string statsText = std::to_string(creature->getPower()) + "/" + std::to_string(creature->getToughness());
        textRenderer.drawText(renderer, statsText, bodyFont, ink, statsBox.x + 4, statsBox.y + 1);
    }
}

void RenderCard::drawHandCard(SDL_Renderer* renderer, RenderText& textRenderer, const Card& card, const SDL_Rect& cardRect, TTF_Font* fontSmall) {
    if (!renderer || !fontSmall) return;
    drawCardFace(renderer, textRenderer, card, cardRect, fontSmall, fontSmall, false);
}

void RenderCard::drawBoardCard(SDL_Renderer* renderer, RenderText& textRenderer, const Card& card, const SDL_Rect& cardRect, TTF_Font* fontSmall) {
    if (!renderer || !fontSmall) return;
    drawCardFace(renderer, textRenderer, card, cardRect, fontSmall, fontSmall, false);
}

void RenderCard::drawPreview(SDL_Renderer* renderer, RenderText& textRenderer, const Card& card, const SDL_Rect& previewRect, TTF_Font* fontSmall, TTF_Font* fontLarge) {
    if (!renderer || !fontSmall || !fontLarge) return;
    drawCardFace(renderer, textRenderer, card, previewRect, fontLarge, fontSmall, false);
}

void RenderCard::drawCardBack(SDL_Renderer* renderer, const SDL_Rect& cardRect) {
    if (!renderer) return;

    const SDL_Color border{25, 25, 35, 255};
    const SDL_Color panel{60, 80, 120, 255};
    const SDL_Color inner{45, 60, 95, 255};

    SDL_SetRenderDrawColor(renderer, panel.r, panel.g, panel.b, panel.a);
    SDL_RenderFillRect(renderer, &cardRect);
    SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
    SDL_RenderDrawRect(renderer, &cardRect);

    SDL_Rect inset{cardRect.x + 6, cardRect.y + 6, cardRect.w - 12, cardRect.h - 12};
    if (inset.w > 0 && inset.h > 0) {
        SDL_SetRenderDrawColor(renderer, inner.r, inner.g, inner.b, inner.a);
        SDL_RenderFillRect(renderer, &inset);
        SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
        SDL_RenderDrawRect(renderer, &inset);
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
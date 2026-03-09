#include "render/RenderCard.hpp"

#include "objects/Card.h"
#include "core/NetworkClient.hpp"
#include "objects/CreatureCard.h"
#include "render/RenderText.hpp"
#include "render/Theme.hpp"
#include "utils/RenderUtil.hpp"
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

    // ── rarity palette from mana cost ────────────────────────────────
    // 1   → grey   (Common)
    // 2-3 → orange (Rare)
    // 4-5 → purple (Epic)
    // 6+  → gold   (Legendary)
    struct RarityPalette {
        SDL_Color border;
        SDL_Color borderInner;
        SDL_Color manaGem;
        SDL_Color nameBg;
    };

    RarityPalette rarityFromMana(int mana) {
        if (mana <= 1) return {
            {160, 160, 170, 255}, {220, 220, 230, 255},
            {110, 110, 125, 255}, {40,  40,  50,  255}
        };
        if (mana <= 3) return {
            {210, 130,  40, 255}, {255, 200, 100, 255},
            {180,  90,  20, 255}, {55,  30,   8,  255}
        };
        if (mana <= 5) return {
            {160,  60, 200, 255}, {220, 140, 255, 255},
            {110,  30, 160, 255}, {38,  12,  55,  255}
        };
        return {
            {220, 180,  40, 255}, {255, 240, 140, 255},
            {160, 120,  10, 255}, {55,  40,   4,  255}
        };
    }

    // ── mana gem (filled circle with gradient) ────────────────────────
    void drawManaGem(SDL_Renderer* r, RenderText& tr, TTF_Font* font,
                     int mana, int cx, int cy, int radius, const RarityPalette& pal) {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        for (int dy = -radius; dy <= radius; ++dy) {
            int dx = (int)std::sqrt((double)(radius*radius - dy*dy));
            SDL_SetRenderDrawColor(r, 0, 0, 0, 100);
            SDL_RenderDrawLine(r, cx - dx + 2, cy + dy + 2, cx + dx + 2, cy + dy + 2);
        }
        SDL_Color gem = pal.manaGem;
        for (int dy = -radius; dy <= radius; ++dy) {
            int dx = (int)std::sqrt((double)(radius*radius - dy*dy));
            float t = (float)(dy + radius) / (2.0f * radius);
            SDL_Color row{
                (Uint8)std::min(255, (int)(gem.r + (1.0f - t) * 55)),
                (Uint8)std::min(255, (int)(gem.g + (1.0f - t) * 55)),
                (Uint8)std::min(255, (int)(gem.b + (1.0f - t) * 55)),
                255
            };
            SDL_SetRenderDrawColor(r, row.r, row.g, row.b, row.a);
            SDL_RenderDrawLine(r, cx - dx, cy + dy, cx + dx, cy + dy);
        }
        SDL_SetRenderDrawColor(r, 255, 255, 255, 160);
        for (int dy = -radius; dy <= radius; ++dy) {
            int dx = (int)std::sqrt((double)(radius*radius - dy*dy));
            SDL_RenderDrawPoint(r, cx - dx, cy + dy);
            SDL_RenderDrawPoint(r, cx + dx, cy + dy);
        }
        const std::string s = std::to_string(mana);
        int tw = 0, th = 0;
        RenderText::measureText(font, s, tw, th);
        tr.drawText(r, s, font, SDL_Color{255, 255, 255, 255}, cx - tw/2, cy - th/2);
    }

// ── shared card body draw ─────────────────────────────────────────
void drawCardBody(SDL_Renderer* renderer, const SDL_Rect& rect, int cornerRadius,
                  SDL_Color artColor, SDL_Color infoBg, int artH, int cardId) {
    RenderUtil::fillRoundedRect(renderer, rect, cornerRadius, infoBg);
    
    SDL_Rect artRect{rect.x, rect.y, rect.w, artH};
    
    // Try to load and render card image
    SDL_Texture* cardImage = getCardImageTexture(renderer, cardId);
    
    if (cardImage) {
        // Create a clipping region with rounded corners to contain the image
        SDL_Rect clipRect{rect.x + 2, rect.y + 2, rect.w - 4, artH - 4};
        
        // Set clipping region
        SDL_RenderSetClipRect(renderer, &clipRect);
        
        // Get image dimensions
        int imgW = 0, imgH = 0;
        SDL_QueryTexture(cardImage, nullptr, nullptr, &imgW, &imgH);
        
        // Calculate scaling to fit width while maintaining aspect ratio
        float scale = (float)clipRect.w / (float)imgW;
        int scaledH = (int)(imgH * scale);
        
        // Center vertically if needed
        int renderY = clipRect.y;
        if (scaledH < clipRect.h) {
            renderY += (clipRect.h - scaledH) / 2;
        }
        
        SDL_Rect imgRect{clipRect.x, renderY, clipRect.w, scaledH};
        
        // Render the image
        SDL_RenderCopy(renderer, cardImage, nullptr, &imgRect);
        
        // Clear clipping region
        SDL_RenderSetClipRect(renderer, nullptr);
        
        // Add a subtle gradient overlay to blend with card style
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        for (int y = cornerRadius; y < artH; ++y) {
            float t = (float)y / artH;
            Uint8 alpha = (Uint8)(80 * (t * t * t)); // Lighter overlay
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, alpha);
            SDL_RenderDrawLine(renderer, rect.x + 2, rect.y + y, rect.x + rect.w - 3, rect.y + y);
        }
    } else {
        // No image - use gradient background as fallback
        RenderUtil::fillRoundedRect(renderer, artRect, cornerRadius, artColor);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        for (int y = cornerRadius; y < artH; ++y) {
            float t = (float)y / artH;
            Uint8 alpha = (Uint8)(210 * (t * t * t));
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, alpha);
            SDL_RenderDrawLine(renderer, rect.x + 2, rect.y + y, rect.x + rect.w - 3, rect.y + y);
        }
    }
}
    // ── compact card (hand/board) ──────────────────────────────────────
    void drawCompact(SDL_Renderer* renderer, RenderText& textRenderer,
                     const Card& card, const SDL_Rect& rect,
                     TTF_Font* titleFont, TTF_Font* bodyFont, bool dimmed) {

        const RarityPalette pal = rarityFromMana(card.getManaCost());
        const int cornerRadius  = std::max(8, rect.w / 7);
        const int borderThick   = 3;
        const int bannerH       = std::max(22, rect.h / 5);
        const int artH          = rect.h - bannerH;

        const auto* creature = dynamic_cast<const CreatureCard*>(&card);
        SDL_Color artColor = dimmed ? SDL_Color{70, 60, 85, 255}
                           : (creature ? SDL_Color{110, 85, 145, 255}
                                       : SDL_Color{75, 105, 145, 255});

        drawCardBody(renderer, rect, cornerRadius, artColor, pal.nameBg, artH, card.getId());

        RenderUtil::drawRoundedBorder(renderer, rect, cornerRadius, SDL_Color{0,0,0,200}, 1);
        RenderUtil::drawRoundedBorder(renderer, {rect.x+1, rect.y+1, rect.w-2, rect.h-2},
                          cornerRadius-1, pal.border, borderThick);
        RenderUtil::drawRoundedBorder(renderer, {rect.x+borderThick+1, rect.y+borderThick+1,
                                     rect.w-2*(borderThick+1), rect.h-2*(borderThick+1)},
                          std::max(2, cornerRadius-borderThick-1), pal.borderInner, 1);

        SDL_SetRenderDrawColor(renderer, pal.border.r, pal.border.g, pal.border.b, 180);
        SDL_RenderDrawLine(renderer, rect.x + cornerRadius, rect.y + artH,
                           rect.x + rect.w - cornerRadius, rect.y + artH);

        const std::string nameText = RenderText::truncateWithEllipsis(titleFont, card.getName(), rect.w - 8);
        int nw=0, nh=0;
        RenderText::measureText(titleFont, nameText, nw, nh);
        textRenderer.drawText(renderer, nameText, titleFont,
                              SDL_Color{255, 245, 210, 255},
                              rect.x + (rect.w - nw) / 2,
                              rect.y + artH + (bannerH - nh) / 2);

        if (creature) {
            const std::string statsText = std::to_string(creature->getPower()) + "/" +
                                          std::to_string(creature->getToughness());
            int sw=0, sh=0;
            RenderText::measureText(bodyFont, statsText, sw, sh);
            const int bp = 4;
            SDL_Rect badge{rect.x + rect.w - sw - bp*2 - 4,
                           rect.y + artH - sh - bp - 4,
                           sw + bp*2, sh + bp};
            RenderUtil::fillRoundedRect(renderer, badge, 3, SDL_Color{170, 120, 45, 235});
            RenderUtil::drawRoundedBorder(renderer, badge, 3, SDL_Color{220, 175, 70, 255}, 1);
            textRenderer.drawText(renderer, statsText, bodyFont,
                                  SDL_Color{255, 245, 200, 255},
                                  badge.x + bp, badge.y + bp/2);
        }

        const int gemR   = std::max(9, rect.w / 7);
        const int gemOff = borderThick + 3;
        drawManaGem(renderer, textRenderer, bodyFont,
                    card.getManaCost(),
                    rect.x + gemR + gemOff,
                    rect.y + gemR + gemOff,
                    gemR, pal);
    }

    // ── full preview card ──────────────────────────────────────────────
    void drawFull(SDL_Renderer* renderer, RenderText& textRenderer,
                  const Card& card, const SDL_Rect& rect,
                  TTF_Font* titleFont, TTF_Font* bodyFont, bool dimmed, int scrollOffset) {

        const RarityPalette pal = rarityFromMana(card.getManaCost());
        const int cornerRadius  = std::max(10, rect.w / 8);
        const int borderThick   = 4;
        const int infoH         = rect.h * 2 / 5;
        const int artH          = rect.h - infoH;

        const auto* creature = dynamic_cast<const CreatureCard*>(&card);
        SDL_Color artColor = dimmed ? SDL_Color{70, 60, 85, 255}
                           : (creature ? SDL_Color{100, 75, 140, 255}
                                       : SDL_Color{65,  95, 140, 255});

        SDL_Color glow = pal.border;
        glow.a = 130;
        RenderUtil::drawRoundedBorder(renderer, {rect.x-3, rect.y-3, rect.w+6, rect.h+6},
                          cornerRadius+3, glow, 4);

        drawCardBody(renderer, rect, cornerRadius, artColor, pal.nameBg, artH, card.getId());

        RenderUtil::drawRoundedBorder(renderer, rect, cornerRadius, SDL_Color{0,0,0,220}, 1);
        RenderUtil::drawRoundedBorder(renderer, {rect.x+1, rect.y+1, rect.w-2, rect.h-2},
                          cornerRadius-1, pal.border, borderThick);
        RenderUtil::drawRoundedBorder(renderer, {rect.x+borderThick+1, rect.y+borderThick+1,
                                     rect.w-2*(borderThick+1), rect.h-2*(borderThick+1)},
                          std::max(2, cornerRadius-borderThick-1), pal.borderInner, 1);

        SDL_Rect infoRect{rect.x, rect.y + artH, rect.w, infoH};
        SDL_SetRenderDrawColor(renderer, pal.border.r, pal.border.g, pal.border.b, 180);
        SDL_RenderDrawLine(renderer, rect.x + cornerRadius, rect.y + artH,
                           rect.x + rect.w - cornerRadius, rect.y + artH);

        const int pad = std::max(10, rect.w / 12);
        const int maxNameW = rect.w - pad * 2 - 20;
        const std::string nameText = card.getName();

        int nw = 0, nh = 0;
        RenderText::measureText(titleFont, nameText, nw, nh);

        if (nw > maxNameW) {
            SDL_Surface* nameSurf = TTF_RenderUTF8_Blended_Wrapped(
                titleFont, nameText.c_str(), 
                SDL_Color{255, 245, 200, 255}, 
                maxNameW
            );
            if (nameSurf) {
                SDL_Texture* nameTex = SDL_CreateTextureFromSurface(renderer, nameSurf);
                if (nameTex) {
                    SDL_Rect dst{
                        infoRect.x + (infoRect.w - nameSurf->w) / 2,
                        infoRect.y + 10,
                        nameSurf->w,
                        nameSurf->h
                    };
                    SDL_RenderCopy(renderer, nameTex, nullptr, &dst);
                    SDL_DestroyTexture(nameTex);
                    nh = nameSurf->h;
                }
                SDL_FreeSurface(nameSurf);
            }
        } else {
            textRenderer.drawText(renderer, nameText, titleFont,
                                  SDL_Color{255, 245, 200, 255},
                                  infoRect.x + (infoRect.w - nw) / 2,
                                  infoRect.y + 10);
        }

        const std::string effectText = card.getText();
        const bool hasEffect = !effectText.empty() &&
                               effectText.find_first_not_of(" \t\n\r") != std::string::npos;
        const int effectY    = infoRect.y + nh + 16;
        const int effectW    = rect.w - pad*2;
        const int effectMaxH = rect.y + rect.h - effectY - (creature ? 36 : 14);

        if (hasEffect && effectMaxH > 0) {
            SDL_Surface* surf = TTF_RenderUTF8_Blended_Wrapped(
                bodyFont, effectText.c_str(), SDL_Color{215, 210, 195, 255}, effectW);
            
            if (surf) {
                SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
                if (tex) {
                    SDL_Rect clip{infoRect.x + pad, effectY, effectW, effectMaxH};
                    SDL_RenderSetClipRect(renderer, &clip);
                    
                    SDL_Rect dst{clip.x, clip.y - scrollOffset, surf->w, surf->h};
                    SDL_RenderCopy(renderer, tex, nullptr, &dst);
                    
                    SDL_RenderSetClipRect(renderer, nullptr);
                    
                    if (surf->h > effectMaxH) {
                        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                        
                        if (scrollOffset < surf->h - effectMaxH) {
                            SDL_SetRenderDrawColor(renderer, 255, 245, 200, 200);
                            int arrowY = clip.y + clip.h - 8;
                            int arrowX = clip.x + clip.w / 2;
                            for (int i = 0; i < 5; i++) {
                                SDL_RenderDrawLine(renderer, arrowX - i, arrowY - i, 
                                                 arrowX + i, arrowY - i);
                            }
                        }
                        
                        if (scrollOffset > 0) {
                            SDL_SetRenderDrawColor(renderer, 255, 245, 200, 200);
                            int arrowY = clip.y + 8;
                            int arrowX = clip.x + clip.w / 2;
                            for (int i = 0; i < 5; i++) {
                                SDL_RenderDrawLine(renderer, arrowX - i, arrowY + i, 
                                                 arrowX + i, arrowY + i);
                            }
                        }
                    }
                    
                    SDL_DestroyTexture(tex);
                }
                SDL_FreeSurface(surf);
            }
        }

        if (creature) {
            const std::string statsText = std::to_string(creature->getPower()) + "/" +
                                          std::to_string(creature->getToughness());
            int sw=0, sh=0;
            RenderText::measureText(bodyFont, statsText, sw, sh);
            const int bp = 7;
            SDL_Rect badge{rect.x + rect.w - sw - bp*2 - pad,
                           rect.y + rect.h - sh - bp - 10,
                           sw + bp*2, sh + bp};
            RenderUtil::fillRoundedRect(renderer, badge, 5, SDL_Color{170, 120, 45, 240});
            RenderUtil::drawRoundedBorder(renderer, badge, 5, SDL_Color{225, 180, 70, 255}, 2);
            textRenderer.drawText(renderer, statsText, bodyFont,
                                  SDL_Color{255, 245, 200, 255},
                                  badge.x + bp, badge.y + bp/2);
        }

        const int gemR   = std::max(12, rect.w / 9);
        const int gemOff = borderThick + 5;
        drawManaGem(renderer, textRenderer, titleFont,
                    card.getManaCost(),
                    rect.x + gemR + gemOff,
                    rect.y + gemR + gemOff,
                    gemR, pal);
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
#include "render/RenderCombatPhaseWidget.hpp"

#include "render/RenderText.hpp"
#include "render/Theme.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <algorithm>

namespace {
    SDL_Texture* getCombatIconTexture(SDL_Renderer* renderer) {
        if (!renderer) {
            return nullptr;
        }

        static SDL_Texture* cachedTexture = nullptr;
        static SDL_Renderer* cachedRenderer = nullptr;

        if (cachedTexture && cachedRenderer == renderer) {
            return cachedTexture;
        }

        if (cachedTexture) {
            SDL_DestroyTexture(cachedTexture);
            cachedTexture = nullptr;
            cachedRenderer = nullptr;
        }

        SDL_Surface* surface = IMG_Load("assets/images/combat.png");
        if (!surface) {
            return nullptr;
        }

        cachedTexture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_FreeSurface(surface);
        if (!cachedTexture) {
            return nullptr;
        }

        cachedRenderer = renderer;
        return cachedTexture;
    }
}

void RenderCombatPhaseWidget::draw(SDL_Renderer* renderer, RenderText& textRenderer, TTF_Font* font,
                                   int screenW, int screenH,
                                   const std::vector<SDL_Rect>& opponentSlots,
                                   const std::vector<SDL_Rect>& playSlots,
                                   bool combatPhaseActive, float barProgress,
                                   const std::string& combatLabel) {
    if (!renderer) {
        return;
    }

    int combatTextW = 0;
    int combatTextH = 0;
    if (font) {
        TTF_SizeText(font, combatLabel.c_str(), &combatTextW, &combatTextH);
    }

    const int iconSize = Theme::CombatWidget::ICON_SIZE;
    const int barWidth = Theme::CombatWidget::BAR_WIDTH;
    const int barHeight = Theme::CombatWidget::BAR_HEIGHT;
    const int iconGap = Theme::CombatWidget::ICON_GAP;
    const int textBarGap = Theme::CombatWidget::TEXT_BAR_GAP;
    const int widgetHeight = std::max(iconSize, combatTextH + textBarGap + barHeight);
    const int widgetWidth = iconSize + iconGap + std::max(combatTextW, barWidth);
    const int widgetX = (screenW - widgetWidth) / 2;

    int widgetY = (screenH - widgetHeight) / 2;
    if (!opponentSlots.empty() && !playSlots.empty()) {
        const int gapTop = opponentSlots.front().y + opponentSlots.front().h;
        const int gapBottom = playSlots.front().y;
        if (gapBottom > gapTop) {
            widgetY = gapTop + (gapBottom - gapTop - widgetHeight) / 2;
        }
    }

    const SDL_Rect iconRect{widgetX, widgetY + (widgetHeight - iconSize) / 2, iconSize, iconSize};
    const int contentX = iconRect.x + iconRect.w + iconGap;
    const SDL_Rect textRect{contentX, widgetY, std::max(combatTextW, barWidth), combatTextH};
    const SDL_Rect barOuter{contentX, textRect.y + textRect.h + textBarGap, barWidth, barHeight};
    const int barInnerWidth = std::max(0, static_cast<int>(static_cast<float>(barOuter.w - 2) * barProgress));
    const SDL_Rect barInner{barOuter.x + 1, barOuter.y + 1, barInnerWidth, std::max(0, barOuter.h - 2)};

    if (SDL_Texture* combatIcon = getCombatIconTexture(renderer)) {
        SDL_RenderCopy(renderer, combatIcon, nullptr, &iconRect);
    }

    if (font) {
        textRenderer.drawText(renderer, combatLabel, font, Theme::CombatWidget::LABEL_TEXT, textRect.x, textRect.y);
    }

    SDL_SetRenderDrawColor(renderer,
                           Theme::CombatWidget::BAR_BACKGROUND.r,
                           Theme::CombatWidget::BAR_BACKGROUND.g,
                           Theme::CombatWidget::BAR_BACKGROUND.b,
                           Theme::CombatWidget::BAR_BACKGROUND.a);
    SDL_RenderFillRect(renderer, &barOuter);
    SDL_SetRenderDrawColor(renderer,
                           Theme::CombatWidget::BAR_BORDER.r,
                           Theme::CombatWidget::BAR_BORDER.g,
                           Theme::CombatWidget::BAR_BORDER.b,
                           Theme::CombatWidget::BAR_BORDER.a);
    SDL_RenderDrawRect(renderer, &barOuter);

    if (barInner.w > 0 && barInner.h > 0) {
        const SDL_Color fillColor = combatPhaseActive
            ? Theme::CombatWidget::BAR_FILL_ACTIVE
            : Theme::CombatWidget::BAR_FILL_INACTIVE;
        SDL_SetRenderDrawColor(renderer, fillColor.r, fillColor.g, fillColor.b, fillColor.a);
        SDL_RenderFillRect(renderer, &barInner);
    }
}

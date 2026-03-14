#ifndef THEME_HPP
#define THEME_HPP

#include <SDL2/SDL.h>

namespace Theme {

    // ── background ───────────────────────────────────────────────────
    constexpr SDL_Color BG                  = {18,  12,  35,  255};

    // ── banner ───────────────────────────────────────────────────────
    constexpr SDL_Color BANNER_FILL         = {55,  20,  100, 255};
    constexpr SDL_Color BANNER_BORDER       = {240, 192, 64,  255};
    constexpr SDL_Color BANNER_TEXT         = {220, 210, 185, 255};
    constexpr SDL_Color BANNER_GLOW         = {120, 60,  220, 255};

    // ── buttons ──────────────────────────────────────────────────────
    constexpr SDL_Color BTN_BORDER          = {220, 210, 185, 255};
    constexpr SDL_Color BTN_TEXT            = {240, 235, 220, 255};
    constexpr SDL_Color BTN_START           = {35,  160, 130, 255};
    constexpr SDL_Color BTN_QUIT            = {185, 50,  70,  255};
    constexpr SDL_Color BTN_BUILD           = {195, 155, 30,  255};
    constexpr SDL_Color BTN_CONNECT         = {75,  95,  140, 255};
    constexpr SDL_Color BTN_PRIMARY         = {70,  120, 200, 255};
    constexpr SDL_Color BTN_SECONDARY       = {70,  70,  70,  255};

    // ── text ─────────────────────────────────────────────────────────
    constexpr SDL_Color TEXT_PRIMARY        = {245, 245, 245, 255};
    constexpr SDL_Color TEXT_MUTED          = {190, 190, 190, 255};
    constexpr SDL_Color TEXT_IVORY          = {220, 210, 185, 255};

    // ── input fields ─────────────────────────────────────────────────
    constexpr SDL_Color INPUT_FILL          = {25,  22,  45,  255};
    constexpr SDL_Color INPUT_ACTIVE        = {40,  35,  70,  255};
    constexpr SDL_Color INPUT_BORDER        = {100, 100, 120, 255};
    constexpr SDL_Color INPUT_BORDER_ACTIVE = {120, 60,  220, 255};
    constexpr SDL_Color INPUT_BORDER_IDLE   = {100, 100, 120, 255};

    // ── panel ────────────────────────────────────────────────────────
    constexpr SDL_Color PANEL_FILL          = {30,  25,  50,  230};
    constexpr SDL_Color PANEL_BORDER        = {240, 192, 64,  100};

    // ── feedback ─────────────────────────────────────────────────────
    constexpr SDL_Color ERROR_RED           = {220, 90,  90,  255};
    constexpr SDL_Color SUCCESS_GREEN       = {80,  200, 120, 255};

    // ── sizing ───────────────────────────────────────────────────────
    constexpr int BTN_W                     = 260;
    constexpr int BTN_H                     = 65;
    constexpr int BTN_RADIUS                = 14;
    constexpr int INPUT_RADIUS              = 8;
    constexpr int PANEL_RADIUS              = 16;
    constexpr int BANNER_W                  = 640;
    constexpr int BANNER_H                  = 100;

    namespace Effects {
        constexpr SDL_Color SHADOW_COLOR                = {0, 0, 0, 90};
        constexpr int SHADOW_OFFSET                     = 5;
        constexpr int BUTTON_GLOW_LAYERS               = 6;
        constexpr Uint8 BUTTON_GLOW_MAX_ALPHA          = 42;
        constexpr int INPUT_GLOW_LAYERS                = 5;
        constexpr Uint8 INPUT_GLOW_BASE_ALPHA          = 20;
        constexpr Uint8 INPUT_GLOW_ALPHA_STEP          = 18;
        constexpr int BANNER_TEXT_GLOW_LAYERS          = 4;
        constexpr Uint8 BANNER_TEXT_GLOW_BASE_ALPHA    = 15;
        constexpr Uint8 BANNER_TEXT_GLOW_STEP_ALPHA    = 15;
    }

    namespace Playing {
        constexpr SDL_Color BACKGROUND                 = {28, 22, 45, 255};

        constexpr int CARD_WIDTH                       = 100;
        constexpr int CARD_HEIGHT                      = 150;
        constexpr float HAND_MAX_WIDTH_RATIO          = 0.75f;
        constexpr int HAND_DEFAULT_SPACING            = 10;
        constexpr int HAND_Y_OFFSET                    = 80;
        constexpr int MAX_HAND_SIZE                    = 10;

        constexpr int SLOT_COUNT                       = 5;
        constexpr int SLOT_WIDTH                       = 115;
        constexpr int SLOT_HEIGHT                      = 145;
        constexpr int SLOT_SPACING                     = 15;
        constexpr int SLOT_TO_HAND_GAP                 = 16;
        constexpr int SCREEN_MARGIN                    = 20;
        constexpr int SIDE_ZONE_MARGIN                 = 10;
        constexpr int SELF_DECK_GAP                    = 20;
        constexpr int OPPONENT_SIDE_GAP                = 28;
        constexpr int OPPONENT_ZONE_OFFSET             = 210;

        constexpr Uint32 HOVER_PREVIEW_DELAY_MS        = 1000;
        constexpr int PREVIEW_MAX_WIDTH                = 320;
        constexpr int PREVIEW_MARGIN                   = 20;
        constexpr int OVERLAY_MARGIN                   = 20;

        constexpr int OPPONENT_BAR_WIDTH               = 480;
        constexpr int OPPONENT_BAR_HEIGHT              = 40;
        constexpr int OPPONENT_BAR_TOP                 = 12;
        constexpr int OPPONENT_BAR_TEXT_PADDING        = 12;
        constexpr int OPPONENT_BAR_DIVIDER_GAP         = 24;
        constexpr int OPPONENT_BAR_DIVIDER_INSET       = 8;

        constexpr int PLAYER_BAR_WIDTH                 = 400;
        constexpr int PLAYER_BAR_HEIGHT                = 50;
        constexpr int PLAYER_BAR_BOTTOM_MARGIN         = 20;
        constexpr int PLAYER_BAR_GLOW_INSET            = 3;
        constexpr int PLAYER_BAR_DIVIDER_INSET         = 10;

        constexpr int MENU_BUTTON_WIDTH                = 120;
        constexpr int MENU_BUTTON_HEIGHT               = 50;
        constexpr int PAUSE_MODAL_WIDTH                = 400;
        constexpr int PAUSE_MODAL_HEIGHT               = 280;
        constexpr int PAUSE_MODAL_TITLE_TOP            = 40;
        constexpr int PAUSE_BUTTON_WIDTH               = 320;
        constexpr int PAUSE_BUTTON_HEIGHT              = 60;
        constexpr int PAUSE_BUTTON_SPACING             = 20;
        constexpr int PAUSE_BUTTON_TOP                 = 100;
        constexpr int EXIT_MODAL_WIDTH                 = 480;
        constexpr int EXIT_MODAL_HEIGHT                = 320;
        constexpr int EXIT_MODAL_TITLE_TOP             = 40;
        constexpr int EXIT_MODAL_QUESTION_TOP          = 100;
        constexpr int EXIT_BUTTON_WIDTH                = 200;
        constexpr int EXIT_BUTTON_HEIGHT               = 60;
        constexpr int EXIT_BUTTON_SPACING              = 20;
        constexpr int EXIT_BUTTON_TOP                  = 140;
        constexpr int RETURN_BUTTON_WIDTH              = 260;
        constexpr int RETURN_BUTTON_HEIGHT             = 62;

        constexpr int DECK_STACK_MAX_CARDS             = 5;
        constexpr int DECK_STACK_X_OFFSET              = 4;
        constexpr int DECK_STACK_Y_OFFSET              = 2;
        constexpr int ZONE_TEXT_PADDING                = 6;

        constexpr int TARGET_PROMPT_X                  = 20;
        constexpr int TARGET_PROMPT_Y                  = 130;

        constexpr SDL_Color OPPONENT_DISCARD_FILL      = {70, 60, 80, 255};
        constexpr SDL_Color OPPONENT_DISCARD_BORDER    = {170, 150, 190, 255};
        constexpr SDL_Color OPPONENT_BAR_BORDER        = {100, 80, 120, 255};
        constexpr SDL_Color OPPONENT_BAR_DIVIDER       = {100, 80, 120, 200};
        constexpr SDL_Color OPPONENT_LABEL             = {180, 170, 200, 255};
        constexpr SDL_Color OPPONENT_HEALTH_TEXT       = {200, 140, 140, 255};
        constexpr SDL_Color OPPONENT_MANA_TEXT         = {140, 170, 200, 255};

        constexpr SDL_Color PLAYER_BAR_GLOW_FILL       = {80, 60, 100, 100};
        constexpr SDL_Color PLAYER_BAR_GLOW_BORDER     = {140, 120, 180, 180};
        constexpr SDL_Color PLAYER_BAR_FILL            = {30, 25, 45, 240};
        constexpr SDL_Color PLAYER_BAR_BORDER          = {140, 120, 180, 255};
        constexpr SDL_Color PLAYER_HEALTH_GLOW_FILL    = {60, 20, 20, 0};
        constexpr SDL_Color PLAYER_HEALTH_GLOW_BORDER  = {220, 80, 80, 150};
        constexpr SDL_Color PLAYER_HEALTH_FILL         = {40, 20, 20, 200};
        constexpr SDL_Color PLAYER_HEALTH_BORDER       = {180, 60, 60, 255};
        constexpr SDL_Color PLAYER_HEALTH_TEXT         = {255, 220, 220, 255};
        constexpr SDL_Color PLAYER_MANA_GLOW_FILL      = {20, 40, 80, 0};
        constexpr SDL_Color PLAYER_MANA_GLOW_BORDER    = {100, 160, 255, 150};
        constexpr SDL_Color PLAYER_MANA_FILL           = {20, 30, 50, 200};
        constexpr SDL_Color PLAYER_MANA_BORDER         = {60, 120, 200, 255};
        constexpr SDL_Color PLAYER_MANA_TEXT           = {180, 220, 255, 255};

        constexpr SDL_Color TARGET_HIGHLIGHT           = {250, 220, 90, 255};
        constexpr SDL_Color TARGET_PROMPT_TEXT         = {250, 240, 180, 255};

        constexpr SDL_Color MENU_BUTTON_FILL           = {50, 50, 60, 200};
        constexpr SDL_Color MENU_BUTTON_BORDER         = {90, 90, 110, 255};
        constexpr SDL_Color MENU_BUTTON_BORDER_HOVER   = {120, 120, 140, 255};
        constexpr SDL_Color MENU_BUTTON_TEXT           = {220, 220, 230, 255};

        constexpr SDL_Color PAUSE_OVERLAY              = {0, 0, 0, 180};
        constexpr SDL_Color EXIT_OVERLAY               = {0, 0, 0, 200};
        constexpr SDL_Color SURRENDER_OVERLAY          = {0, 0, 0, 190};

        constexpr SDL_Color PAUSE_MODAL_BORDER         = {160, 120, 200, 255};
        constexpr SDL_Color EXIT_MODAL_BORDER          = {240, 192, 64, 150};

        constexpr SDL_Color RESUME_BUTTON_FILL         = {35, 160, 130, 240};
        constexpr SDL_Color RESUME_BUTTON_FILL_HOVER   = {50, 180, 120, 255};
        constexpr SDL_Color RESUME_BUTTON_BORDER       = {80, 200, 140, 255};
        constexpr SDL_Color RESUME_BUTTON_BORDER_HOVER = {100, 220, 160, 255};

        constexpr SDL_Color DANGER_BUTTON_FILL         = {185, 50, 70, 240};
        constexpr SDL_Color DANGER_BUTTON_FILL_HOVER   = {220, 70, 90, 255};
        constexpr SDL_Color DANGER_BUTTON_BORDER       = {220, 90, 110, 255};
        constexpr SDL_Color DANGER_BUTTON_BORDER_HOVER = {255, 120, 140, 255};

        constexpr SDL_Color PRIMARY_ACTION_FILL        = {70, 120, 200, 240};
        constexpr SDL_Color PRIMARY_ACTION_FILL_HOVER  = {90, 140, 220, 255};
        constexpr SDL_Color PRIMARY_ACTION_BORDER      = {110, 160, 240, 255};
        constexpr SDL_Color PRIMARY_ACTION_BORDER_HOVER = {140, 180, 255, 255};

        constexpr SDL_Color RETURN_BUTTON_FILL         = {60, 100, 140, 220};
        constexpr SDL_Color RETURN_BUTTON_FILL_HOVER   = {100, 160, 220, 255};
        constexpr SDL_Color RETURN_BUTTON_BORDER       = {80, 130, 180, 255};
        constexpr SDL_Color RETURN_BUTTON_BORDER_HOVER = {100, 160, 220, 255};
    }

    namespace Board {
        constexpr int LABEL_MIN_Y                      = 10;
        constexpr int LABEL_OFFSET_Y                   = 22;
        constexpr int ZONE_CORNER_RADIUS               = 8;
        constexpr int ZONE_BORDER_THICKNESS            = 2;
        constexpr int DISCARD_CORNER_RADIUS            = 10;
        constexpr int DISCARD_TEXT_PADDING             = 6;
        constexpr int DISCARD_BORDER_THICKNESS         = 2;
        constexpr int DISCARD_HOVER_BORDER_THICKNESS   = 3;

        constexpr SDL_Color OPPONENT_LABEL             = {180, 160, 200, 255};
        constexpr SDL_Color OPPONENT_ZONE_FILL         = {50, 40, 55, 180};
        constexpr SDL_Color OPPONENT_ZONE_BORDER       = {120, 90, 130, 255};

        constexpr SDL_Color PLAYER_LABEL               = {180, 220, 180, 255};
        constexpr SDL_Color PLAYER_ZONE_FILL           = {40, 60, 50, 180};
        constexpr SDL_Color PLAYER_ZONE_BORDER         = {100, 160, 120, 255};

        constexpr SDL_Color DISCARD_FILL               = {45, 60, 80, 180};
        constexpr SDL_Color DISCARD_FILL_HOVER         = {60, 75, 100, 200};
        constexpr SDL_Color DISCARD_BORDER             = {90, 120, 160, 255};
        constexpr SDL_Color DISCARD_BORDER_HOVER       = {120, 150, 200, 255};
        constexpr SDL_Color DISCARD_TITLE_TEXT         = {220, 230, 255, 255};
        constexpr SDL_Color DISCARD_DESCRIPTION_TEXT   = {180, 200, 230, 255};
    }

    namespace Card {
        constexpr SDL_Color BADGE_TEXT                 = {255, 255, 255, 255};
        constexpr SDL_Color CREATURE_BORDER           = {72, 92, 190, 255};
        constexpr SDL_Color SPELL_BORDER              = {70, 162, 90, 255};
        constexpr SDL_Color CREATURE_BASE             = {20, 28, 48, 255};
        constexpr SDL_Color SPELL_BASE                = {14, 34, 22, 255};
        constexpr SDL_Color DIMMED_BORDER             = {95, 95, 95, 255};
        constexpr SDL_Color DIMMED_BASE               = {35, 35, 35, 255};
        constexpr SDL_Color OUTER_BORDER              = {0, 0, 0, 200};
        constexpr SDL_Color ART_BORDER                = {20, 20, 20, 220};
        constexpr SDL_Color ART_DIMMED_FALLBACK       = {62, 62, 62, 255};
        constexpr SDL_Color ART_CREATURE_FALLBACK     = {34, 66, 115, 255};
        constexpr SDL_Color ART_SPELL_FALLBACK        = {24, 78, 44, 255};
        constexpr SDL_Color NAME_PLATE_FILL           = {14, 18, 26, 212};
        constexpr SDL_Color NAME_TEXT                 = {228, 235, 255, 255};
        constexpr SDL_Color TYPE_LINE_CREATURE_FILL   = {11, 28, 46, 235};
        constexpr SDL_Color TYPE_LINE_SPELL_FILL      = {10, 45, 26, 235};
        constexpr SDL_Color TYPE_PILL_CREATURE_FILL   = {34, 74, 130, 255};
        constexpr SDL_Color TYPE_PILL_SPELL_FILL      = {30, 99, 59, 255};
        constexpr SDL_Color TYPE_TEXT                 = {198, 242, 230, 255};
        constexpr SDL_Color TEXT_BOX_FILL             = {8, 22, 37, 230};
        constexpr SDL_Color TEXT_BOX_BORDER           = {20, 56, 86, 255};
        constexpr SDL_Color TEXT_BODY                 = {172, 206, 227, 255};
        constexpr SDL_Color BOTTOM_BAR_FILL           = {9, 18, 33, 245};
        constexpr SDL_Color STAT_LABEL                = {98, 118, 170, 255};
        constexpr SDL_Color STAT_VALUE                = {220, 230, 255, 255};
        constexpr SDL_Color MANA_BADGE_CREATURE_FILL  = {74, 58, 175, 255};
        constexpr SDL_Color MANA_BADGE_CREATURE_BORDER = {170, 150, 255, 255};
        constexpr SDL_Color MANA_BADGE_SPELL_FILL     = {55, 138, 78, 255};
        constexpr SDL_Color MANA_BADGE_SPELL_BORDER   = {146, 233, 168, 255};
        constexpr SDL_Color VALUE_BADGE_CREATURE_FILL = {49, 80, 155, 255};
        constexpr SDL_Color VALUE_BADGE_CREATURE_BORDER = {142, 175, 255, 255};
        constexpr SDL_Color VALUE_BADGE_SPELL_FILL    = {47, 126, 75, 255};
        constexpr SDL_Color VALUE_BADGE_SPELL_BORDER  = {128, 225, 151, 255};
        constexpr SDL_Color CARD_BACK_OUTER_FILL      = {65, 48, 95, 255};
        constexpr SDL_Color CARD_BACK_OUTER_BORDER    = {35, 25, 55, 255};
        constexpr SDL_Color CARD_BACK_INNER_BORDER    = {115, 85, 155, 255};
        constexpr SDL_Color CARD_BACK_INSET_FILL      = {85, 60, 120, 255};
        constexpr SDL_Color CARD_BACK_INSET_BORDER    = {125, 95, 165, 255};

        constexpr int MIN_CORNER_RADIUS               = 8;
        constexpr int BORDER_THICKNESS                = 3;
        constexpr int EXPANDED_BORDER_THICKNESS       = 4;
        constexpr int INNER_PADDING                   = 4;
        constexpr int ART_INSET                       = 2;
        constexpr int MIN_ART_HEIGHT                  = 28;
        constexpr int MIN_NAME_HEIGHT                 = 18;
        constexpr int MIN_TYPE_HEIGHT                 = 16;
        constexpr int MIN_BOTTOM_HEIGHT               = 24;
        constexpr int MIN_COLLAPSED_BOTTOM_HEIGHT     = 20;
        constexpr int MIN_TEXT_HEIGHT                 = 24;
        constexpr int MIN_MANA_RADIUS                 = 12;
        constexpr int MIN_NAME_PADDING                = 6;
        constexpr int MIN_TYPE_PILL_PADDING           = 6;
        constexpr int TEXT_CLIP_HORIZONTAL_PADDING    = 6;
        constexpr int TEXT_CLIP_VERTICAL_PADDING      = 4;
        constexpr int TEXT_LINE_GAP                   = 2;
        constexpr int BOTTOM_SECTION_PADDING          = 6;
        constexpr int BOTTOM_SECTION_GAP              = 8;
        constexpr int BOTTOM_COMPACT_SECTION_GAP      = 2;
        constexpr int MIN_STAT_LABEL_TOP_PADDING      = 2;
        constexpr int MIN_STAT_BASELINE_OFFSET        = 16;
        constexpr int CARD_BACK_MIN_RADIUS            = 6;
        constexpr int CARD_BACK_INSET                 = 8;
        constexpr int CARD_BACK_MIN_INSET_RADIUS      = 3;
    }
}

#endif
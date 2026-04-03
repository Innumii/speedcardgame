#include "states/Login.hpp"
#include "core/Game.hpp"
#include "core/NetworkClient.hpp"
#include "utils/HttpUtil.hpp"
#include "utils/JsonUtil.hpp"
#include "utils/EnvUtil.hpp"
#include "utils/RenderUtil.hpp"
#include "render/RenderBackdrop.hpp"
#include "render/RenderButton.hpp"
#include "render/RenderText.hpp"
#include "render/RenderBanner.hpp"
#include "render/Theme.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <cmath>

// ── network helpers ───────────────────────────────────────────────────────────

namespace {
    constexpr std::size_t kMaxUsernameLen = 64;
    constexpr std::size_t kMaxPasswordLen = 32;

    constexpr float kRefW = 1200.0F;
    constexpr float kRefH = 850.0F;

    bool parseUserId(const std::string& body, int& outId, std::string& error) {
        std::string userJson;
        if (!JsonUtil::extractJsonObject(body, "user", userJson)) {
            error = "missing user object";
            return false;
        }
        if (JsonUtil::readJsonIntField(userJson, "id", outId)) return true;
        if (JsonUtil::readJsonIntField(userJson, "ID", outId)) return true;
        error = "missing user id";
        return false;
    }

    bool authenticate(const std::string& email, const std::string& password,
                      int& outId, std::string& outSessionId, std::string& error) {
        if (email.empty() || password.empty()) {
            error = "email and password required";
            return false;
        }
        const std::string host = EnvUtil::getAuthServiceHost();
        const int         port = EnvUtil::getAuthServicePort();

        std::ostringstream payload;
        payload << "{\"email\":\"" << JsonUtil::escapeJsonString(email)
            << "\",\"password\":\"" << JsonUtil::escapeJsonString(password) << "\"}";

        int statusCode = -1;
        std::string responseBody;
        if (!HttpUtil::sendHttp(host, port, "POST", "/auth/login", payload.str(), statusCode, responseBody)) {
            error = "auth service unreachable";
            return false;
        }
        if (statusCode != 200) {
            if (!JsonUtil::readJsonStringField(responseBody, "error", error))
                error = "login failed";
            return false;
        }
        if (!JsonUtil::readJsonStringField(responseBody, "session_id", outSessionId)) {
            error = "missing session id";
            return false;
        }

        return parseUserId(responseBody, outId, error);
    }
}

// ── Login implementation ──────────────────────────────────────────────────────

Login::Login()  = default;
Login::~Login() = default;

void Login::enter(Game& game) {
    setActiveField(Field::Username);
    SDL_StartTextInput();
}

void Login::exit(Game& game) {
    SDL_StopTextInput();
    loginPressed    = false;
    registerPressed = false;
    loginHover      = false;
    backHover       = false;
}

void Login::setActiveField(Field field) {
    activeField = field;
    activeField == Field::None ? SDL_StopTextInput() : SDL_StartTextInput();
}

void Login::updateLayout(SDL_Renderer* renderer) {
    int screenW = 800, screenH = 600;
    if (renderer) SDL_GetRendererOutputSize(renderer, &screenW, &screenH);

    const float scale = std::min(
        static_cast<float>(screenW) / kRefW,
        static_cast<float>(screenH) / kRefH);

    // All reference values are in 1200x850 space
    const int inputH     = static_cast<int>(52  * scale);
    const int inputGapY  = static_cast<int>(40  * scale);
    const int btnH       = static_cast<int>(52  * scale);
    const int btnGapY    = static_cast<int>(36  * scale);
    const int titleSpace = static_cast<int>(120 * scale);
    const int panelPad   = static_cast<int>(40  * scale);
    const int btnGapX    = static_cast<int>(16  * scale);
    const int labelOffY  = static_cast<int>(30  * scale);

    // Panel height derived from its contents, not hardcoded
    const int panelH = titleSpace
                     + inputH + inputGapY
                     + inputH + btnGapY
                     + btnH
                     + static_cast<int>(40 * scale);  // bottom padding

    const int panelW = std::min(
        static_cast<int>(520 * scale),
        static_cast<int>(screenW * 0.75f));

    const int bannerH    = static_cast<int>(Theme::BANNER_H * scale);
    const int bannerTopY = static_cast<int>(30 * scale);
    const int panelX     = (screenW - panelW) / 2;
    const int panelY     = (screenH - panelH) / 2 + static_cast<int>(20 * scale);

    panelRect = {panelX, panelY, panelW, panelH};

    const int inputW = panelW - panelPad * 2;
    const int inputX = panelX + panelPad;
    int cursorY      = panelY + titleSpace;

    usernameRect = {inputX, cursorY, inputW, inputH};
    cursorY += inputH + inputGapY;

    passwordRect = {inputX, cursorY, inputW, inputH};
    cursorY += inputH + btnGapY;

    const int btnW      = (inputW - btnGapX) / 2;
    loginButtonRect = {inputX,            cursorY, btnW, btnH};
    backButtonRect  = {inputX + btnW + btnGapX, cursorY, btnW, btnH};

    // Store scale for use in render()
    cachedScale    = scale;
    cachedBannerY  = bannerTopY;
    cachedBannerW  = static_cast<int>(Theme::BANNER_W * scale);
    cachedBannerH  = bannerH;
    cachedLabelOffY = labelOffY;
    cachedInputPad  = static_cast<int>(16 * scale);
}

void Login::handleEvents(Game& game, const SDL_Event& event) {
    updateLayout(game.getRenderer());

    if (event.type == SDL_QUIT) {
        game.setNextState(GameState::Quit);
        return;
    }
    if (event.type == SDL_MOUSEMOTION) {
        loginHover = RenderUtil::pointInRect(loginButtonRect, event.motion.x, event.motion.y);
        backHover  = RenderUtil::pointInRect(backButtonRect,  event.motion.x, event.motion.y);
    }
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        const int mx = event.button.x, my = event.button.y;
        if      (RenderUtil::pointInRect(usernameRect,    mx, my)) setActiveField(Field::Username);
        else if (RenderUtil::pointInRect(passwordRect,    mx, my)) setActiveField(Field::Password);
        else                                                        setActiveField(Field::None);
        if      (RenderUtil::pointInRect(loginButtonRect, mx, my)) loginPressed    = true;
        else if (RenderUtil::pointInRect(backButtonRect,  mx, my)) registerPressed = true;
    }
    if (event.type == SDL_TEXTINPUT) {
        statusMessage.clear();
        if (activeField == Field::Username && username.size() < kMaxUsernameLen)
            username.append(event.text.text);
        else if (activeField == Field::Password && password.size() < kMaxPasswordLen)
            password.append(event.text.text);
    }
    if (event.type == SDL_KEYDOWN) {
        switch (event.key.keysym.sym) {
            case SDLK_TAB:
                setActiveField(activeField == Field::Username ? Field::Password : Field::Username);
                break;
            case SDLK_BACKSPACE:
                if      (activeField == Field::Username && !username.empty()) username.pop_back();
                else if (activeField == Field::Password && !password.empty()) password.pop_back();
                break;
            case SDLK_RETURN: case SDLK_KP_ENTER:
                loginPressed = true; break;
            case SDLK_ESCAPE:
                registerPressed = true; break;
            default: break;
        }
    }
}

void Login::update(Game& game) {
    if (loginPressed) {
        loginPressed = false;
        statusMessage.clear();

        if (username.empty()) {
            statusMessage = "Email is required.";
            return;
        }

        const std::size_t atPos = username.find('@');
        if (atPos == std::string::npos || atPos == 0 || atPos + 1 >= username.size()) {
            statusMessage = "Enter a valid email address.";
            return;
        }
        const std::size_t dotPos = username.find('.', atPos + 1);
        if (dotPos == std::string::npos || dotPos + 1 >= username.size()) {
            statusMessage = "Enter a valid email address.";
            return;
        }

        if (password.empty()) {
            statusMessage = "Password is required.";
            return;
        }

        int userId = -1;
        std::string sessionId;
        std::string error;
        if (!authenticate(username, password, userId, sessionId, error)) {
            std::cerr << "Login failed: " << error << '\n';
            statusMessage = error;
            return;
        }
        game.setPlayerId(userId);
        game.setAuthSessionId(sessionId);
        std::cout << game.getPlayer().id << "\n";
        game.setPlayerUsername(username);
        game.setNextState(GameState::Loading);
        return;
    }
    if (registerPressed) {
        registerPressed = false;
        game.setNextState(GameState::Register);
    }
}

void Login::render(Game& game) {
    SDL_Renderer*              r          = game.getRenderer();
    const RenderText::FontSet& titleFonts = game.getTitleFonts();
    const RenderText::FontSet& uiFonts    = game.getUIFonts();
    updateLayout(r);

    int screenW, screenH;
    SDL_GetRendererOutputSize(r, &screenW, &screenH);

    // ── background + vignette ────────────────────────────────────────
    RenderBackdrop::drawBackgroundWithVignette(
        r, screenW, screenH,
        Theme::BG,
        SDL_Color{0, 0, 0, 255},
        80, 1.5f, 120
    );

    // ── banner ───────────────────────────────────────────────────────
    // SDL_Rect bannerRect = {
    //     screenW / 2 - cachedBannerW / 2,
    //     cachedBannerY,
    //     cachedBannerW,
    //     cachedBannerH
    // };
    // RenderBanner::drawBanner(r, bannerRect, "Archcast",
    //                          titleFonts.large,
    //                          Theme::BANNER_FILL,  Theme::BANNER_BORDER,
    //                          Theme::BANNER_TEXT,  Theme::BANNER_GLOW);

    // ── panel ────────────────────────────────────────────────────────
    RenderUtil::drawRoundedShadow(r, panelRect, Theme::PANEL_RADIUS,
                                  Theme::Effects::SHADOW_OFFSET,
                                  Theme::Effects::SHADOW_COLOR);
    RenderUtil::drawRoundedRect(r, panelRect, Theme::PANEL_RADIUS,
                                Theme::PANEL_FILL, Theme::PANEL_BORDER);

    // ── panel title ──────────────────────────────────────────────────
    if (titleFonts.medium) {
        RenderText::drawText(r, "Login", titleFonts.medium,
                             Theme::BANNER_TEXT,
                             panelRect.x + static_cast<int>(30 * cachedScale),
                             panelRect.y + static_cast<int>(22 * cachedScale));
    }

    // ── field labels ─────────────────────────────────────────────────
    if (uiFonts.large) {
        RenderText::drawText(r, "Username", uiFonts.large,
                             Theme::TEXT_MUTED,
                             usernameRect.x, usernameRect.y - cachedLabelOffY);
        RenderText::drawText(r, "Password", uiFonts.large,
                             Theme::TEXT_MUTED,
                             passwordRect.x, passwordRect.y - cachedLabelOffY);
    }

    // ── input fields ─────────────────────────────────────────────────
    const bool userActive = activeField == Field::Username;
    const bool passActive = activeField == Field::Password;

    RenderUtil::drawRoundedRect(r, usernameRect, Theme::INPUT_RADIUS,
                                userActive ? Theme::INPUT_ACTIVE : Theme::INPUT_FILL,
                                userActive ? Theme::INPUT_BORDER_ACTIVE : Theme::INPUT_BORDER_IDLE);
    if (userActive) {
        RenderUtil::drawRectGlowBorder(r, usernameRect, Theme::INPUT_BORDER_ACTIVE,
                                       Theme::Effects::INPUT_GLOW_LAYERS,
                                       Theme::Effects::INPUT_GLOW_BASE_ALPHA,
                                       Theme::Effects::INPUT_GLOW_ALPHA_STEP);
    }

    RenderUtil::drawRoundedRect(r, passwordRect, Theme::INPUT_RADIUS,
                                passActive ? Theme::INPUT_ACTIVE : Theme::INPUT_FILL,
                                passActive ? Theme::INPUT_BORDER_ACTIVE : Theme::INPUT_BORDER_IDLE);
    if (passActive) {
        RenderUtil::drawRectGlowBorder(r, passwordRect, Theme::INPUT_BORDER_ACTIVE,
                                       Theme::Effects::INPUT_GLOW_LAYERS,
                                       Theme::Effects::INPUT_GLOW_BASE_ALPHA,
                                       Theme::Effects::INPUT_GLOW_ALPHA_STEP);
    }

    // ── validation message ───────────────────────────────────────────
    if (uiFonts.small && !statusMessage.empty()) {
        RenderText::drawText(r, statusMessage, uiFonts.small,
                             Theme::ERROR_RED,
                             passwordRect.x,
                             passwordRect.y + passwordRect.h + static_cast<int>(8 * cachedScale));
    }

    // ── input text ───────────────────────────────────────────────────
    if (uiFonts.large) {
        const int ty = usernameRect.y + (usernameRect.h - 20) / 2;
        RenderText::drawText(r, username, uiFonts.large,
                             Theme::TEXT_PRIMARY,
                             usernameRect.x + cachedInputPad, ty);
        std::string mask(password.size(), '*');
        RenderText::drawText(r, mask, uiFonts.large,
                             Theme::TEXT_PRIMARY,
                             passwordRect.x + cachedInputPad,
                             passwordRect.y + (passwordRect.h - 20) / 2);
    }

    // ── buttons ──────────────────────────────────────────────────────
    RenderButton::drawButton(r, loginButtonRect, "Login", uiFonts.large,
                             Theme::BTN_PRIMARY, Theme::BTN_BORDER,
                             Theme::BTN_TEXT, loginHover, false);

    RenderButton::drawButton(r, backButtonRect, "Register", uiFonts.large,
                             Theme::BTN_SECONDARY, Theme::BTN_BORDER,
                             Theme::BTN_TEXT, backHover, false);

                    
}
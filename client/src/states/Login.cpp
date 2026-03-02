#include "states/Login.hpp"
#include "core/Game.hpp"
#include "core/NetworkClient.hpp"
#include "utils/HttpUtil.hpp"
#include "utils/JsonUtil.hpp"
#include "utils/EnvUtil.hpp"
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

    std::string escapeJson(const std::string& s) {
        std::string out;
        for (char c : s) {
            switch (c) {
                case '\\': out += "\\\\"; break;
                case '"':  out += "\\\""; break;
                case '\n': out += "\\n";  break;
                case '\r': out += "\\r";  break;
                case '\t': out += "\\t";  break;
                default:   out += c;
            }
        }
        return out;
    }

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
                      int& outId, std::string& error) {
        if (email.empty() || password.empty()) {
            error = "email and password required";
            return false;
        }
        const std::string host = EnvUtil::getAuthServiceHost();
        const int         port = EnvUtil::getAuthServicePort();

        std::ostringstream payload;
        payload << "{\"email\":\"" << escapeJson(email)
                << "\",\"password\":\"" << escapeJson(password) << "\"}";

        int statusCode = -1;
        std::string responseBody;
        if (!HttpUtil::sendHttp(host, port, "POST", "/login", payload.str(), statusCode, responseBody)) {
            error = "auth service unreachable";
            return false;
        }
        if (statusCode != 200) {
            if (!JsonUtil::readJsonStringField(responseBody, "error", error))
                error = "login failed";
            return false;
        }
        return parseUserId(responseBody, outId, error);
    }
}

// ── render helpers (file-local) ───────────────────────────────────────────────

namespace {
    void fillCircleR(SDL_Renderer* r, int cx, int cy, int rad) {
        for (int dy = -rad; dy <= rad; dy++) {
            int dx = (int)sqrt((double)(rad*rad - dy*dy));
            SDL_RenderDrawLine(r, cx-dx, cy+dy, cx+dx, cy+dy);
        }
    }

    void drawRoundedRect(SDL_Renderer* r, const SDL_Rect& rect,
                          int rad, SDL_Color fill, SDL_Color border) {
        SDL_SetRenderDrawColor(r, fill.r, fill.g, fill.b, fill.a);
        SDL_Rect body  = {rect.x + rad,          rect.y,       rect.w - 2*rad, rect.h        };
        SDL_Rect left  = {rect.x,                 rect.y + rad, rad,            rect.h - 2*rad};
        SDL_Rect right = {rect.x + rect.w - rad,  rect.y + rad, rad,            rect.h - 2*rad};
        SDL_RenderFillRect(r, &body);
        SDL_RenderFillRect(r, &left);
        SDL_RenderFillRect(r, &right);
        fillCircleR(r, rect.x + rad,          rect.y + rad,          rad);
        fillCircleR(r, rect.x + rect.w - rad, rect.y + rad,          rad);
        fillCircleR(r, rect.x + rad,          rect.y + rect.h - rad, rad);
        fillCircleR(r, rect.x + rect.w - rad, rect.y + rect.h - rad, rad);

        SDL_SetRenderDrawColor(r, border.r, border.g, border.b, border.a);
        SDL_RenderDrawLine(r, rect.x + rad,       rect.y,            rect.x + rect.w - rad, rect.y            );
        SDL_RenderDrawLine(r, rect.x + rad,       rect.y + rect.h,   rect.x + rect.w - rad, rect.y + rect.h   );
        SDL_RenderDrawLine(r, rect.x,             rect.y + rad,      rect.x,                rect.y + rect.h - rad);
        SDL_RenderDrawLine(r, rect.x + rect.w,    rect.y + rad,      rect.x + rect.w,       rect.y + rect.h - rad);
    }

    void drawGlowBorder(SDL_Renderer* r, const SDL_Rect& rect, SDL_Color glow) {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        for (int i = 5; i >= 1; i--) {
            Uint8 alpha = (Uint8)(20 + (5 - i) * 18);
            SDL_SetRenderDrawColor(r, glow.r, glow.g, glow.b, alpha);
            SDL_Rect g = {rect.x - i, rect.y - i, rect.w + i*2, rect.h + i*2};
            SDL_RenderDrawRect(r, &g);
        }
    }

    void drawShadow(SDL_Renderer* r, const SDL_Rect& rect, int rad) {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 0, 0, 0, 90);
        SDL_Rect s = {rect.x + 5, rect.y + 5, rect.w, rect.h};
        SDL_Rect sb = {s.x + rad,        s.y,     s.w - 2*rad, s.h        };
        SDL_Rect sl = {s.x,              s.y+rad, rad,         s.h - 2*rad};
        SDL_Rect sr = {s.x + s.w - rad,  s.y+rad, rad,         s.h - 2*rad};
        SDL_RenderFillRect(r, &sb);
        SDL_RenderFillRect(r, &sl);
        SDL_RenderFillRect(r, &sr);
        fillCircleR(r, s.x + rad,       s.y + rad,       rad);
        fillCircleR(r, s.x + s.w - rad, s.y + rad,       rad);
        fillCircleR(r, s.x + rad,       s.y + s.h - rad, rad);
        fillCircleR(r, s.x + s.w - rad, s.y + s.h - rad, rad);
    }

    void drawCentered(SDL_Renderer* r, TTF_Font* font,
                       const std::string& text, const SDL_Rect& rect, SDL_Color color) {
        if (!font) return;
        SDL_Surface* s = TTF_RenderUTF8_Blended(font, text.c_str(), color);
        if (!s) return;
        SDL_Texture* t = SDL_CreateTextureFromSurface(r, s);
        if (t) {
            SDL_Rect dst = {rect.x + (rect.w - s->w)/2,
                            rect.y + (rect.h - s->h)/2,
                            s->w, s->h};
            SDL_RenderCopy(r, t, nullptr, &dst);
            SDL_DestroyTexture(t);
        }
        SDL_FreeSurface(s);
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

bool Login::pointInRect(const SDL_Rect& rect, int x, int y) const {
    return x >= rect.x && x <= rect.x + rect.w &&
           y >= rect.y && y <= rect.y + rect.h;
}

void Login::setActiveField(Field field) {
    activeField = field;
    activeField == Field::None ? SDL_StopTextInput() : SDL_StartTextInput();
}

void Login::updateLayout(SDL_Renderer* renderer) {
    int screenW = 800, screenH = 600;
    if (renderer) SDL_GetRendererOutputSize(renderer, &screenW, &screenH);

    const int panelW  = std::min(520, (int)(screenW * 0.75f));
    const int panelH  = 420;
    const int panelX  = (screenW - panelW) / 2;
    const int panelY  = (screenH - panelH) / 2 + 20;  // pushed down for banner
    panelRect = {panelX, panelY, panelW, panelH};

    const int inputW  = panelW - 80;
    const int inputH  = 52;                            // taller inputs
    const int inputX  = panelX + 40;
    int       cursorY = panelY + 120;                   // more space for title

    usernameRect = {inputX, cursorY, inputW, inputH};
    cursorY += inputH + 40;                            // generous spacing

    passwordRect = {inputX, cursorY, inputW, inputH};
    cursorY += inputH + 36;

    const int btnW = (inputW - 16) / 2;
    const int btnH = 52;
    loginButtonRect = {inputX,          cursorY, btnW, btnH};
    backButtonRect  = {inputX + btnW + 16, cursorY, btnW, btnH};
}

void Login::handleEvents(Game& game, const SDL_Event& event) {
    updateLayout(game.getRenderer());

    if (event.type == SDL_QUIT) {
        game.setNextState(GameState::Quit);
        return;
    }
    if (event.type == SDL_MOUSEMOTION) {
        loginHover = pointInRect(loginButtonRect, event.motion.x, event.motion.y);
        backHover  = pointInRect(backButtonRect,  event.motion.x, event.motion.y);
    }
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        const int mx = event.button.x, my = event.button.y;
        if      (pointInRect(usernameRect,    mx, my)) setActiveField(Field::Username);
        else if (pointInRect(passwordRect,    mx, my)) setActiveField(Field::Password);
        else                                           setActiveField(Field::None);
        if      (pointInRect(loginButtonRect, mx, my)) loginPressed    = true;
        else if (pointInRect(backButtonRect,  mx, my)) registerPressed = true;
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

        // input validation checks
        if (username.empty()) {
            statusMessage = "Email is required.";
            return;
        }

        // basic email check — must have @ and a dot after it
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

        // authentication
        int userId = -1;
        std::string error;
        if (!authenticate(username, password, userId, error)) {
            std::cerr << "Login failed: " << error << '\n';
            statusMessage = error;
            return;
        }
        game.setPlayerId(userId);
        std::cout << game.getPlayer().id << "\n";
        game.setPlayerUsername(username);
        if (!game.refreshPlayerDeckFromService())
            std::cerr << "Failed to refresh deck after login\n";
// game.getPlayer().deck.toString();
        game.setNextState(GameState::Title);
        return;
    }
    if (registerPressed) {
        registerPressed = false;
        game.setNextState(GameState::Register);
    }
}

void Login::render(const Game& game) {
    SDL_Renderer*              r          = game.getRenderer();
    const RenderText::FontSet& titleFonts = game.getTitleFonts();
    const RenderText::FontSet& uiFonts    = game.getUIFonts();
    updateLayout(r);

    int screenW, screenH;
    SDL_GetRendererOutputSize(r, &screenW, &screenH);

    // ── background + vignette ────────────────────────────────────────
    SDL_SetRenderDrawColor(r, Theme::BG.r, Theme::BG.g, Theme::BG.b, 255);
    SDL_RenderClear(r);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    for (int i = 0; i < 80; i++) {
        SDL_SetRenderDrawColor(r, 0, 0, 0, (Uint8)(120 - i * 1.5f));
        SDL_Rect edge = {i, i, screenW - 2*i, screenH - 2*i};
        SDL_RenderDrawRect(r, &edge);
    }

    // ── banner ───────────────────────────────────────────────────────
    SDL_Rect bannerRect = {screenW/2 - Theme::BANNER_W/2, 30,
                           Theme::BANNER_W, Theme::BANNER_H};
    RenderBanner::drawBanner(r, bannerRect, "Speed Card Game",
                              titleFonts.large,
                              Theme::BANNER_FILL,  Theme::BANNER_BORDER,
                              Theme::BANNER_TEXT,  Theme::BANNER_GLOW);

    // ── panel ────────────────────────────────────────────────────────
    drawShadow(r, panelRect, Theme::PANEL_RADIUS);
    drawRoundedRect(r, panelRect, Theme::PANEL_RADIUS,
                    Theme::PANEL_FILL, Theme::PANEL_BORDER);

    // ── panel title ──────────────────────────────────────────────────
    if (titleFonts.medium) {
        RenderText::drawText(r, "Login", titleFonts.medium,
                             Theme::BANNER_TEXT,
                             panelRect.x + 30, panelRect.y + 22);
    }

    // ── field labels ─────────────────────────────────────────────────
    if (uiFonts.large) {
        RenderText::drawText(r, "Username", uiFonts.large,
                             Theme::TEXT_MUTED,
                             usernameRect.x, usernameRect.y - 30);
        RenderText::drawText(r, "Password", uiFonts.large,
                             Theme::TEXT_MUTED,
                             passwordRect.x, passwordRect.y - 30);
    }

    // ── input fields ─────────────────────────────────────────────────
    const bool userActive = activeField == Field::Username;
    const bool passActive = activeField == Field::Password;

    drawRoundedRect(r, usernameRect, Theme::INPUT_RADIUS,
                    userActive ? Theme::INPUT_ACTIVE : Theme::INPUT_FILL,
                    userActive ? Theme::INPUT_BORDER_ACTIVE : Theme::INPUT_BORDER_IDLE);
    if (userActive) drawGlowBorder(r, usernameRect, Theme::INPUT_BORDER_ACTIVE);

    drawRoundedRect(r, passwordRect, Theme::INPUT_RADIUS,
                    passActive ? Theme::INPUT_ACTIVE : Theme::INPUT_FILL,
                    passActive ? Theme::INPUT_BORDER_ACTIVE : Theme::INPUT_BORDER_IDLE);
    if (passActive) drawGlowBorder(r, passwordRect, Theme::INPUT_BORDER_ACTIVE);

    // ── validation message ───────────────────────────────────────────
    if (uiFonts.small && !statusMessage.empty()) {
        RenderText::drawText(r, statusMessage, uiFonts.small,
                            Theme::ERROR_RED,
                            passwordRect.x,
                            passwordRect.y + passwordRect.h + 8);
    }

    // ── input text ───────────────────────────────────────────────────
    if (uiFonts.large) {
        const int pad = 16;
        const int ty  = usernameRect.y + (usernameRect.h - 20) / 2;
        RenderText::drawText(r, username, uiFonts.large,
                             Theme::TEXT_PRIMARY, usernameRect.x + pad, ty);
        std::string mask(password.size(), '*');
        RenderText::drawText(r, mask, uiFonts.large,
                             Theme::TEXT_PRIMARY,
                             passwordRect.x + pad,
                             passwordRect.y + (passwordRect.h - 20) / 2);
    }

    // ── buttons ──────────────────────────────────────────────────────
    auto brighten = [](SDL_Color c, int amt) -> SDL_Color {
        return {(Uint8)std::min(c.r + amt, 255),
                (Uint8)std::min(c.g + amt, 255),
                (Uint8)std::min(c.b + amt, 255), c.a};
    };

    SDL_Color loginFill    = loginHover ? brighten(Theme::BTN_PRIMARY,   40) : Theme::BTN_PRIMARY;
    SDL_Color registerFill = backHover  ? brighten(Theme::BTN_SECONDARY, 40) : Theme::BTN_SECONDARY;

    drawShadow(r, loginButtonRect,  Theme::BTN_RADIUS);
    drawRoundedRect(r, loginButtonRect,  Theme::BTN_RADIUS, loginFill,    Theme::BTN_BORDER);
    drawCentered(r, uiFonts.large, "Login",    loginButtonRect,  Theme::BTN_TEXT);

    drawShadow(r, backButtonRect,   Theme::BTN_RADIUS);
    drawRoundedRect(r, backButtonRect,   Theme::BTN_RADIUS, registerFill, Theme::BTN_BORDER);
    drawCentered(r, uiFonts.large, "Register", backButtonRect,   Theme::BTN_TEXT);
}
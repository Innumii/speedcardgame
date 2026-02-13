#include "states/Login.hpp"

#include "core/Game.hpp"
#include "render/RenderButton.hpp"
#include "render/RenderText.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <algorithm>
#include <iostream>

namespace {
    constexpr std::size_t kMaxUsernameLen = 24;
    constexpr std::size_t kMaxPasswordLen = 32;
}

Login::Login() = default;

Login::~Login() {
    RenderText::closeFonts(fonts);
}

void Login::enter(Game& game) {
    ensureFonts();
    setActiveField(Field::Username);
    SDL_StartTextInput();
}

void Login::exit(Game& game) {
    SDL_StopTextInput();
    loginPressed = false;
    backPressed = false;
    loginHover = false;
    backHover = false;
}

void Login::ensureFonts() {
    if (fontsReady) return;
    if (!RenderText::ensureTtfReady()) return;

    fonts = RenderText::loadFonts("assets/font.TTF", 16, 12, 28);
    if (!fonts.small || !fonts.large) {
        RenderText::closeFonts(fonts);
        fontsReady = false;
        return;
    }

    fontsReady = true;
}

bool Login::pointInRect(const SDL_Rect& rect, int x, int y) const {
    return x >= rect.x && x <= rect.x + rect.w && y >= rect.y && y <= rect.y + rect.h;
}

void Login::setActiveField(Field field) {
    activeField = field;
    if (activeField == Field::None) {
        SDL_StopTextInput();
    } else {
        SDL_StartTextInput();
    }
}

void Login::updateLayout(SDL_Renderer* renderer) {
    int screenW = 800;
    int screenH = 600;
    if (renderer) {
        SDL_GetRendererOutputSize(renderer, &screenW, &screenH);
    }

    const int panelW = std::min(560, static_cast<int>(screenW * 0.8f));
    const int panelH = 360;
    panelRect = SDL_Rect{
        (screenW - panelW) / 2,
        (screenH - panelH) / 2,
        panelW,
        panelH
    };

    const int inputW = panelW - 120;
    const int inputH = 44;
    const int inputX = panelRect.x + 60;
    int cursorY = panelRect.y + 80;

    usernameRect = SDL_Rect{inputX, cursorY, inputW, inputH};
    cursorY += inputH + 24;

    passwordRect = SDL_Rect{inputX, cursorY, inputW, inputH};
    cursorY += inputH + 32;

    const int buttonW = (inputW - 20) / 2;
    const int buttonH = 46;
    loginButtonRect = SDL_Rect{inputX, cursorY, buttonW, buttonH};
    backButtonRect = SDL_Rect{inputX + buttonW + 20, cursorY, buttonW, buttonH};
}

void Login::handleEvents(Game& game, const SDL_Event& event) {
    updateLayout(game.getRenderer());

    if (event.type == SDL_QUIT) {
        game.setNextState(GameState::Quit);
        return;
    }

    if (event.type == SDL_MOUSEMOTION) {
        const int mouseX = event.motion.x;
        const int mouseY = event.motion.y;
        loginHover = pointInRect(loginButtonRect, mouseX, mouseY);
        backHover = pointInRect(backButtonRect, mouseX, mouseY);
    }

    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        const int mouseX = event.button.x;
        const int mouseY = event.button.y;

        if (pointInRect(usernameRect, mouseX, mouseY)) {
            setActiveField(Field::Username);
        } else if (pointInRect(passwordRect, mouseX, mouseY)) {
            setActiveField(Field::Password);
        } else {
            setActiveField(Field::None);
        }

        if (pointInRect(loginButtonRect, mouseX, mouseY)) {
            loginPressed = true;
        } else if (pointInRect(backButtonRect, mouseX, mouseY)) {
            backPressed = true;
        }
    }

    if (event.type == SDL_TEXTINPUT) {
        if (activeField == Field::Username && username.size() < kMaxUsernameLen) {
            username.append(event.text.text);
        } else if (activeField == Field::Password && password.size() < kMaxPasswordLen) {
            password.append(event.text.text);
        }
    }

    if (event.type == SDL_KEYDOWN) {
        if (event.key.keysym.sym == SDLK_TAB) {
            if (activeField == Field::Username) {
                setActiveField(Field::Password);
            } else {
                setActiveField(Field::Username);
            }
        } else if (event.key.keysym.sym == SDLK_BACKSPACE) {
            if (activeField == Field::Username && !username.empty()) {
                username.pop_back();
            } else if (activeField == Field::Password && !password.empty()) {
                password.pop_back();
            }
        } else if (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER) {
            loginPressed = true;
        } else if (event.key.keysym.sym == SDLK_ESCAPE) {
            backPressed = true;
        }
    }
}

void Login::update(Game& game) {
    if (loginPressed) {
        std::cout << "Login requested for user: " << username << '\n';
        loginPressed = false;
        game.setNextState(GameState::Title);
        return;
    }

    if (backPressed) {
        backPressed = false;
        game.setNextState(GameState::Title);
    }
}

void Login::render(Game& game) {
    SDL_Renderer* renderer = game.getRenderer();
    updateLayout(renderer);
    ensureFonts();

    SDL_SetRenderDrawColor(renderer, 18, 18, 20, 255);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawColor(renderer, 40, 45, 60, 230);
    SDL_RenderFillRect(renderer, &panelRect);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &panelRect);

    SDL_Color white{245, 245, 245, 255};
    SDL_Color muted{190, 190, 190, 255};
    SDL_Color fieldFill{30, 30, 34, 255};
    SDL_Color fieldActive{45, 45, 55, 255};

    if (fonts.large) {
        RenderText::drawText(renderer, "Login", fonts.large, white, panelRect.x + 30, panelRect.y + 20);
    }

    if (fonts.small) {
        RenderText::drawText(renderer, "Username", fonts.small, muted, usernameRect.x, usernameRect.y - 22);
        RenderText::drawText(renderer, "Password", fonts.small, muted, passwordRect.x, passwordRect.y - 22);
    }

    SDL_Color userFill = activeField == Field::Username ? fieldActive : fieldFill;
    SDL_Color passFill = activeField == Field::Password ? fieldActive : fieldFill;

    SDL_SetRenderDrawColor(renderer, userFill.r, userFill.g, userFill.b, userFill.a);
    SDL_RenderFillRect(renderer, &usernameRect);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &usernameRect);

    SDL_SetRenderDrawColor(renderer, passFill.r, passFill.g, passFill.b, passFill.a);
    SDL_RenderFillRect(renderer, &passwordRect);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &passwordRect);

    if (fonts.small) {
        const int textPad = 10;
        RenderText::drawText(renderer, username, fonts.small, white, usernameRect.x + textPad, usernameRect.y + 12);
        std::string mask(password.size(), '*');
        RenderText::drawText(renderer, mask, fonts.small, white, passwordRect.x + textPad, passwordRect.y + 12);
    }

    SDL_Color baseButton{70, 120, 200, 255};
    SDL_Color highlightButton{90, 150, 220, 255};
    SDL_Color pressedButton{60, 100, 180, 255};
    SDL_Color backBase{70, 70, 70, 255};
    SDL_Color backHighlight{90, 90, 90, 255};
    SDL_Color backPressed{60, 60, 60, 255};

    RenderButton::drawButton(
        renderer,
        loginButtonRect,
        "Login",
        loginHover,
        false,
        baseButton,
        highlightButton,
        pressedButton,
        white,
        fonts.small
    );

    RenderButton::drawButton(
        renderer,
        backButtonRect,
        "Back",
        backHover,
        false,
        backBase,
        backHighlight,
        backPressed,
        white,
        fonts.small
    );
}

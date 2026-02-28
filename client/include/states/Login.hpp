#ifndef LOGIN_HPP
#define LOGIN_HPP

#include <SDL2/SDL.h>
#include <string>

#include "render/RenderText.hpp"

class Game;

class Login {
public:
    Login();
    ~Login();

    void enter(Game& game);
    void exit(Game& game);
    void handleEvents(Game& game, const SDL_Event& event);
    void update(Game& game);
    void render(const Game& game);

private:
    enum class Field {
        None,
        Username,
        Password
    };

    void updateLayout(SDL_Renderer* renderer);
    void ensureFonts();
    void setActiveField(Field field);
    bool pointInRect(const SDL_Rect& rect, int x, int y) const;

    SDL_Rect panelRect{0, 0, 0, 0};
    SDL_Rect usernameRect{0, 0, 0, 0};
    SDL_Rect passwordRect{0, 0, 0, 0};
    SDL_Rect loginButtonRect{0, 0, 0, 0};
    SDL_Rect backButtonRect{0, 0, 0, 0};

    Field activeField{Field::Username};
    std::string username;
    std::string password;
    std::string statusMessage; // add auth status msg

    bool loginPressed{false};
    bool registerPressed{false};
    bool loginHover{false};
    bool backHover{false};

    bool fontsReady{false};
    RenderText::FontSet fonts{};
};

#endif

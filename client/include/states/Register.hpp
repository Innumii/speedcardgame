#ifndef REGISTER_HPP
#define REGISTER_HPP

#include <SDL2/SDL.h>
#include <string>

#include "render/RenderText.hpp"

class Game;

class Register {
public:
    Register();
    ~Register();

    void enter(Game& game);
    void exit(Game& game);
    void handleEvents(Game& game, const SDL_Event& event);
    void update(Game& game);
    void render(const Game& game);

private:
    enum class Field {
        None,
        Email,
        Password,
        ConfirmPassword
    };

    void updateLayout(SDL_Renderer* renderer);
    void setActiveField(Field field);

    SDL_Rect panelRect{0, 0, 0, 0};
    SDL_Rect emailRect{0, 0, 0, 0};
    SDL_Rect passwordRect{0, 0, 0, 0};
    SDL_Rect confirmRect{0, 0, 0, 0};
    SDL_Rect createButtonRect{0, 0, 0, 0};
    SDL_Rect backButtonRect{0, 0, 0, 0};

    Field activeField{Field::Email};
    std::string email;
    std::string password;
    std::string confirmPassword;
    std::string statusMessage;

    bool createPressed{false};
    bool backPressed{false};
    bool createHover{false};
    bool backHover{false};

};

#endif

#ifndef REGISTER_HPP
#define REGISTER_HPP

#include <SDL2/SDL.h>
#include <string>

#include "StateInterface.hpp"
#include "render/RenderText.hpp"

class Game;

class Register : public StateInterface {
public:
    Register();
    ~Register();

    void enter(Game& game) override;
    void exit(Game& game) override;
    void handleEvents(Game& game, const SDL_Event& event) override;
    void update(Game& game) override;
    void render(Game& game) override;

private:
    enum class Field {
        None,
        Username,
        Email,
        Password,
        ConfirmPassword
    };

    void updateLayout(SDL_Renderer* renderer);
    void setActiveField(Field field);

    SDL_Rect panelRect{0, 0, 0, 0};
    SDL_Rect usernameRect{0, 0, 0, 0};
    SDL_Rect emailRect{0, 0, 0, 0};
    SDL_Rect passwordRect{0, 0, 0, 0};
    SDL_Rect confirmRect{0, 0, 0, 0};
    SDL_Rect createButtonRect{0, 0, 0, 0};
    SDL_Rect backButtonRect{0, 0, 0, 0};

    Field activeField{Field::Email};
    std::string username;
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

#include <SDL2/SDL.h>
#include <iostream>
#include <optional>
#include <string>
#include "core/Game.hpp"

namespace {
    std::optional<int> parseDrawInterval(int argc, char** argv) {
        if (argc < 2) return std::nullopt;
        try {
            return std::stoi(argv[1]);
        } catch (...) {
            return std::nullopt;
        }
    }

    bool parseFullscreenFlag(int argc, char** argv) {
        if (argc < 3) return false;
        std::string flag = argv[2];
        return flag == "--fullscreen" || flag == "-f";
    }
}

int main(int argc, char** argv) {
    try {
        const int defaultDrawIntervalSeconds = 3;
        const int drawInterval = parseDrawInterval(argc, argv).value_or(defaultDrawIntervalSeconds);
        const bool fullscreen = parseFullscreenFlag(argc, argv);

        std::cout << "Beginning Game (draw every " << drawInterval << "s)" << std::endl;

        Game game("SpeedCardGame", 100, 100, 800, 600, fullscreen, drawInterval);
        while (game.running()) {
            game.handleEvents();
            game.update();
            game.render();
        }
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    return 0;
}
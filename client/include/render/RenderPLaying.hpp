#ifndef RENDERPLAYING_HPP
#define RENDERPLAYING_HPP

class Game;
class Playing;

// Orchestrates rendering for the Playing state (logic lives in Playing)
class RenderPlaying {
public:
	static void render(Playing& playing, const Game& game);
};

#endif

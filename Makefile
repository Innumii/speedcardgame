all:
	g++ -I include -L lib -o main src/main.cpp src/core/Game.cpp -lmingw32 -lSDL2main -lSDL2
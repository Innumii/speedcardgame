all:
	g++ -I include -L lib -o main \
		src/main.cpp \
		src/core/Game.cpp \
		src/states/Title.cpp \
		src/objects/Card.cpp \
		src/objects/CreatureCard.cpp \
		src/objects/SpellCard.cpp \
		src/objects/Deck.cpp \
		src/objects/Player.cpp \
		-lmingw32 -lSDL2main -lSDL2
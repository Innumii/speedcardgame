CXX := g++
STD := -std=c++17 -D_WIN32_WINNT=0x0A00
INCLUDES := -I include
LIBDIR := -L lib
LIBS := -lmingw32 -lSDL2main -lSDL2 -lSDL2_ttf -lSDL2_image -lWs2_32 -lssl -lcrypto -lcrypt32 -lSDL2_mixer
GUI_LDFLAGS := -mwindows
BUILD_DIR := build
MKDIR_P := powershell -NoProfile -Command "New-Item -ItemType Directory -Force -Path"

# Pick up .cpp files in src and one/two-level subdirs.
SRCS := $(wildcard src/*.cpp src/*/*.cpp src/*/*/*.cpp)
OBJS := $(patsubst src/%.cpp,$(BUILD_DIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

all: main

main: $(OBJS)
	$(CXX) $(STD) -o $@ $^ $(LIBDIR) $(LIBS)

# Production Windows build (no attached console window).
release: MyGame.exe

MyGame.exe: $(OBJS)
	$(CXX) $(STD) $(GUI_LDFLAGS) -o $@ $^ $(LIBDIR) $(LIBS)

$(BUILD_DIR)/%.o: src/%.cpp
	$(MKDIR_P) $(@D)
	$(CXX) $(STD) $(INCLUDES) -MMD -MP -c $< -o $@

-include $(DEPS)

clean:
	powershell -NoProfile -Command "Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $(BUILD_DIR), main, MyGame.exe"

.PHONY: all clean release
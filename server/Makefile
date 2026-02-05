CXX := g++
INCLUDES := -I include
BUILD_DIR := build

SRCS := $(wildcard src/*.cpp src/*/*.cpp src/*/*/*.cpp)
OBJS := $(patsubst src/%.cpp,$(BUILD_DIR)/%.o,$(SRCS))

CXXFLAGS := -std=c++17 -Wall -Wextra -pthread

all: server

server: $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(BUILD_DIR)/%.o: src/%.cpp
	mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) server

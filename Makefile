CXX      = g++
CXXFLAGS = -Wall -Wextra -std=c++17 -pthread

# Uncomment the one LIBS line that matches your GUI choice:
LIBS = -lsfml-graphics -lsfml-window -lsfml-audio -lsfml-network -lsfml-system -lrt
# LIBS = $(shell sdl2-config --libs) -lrt
# LIBS = -lglfw -lGL -lrt
# LIBS = -lncurses -lrt

BIN = bin

TARGETS = $(BIN)/arbiter $(BIN)/hip $(BIN)/asp

all: clean $(BIN) $(TARGETS)
	@echo Build complete.

$(BIN):
	mkdir -p $(BIN)

debug: debug.c $(BIN)
	gcc -std=c11 -Wall -Wextra -pthread debug.c -o $(BIN)/debug -lrt

$(BIN)/arbiter: Arbiter/arbiter.cpp
	$(CXX) $(CXXFLAGS) Arbiter/*.cpp -o $(BIN)/arbiter $(LIBS)

$(BIN)/hip: Hip/hip.cpp
	$(CXX) $(CXXFLAGS) Hip/*.cpp -o $(BIN)/hip $(LIBS)

$(BIN)/asp: Asp/asp.cpp
	$(CXX) $(CXXFLAGS) Asp/*.cpp -o $(BIN)/asp $(LIBS)

inventory_test: $(BIN)/inventory_test

$(BIN)/inventory_test: tests/inventory_test.cpp
	$(CXX) $(CXXFLAGS) tests/inventory_test.cpp -o $(BIN)/inventory_test -lrt

clean:
	rm -f $(BIN)/arbiter $(BIN)/hip $(BIN)/asp $(BIN)/debug $(BIN)/inventory_test

.PHONY: all clean debug inventory_test

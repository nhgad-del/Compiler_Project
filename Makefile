# Makefile for the Interactive Compiler Explorer
# Usage:  make          build the program ("explorer")
#         make serve    build and start the web interface
#         make test     run the automated tests
#         make clean    delete build files

CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -pthread
SRC      = $(wildcard src/*.cpp)
OBJ      = $(patsubst src/%.cpp,build/%.o,$(SRC))
HDR      = $(wildcard src/*.h)
TARGET   = explorer

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ)

build/%.o: src/%.cpp $(HDR)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -c $< -o $@

serve: $(TARGET)
	./$(TARGET) --serve

test: $(TARGET)
	bash tests/run_tests.sh

clean:
	rm -rf build $(TARGET)

.PHONY: all serve test clean

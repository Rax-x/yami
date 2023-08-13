CXX = g++
CFLAGS = -g -Wall -Wextra -std=c++11

BIN = math
SOURCES = $(wildcard *.cc)

all: $(BIN)

$(BIN): $(SOURCES)
	@clear
	$(CXX) $(CFLAGS) $< -o $(BIN)

.PHONY: clean
clean:
	rm -rf $(BIN)
	

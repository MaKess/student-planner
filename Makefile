BIN=student-planner
SRC=main.cpp

.PHONY: all

all: $(BIN)

clean:
	$(RM) $(BIN)

run: $(BIN)
	./$< -i availability.json -a 7 -d 2 -o schedule.json

$(BIN): $(SRC)
	$(CXX) -Wall -Wextra -std=c++17 -O3 -mtune=native -ggdb3 -o $@ $^ -lfmt

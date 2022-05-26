BIN=student-planner
SRC=main.cpp

.PHONY: all

all: $(BIN)

clean:
	$(RM) $(BIN)

run: $(BIN)
	./$< availability.json 7 2 schedule.json

$(BIN): $(SRC)
	$(CXX) -Wall -Wextra -std=c++17 -O3 -mtune=native -ggdb3 -o $@ $^ -lfmt

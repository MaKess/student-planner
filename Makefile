BIN=student-planner
SRC=main.cpp

CXXFLAGS = -Wall -Wextra -std=c++17 -O3 -mtune=native
LDFLAGS = -lfmt

.PHONY: all opt

all: $(BIN)

clean:
	$(RM) $(BIN)

run: $(BIN)
	./$< -i availability.json -a 7 -d 2 -o schedule.json

$(BIN): $(SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

opt:
	$(CXX) $(CXXFLAGS) -o $(BIN) $(SRC) $(LDFLAGS) -fprofile-generate
	./$(BIN) -i availability.json -a 7 -d 2 -o schedule.json
	$(CXX) $(CXXFLAGS) -o $(BIN) $(SRC) $(LDFLAGS) -fprofile-use

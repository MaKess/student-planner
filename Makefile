BIN=student-planner
SRC=main.cpp
CXXFLAGS = -I fmt/include -Wall -Wextra -std=c++20 -O3 -mtune=native
LDFLAGS = -L fmt/build -lfmt

BIN = or
OR_PATH = /pools/datapool/home/martin/coding/or-tools_x86_64_Ubuntu-22.04_cpp_v9.10.4067

CXXFLAGS += \
	-I $(OR_PATH)/include \
	-DHAVE_CONFIG_H \
	-DOR_TOOLS_AS_DYNAMIC_LIB \
	-DUSE_BOP \
	-DUSE_CBC \
	-DUSE_CLP \
	-DUSE_GLOP \
	-DUSE_LP_PARSER \
	-DUSE_PDLP \
	-DUSE_SCIP
CXXFLAGS += -DUSE_OR
LDFLAGS += -L $(OR_PATH)/lib -Wl,-rpath,$(OR_PATH)/lib -lortools

.PHONY: all clean opt

all: $(BIN)

clean:
	$(RM) $(BIN) *.o *.d

run: $(BIN)
	./$< -i availability.json -a 7 -d 2 -o schedule.json

main.o: main.cpp
	$(CXX) $(CXXFLAGS) -c -MMD -MF main.d -o $@ $<

$(BIN): main.o
	$(CXX) -o $@ $^ $(LDFLAGS)

opt:
	$(CXX) $(CXXFLAGS) -o $(BIN) $(SRC) $(LDFLAGS) -fprofile-generate
	./$(BIN) -i availability.json -a 7 -d 2 -o schedule.json
	$(CXX) $(CXXFLAGS) -o $(BIN) $(SRC) $(LDFLAGS) -fprofile-use

-include *.d

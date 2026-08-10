# Aurora Player - dependency-free Makefile for the core library, the CLI and
# the tests. The Qt desktop UI is built with CMake (see README).
#
#   make            build lib + CLI + tests
#   make test       build and run the test suite
#   make run        build and print the CLI help
#   make clean
#
# Only a C++17 compiler and pthreads are required.

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -pipe -Wall -Wextra -Wno-unused-parameter
LDFLAGS  ?= -pthread
INCLUDES  = -Icore/include
BUILD     = build

CORE_SRC  = $(wildcard core/src/*.cpp)
CORE_OBJ  = $(patsubst core/src/%.cpp,$(BUILD)/core/%.o,$(CORE_SRC))
CLI_SRC   = $(wildcard cli/*.cpp)
CLI_OBJ   = $(patsubst cli/%.cpp,$(BUILD)/cli/%.o,$(CLI_SRC))
TEST_SRC  = $(wildcard tests/*.cpp)
TEST_OBJ  = $(patsubst tests/%.cpp,$(BUILD)/tests/%.o,$(TEST_SRC))

CLI_BIN   = $(BUILD)/aurora-cli
TEST_BIN  = $(BUILD)/aurora-tests
LIB       = $(BUILD)/libauroracore.a

.PHONY: all test run clean

all: $(CLI_BIN) $(TEST_BIN)

$(BUILD)/core/%.o: core/src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD)/cli/%.o: cli/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD)/tests/%.o: tests/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(LIB): $(CORE_OBJ)
	@mkdir -p $(dir $@)
	ar rcs $@ $^

$(CLI_BIN): $(CLI_OBJ) $(LIB)
	$(CXX) $(CLI_OBJ) $(LIB) $(LDFLAGS) -o $@

$(TEST_BIN): $(TEST_OBJ) $(LIB)
	$(CXX) $(TEST_OBJ) $(LIB) $(LDFLAGS) -o $@

test: $(TEST_BIN)
	$(TEST_BIN)

run: $(CLI_BIN)
	$(CLI_BIN) help

clean:
	rm -rf $(BUILD)

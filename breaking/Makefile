# Makefile for BMSSP Algorithm Implementation and Verification

CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall
LDFLAGS =

# Source files for verification
SOURCES = verify.cpp bmssp.cpp
OBJECTS = $(SOURCES:.cpp=.o)
TARGET = verify

# OJ submission target
OJ_SRC = oj.cpp
OJ_TARGET = oj

# Test parameters (can be overridden)
# 100 20000 100000000 100
NUM_TESTS ?= 100
MAX_N ?= 40000
MAX_WEIGHT ?= 1000000
EDGE_FACTOR ?= 100

# Default target
all: $(TARGET) $(OJ_TARGET)

# Link the executable
$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Build OJ submission
$(OJ_TARGET): $(OJ_SRC)
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

# Compile source files
%.o: %.cpp bmssp.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	rm -f $(OBJECTS) $(TARGET) $(OJ_TARGET)

# Run verification with customizable parameters
# Usage: make test [NUM_TESTS=xxx] [MAX_N=xxx] [MAX_WEIGHT=xxx] [EDGE_FACTOR=xxx]
test: $(TARGET)
	./$(TARGET) $(NUM_TESTS) $(MAX_N) $(MAX_WEIGHT) $(EDGE_FACTOR)

# Quick test (small scale, sparse)
quick: $(TARGET)
	./$(TARGET) 10 20 50 10

# Sparse graph test
sparse: $(TARGET)
	./$(TARGET) $(NUM_TESTS) $(MAX_N) $(MAX_WEIGHT) 10

# Dense graph test
dense: $(TARGET)
	./$(TARGET) $(NUM_TESTS) $(MAX_N) $(MAX_WEIGHT) 1000

# Debug build
debug: CXXFLAGS += -g -O0
debug: clean $(TARGET)

# Format all source files with clang-format
format:
	@clang-format -i $(wildcard *.cpp) $(wildcard *.h)
	@echo "Formatted all source files"

# Phony targets
.PHONY: all clean test quick sparse dense debug oj format

# Build OJ only
oj: $(OJ_TARGET)

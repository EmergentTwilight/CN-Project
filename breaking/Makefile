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

# Run verification with default parameters (100 tests, max n=50, max weight=100)
test: $(TARGET)
	./$(TARGET)

# Run quick verification (10 tests, max n=20, max weight=50)
test-quick: $(TARGET)
	./$(TARGET) 10 20 50

# Run comprehensive verification (1000 tests, max n=10000, max weight=200000)
test-full: $(TARGET)
	./$(TARGET) 1000 10000 200000

# Run with custom parameters
# Usage: make test-custom NUM_TESTS=xxx MAX_N=xxx MAX_WEIGHT=xxx
test-custom: $(TARGET)
	./$(TARGET) $(NUM_TESTS) $(MAX_N) $(MAX_WEIGHT)

# Debug build
debug: CXXFLAGS += -g -O0
debug: clean $(TARGET)

# Phony targets
.PHONY: all clean test test-quick test-full test-custom debug oj

# Build OJ only
oj: $(OJ_TARGET)

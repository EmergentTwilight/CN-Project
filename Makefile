# Makefile for NS-3 Algorithm Comparison
# Build and run routing algorithm experiments

.PHONY: all configure build run benchmark test clean

# NS-3 directory path
NS3_DIR = ns-allinone-3.46.1/ns-3.46.1

# Default target if no program specified
PROGRAM ?= benchmark

# Default target: configure, build and run benchmark
all: configure build benchmark

# ==============================================================================
# CORE COMMANDS
# ==============================================================================

# Configure: configure NS-3 with examples and tests enabled
# Usage: make configure
configure:
	@echo "Configuring NS-3 with examples and tests..."
	cd $(NS3_DIR) && ./ns3 configure --enable-examples --enable-tests
	@echo "✓ Configuration completed."

# Build: compile NS-3 with algorithm modifications
# Usage: make build
build:
	@echo "Building NS-3..."
	cd $(NS3_DIR) && ./ns3 build
	@echo "✓ Build completed."

# Run: execute program in scratch/ directory
# Usage:
#   make run                    - Run scratch/benchmark (default program)
#   make run PROGRAM=test       - Run scratch/test using PROGRAM variable
run:
	@echo "Running: scratch/$(PROGRAM)... ============================================================"
	cd $(NS3_DIR) && ./ns3 run $(PROGRAM)
	@echo "✓ Run completed. ============================================================"

# Benchmark: run the benchmark test
# Usage: make benchmark
benchmark:
	@echo "Running benchmark test..."
	cd $(NS3_DIR) && ./ns3 run benchmark
	@echo "✓ Benchmark completed."

# Test: alias for benchmark (for convenience)
# Usage: make test
test: benchmark

# Clean: clean build files
# Usage: make clean
clean:
	@echo "Cleaning NS-3 build files..."
	cd $(NS3_DIR) && ./ns3 clean
	@echo "✓ Clean completed."

# ==============================================================================
# NOTES
# ==============================================================================
# To add new tests:
# 1. Create your test file in: $(NS3_DIR)/scratch/
# 2. Follow the format of benchmark.cc
# 3. Run with: make run PROGRAM=your_test_name
#    (without the .cc extension)

# Makefile for NS-3 Algorithm Comparison
# Build and run routing algorithm experiments

.PHONY: all build run

# Default target if no program specified
PROGRAM ?= main

# Default target: build and run
all: build run

# ==============================================================================
# CORE COMMANDS
# ==============================================================================

# Build: compile NS-3 with algorithm modifications
build:
	@echo "Building NS-3..."
	cd ns-3.46.1 && ./ns3 build --quiet
	@echo "✓ Build completed."

# Run: execute program in scratch/ directory
# Usage:
#   make               - Build and run scratch/main
#   make run           - Run scratch/main (default program)
#   make run PROGRAM=test  - Run scratch/test using PROGRAM variable
run:
	@echo "Running: scratch/$(PROGRAM)... ============================================================"
	cd ns-3.46.1 && ./ns3 run "scratch/$(PROGRAM)" --no-build --quiet
	@echo "✓ Run completed. ============================================================"

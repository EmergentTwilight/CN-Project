# Makefile for ns-3 development
# This file handles the complete workflow: setup, copy, build, and run ns-3 projects

.PHONY: all setup clean copy build run help

# Default target: execute the complete workflow
all: clean copy build run

# ==============================================================================
# SETUP COMMANDS
# ==============================================================================

# Setup: create initial backup of ns-3 scratch directory
setup:
	@echo "Setting up by backing up ns-3.46.1/scratch..."
	cp -r ns-3.46.1/scratch ns-3.46.1/scratch_bak
	rm -rf ns-3.46.1/scratch/*
	@echo "✓ Setup completed. Backup created at scratch_bak"

# ==============================================================================
# CLEAN COMMANDS
# ==============================================================================

# Clean: restore scratch directory to original state from backup
clean:
	@echo "Restoring scratch directory from backup..."
	rm -rf ns-3.46.1/scratch/*
	@echo "✓ Clean completed. Scratch directory restored to original state."

# ==============================================================================
# DEVELOPMENT WORKFLOW COMMANDS
# ==============================================================================

# Copy: copy all files from code/ directory to ns-3 scratch/
copy:
	@echo "Copying files from code/ to ns-3.46.1/scratch/..."
	@# Check if code directory exists and is not empty
	if [ -d "code" ] && [ "$$(ls -A code)" ]; then \
		cp -r code/* ns-3.46.1/scratch/; \
		echo "✓ Files copied successfully."; \
	else \
		echo "⚠ Warning: code/ directory is empty or does not exist."; \
	fi
	@echo "✓ Copy completed."

# Build: compile ns-3 with updated code
build:
	@echo "Building ns-3..."
	@echo "Changing to ns-3.46.1 directory..."
	cd ns-3.46.1 && ./ns3 build --quiet
	cd ..
	@echo "✓ Build completed."

# Run: execute the main program in scratch/
run:
	@echo "Running ns-3 application..."
	@echo "Executing scratch/main ========================================"
	cd ns-3.46.1 && ./ns3 run scratch/main --no-build --quiet
	cd ..
	@echo "✓ Run completed. =============================================="

# ==============================================================================
# UTILITY COMMANDS
# ==============================================================================

# Help: display available targets and their descriptions
help:
	@echo "NS-3 Development Makefile"
	@echo "=========================="
	@echo ""
	@echo "Available targets:"
	@echo ""
	@echo "  setup           - Create initial backup of ns-3 scratch directory"
	@echo "  clean           - Restore scratch directory to original state"
	@echo "  copy            - Copy files from code/ to ns-3 scratch/"
	@echo "  build           - Compile ns-3 with updated code"
	@echo "  run             - Execute the main program (scratch/main)"
	@echo "  all             - Execute complete workflow: copy → build → run → clean"
	@echo "  help            - Show this help message"
	@echo ""
	@echo "Usage Examples:"
	@echo "  make setup      # Initial setup (run once)"
	@echo "  make            # Full development cycle"
	@echo "  make clean      # Reset scratch directory"
	@echo "  make copy build # Just copy and build (no run)"
	@echo "  make run        # Just run existing code"
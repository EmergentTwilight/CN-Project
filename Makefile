# Makefile for NS-3 Algorithm Comparison
# Simplified: Separate build and test responsibilities
#
# Algorithm switching method: Uses sed to patch source code before building
# - This bypasses CMake caching issues with CXXFLAGS
# - Modifies global-route-manager-impl.cc (m_useBmssp value)
# - Modifies test-*.cc files (algorithm name strings)
# ==============================================================================

.PHONY: all configure clean help breaking dijkstra show-alg setup-results
.PHONY: test-correctness test-scalability test-density test-topology test-realistic test-all
.PHONY: test-large-scale test-large-scale-quick

# NS-3 directory path
NS3_DIR = ns-allinone-3.46.1/ns-3.46.1

# Results directory
RESULTS_DIR = results

# Algorithm selection state file
ALG_STATE_FILE = .alg_state

# Get current algorithm from state file (default: unknown)
CURRENT_ALG = $(shell cat $(ALG_STATE_FILE) 2>/dev/null || echo "unknown")

# Algorithm-specific results directory
ALG_RESULTS_DIR = $(RESULTS_DIR)/$(CURRENT_ALG)

# ==============================================================================
# SETUP
# ==============================================================================

all: setup-results
	@echo "================================================================"
	@echo "Quick Start:"
	@echo "  make breaking          # Build with Breaking algorithm"
	@echo "  make dijkstra          # Build with Dijkstra algorithm"
	@echo "  make test-correctness  # Run correctness test"
	@echo "  make test-all          # Run all tests"
	@echo "================================================================"

setup-results:
	@mkdir -p $(ALG_RESULTS_DIR)

# ==============================================================================
# BUILD TARGETS (Algorithm Selection)
# ==============================================================================
# Algorithm switching is done via sed patching:
# - m_useBmssp(false) -> m_useBmssp(true) for Breaking
# - m_useBmssp(true) -> m_useBmssp(false) for Dijkstra
# - Test code algorithm names are also patched
# ==============================================================================

breaking:
	@echo "================================================================"
	@echo "Switching to Breaking algorithm..."
	@echo "================================================================"
	@echo "Patching source code to use Breaking algorithm..."
	cd $(NS3_DIR) && sed -i 's/m_useBmssp(false)/m_useBmssp(true)/g' src/internet/model/global-route-manager-impl.cc
	cd $(NS3_DIR) && for f in scratch/test-*.cc; do sed -i 's/"Algorithm"/"Breaking"/g' $$f; done
	@echo "Building..."
	cd $(NS3_DIR) && ./ns3 build -j 10
	@echo "breaking" > $(ALG_STATE_FILE)
	@echo "✓ Built with Breaking algorithm"
	@echo "  Now run: make test-correctness"

dijkstra:
	@echo "================================================================"
	@echo "Switching to Dijkstra algorithm..."
	@echo "================================================================"
	@echo "Patching source code to use Dijkstra algorithm..."
	cd $(NS3_DIR) && sed -i 's/m_useBmssp(true)/m_useBmssp(false)/g' src/internet/model/global-route-manager-impl.cc
	cd $(NS3_DIR) && for f in scratch/test-*.cc; do sed -i 's/"Algorithm"/"Dijkstra"/g' $$f; done
	@echo "Building..."
	cd $(NS3_DIR) && ./ns3 build -j 10
	@echo "dijkstra" > $(ALG_STATE_FILE)
	@echo "✓ Built with Dijkstra algorithm"
	@echo "  Now run: make test-correctness"

configure:
	@echo "Configuring NS-3 (default: Breaking)..."
	cd $(NS3_DIR) && ./ns3 configure --enable-examples --enable-tests --quiet -- CMAKE_CXX_FLAGS="-DNS3_USE_BMSSP=1"
	@echo "✓ Configuration completed"

clean:
	@echo "Cleaning NS-3 build files..."
	cd $(NS3_DIR) && ./ns3 clean
	@echo "✓ Clean completed"

show-alg:
	@if [ -f $(ALG_STATE_FILE) ]; then \
		ALG=$$(cat $(ALG_STATE_FILE)); \
		echo "Current algorithm: $$ALG"; \
	else \
		echo "Algorithm state unknown. Run 'make breaking' or 'make dijkstra' first."; \
	fi

# ==============================================================================
# TEST TARGETS (Run Only, No Build)
# ==============================================================================

test-correctness: setup-results
	@echo "================================================================"
	@echo "Running Correctness Tests (Experiment 1)"
	@echo "================================================================"
	@cd $(NS3_DIR) && ./ns3 run --no-build test-correctness
	@if [ -f $(NS3_DIR)/test-correctness-results.csv ]; then \
		ALG=$$(head -1 $(NS3_DIR)/test-correctness-results.csv | grep -oP '(?<=Algorithm:)[^,]*' | head -1); \
		if [ -z "$$ALG" ]; then ALG="$(CURRENT_ALG)"; fi; \
		mkdir -p $(RESULTS_DIR)/$$ALG; \
		mv $(NS3_DIR)/test-correctness-results.csv $(RESULTS_DIR)/$$ALG/; \
		echo "✓ Results: $(RESULTS_DIR)/$$ALG/test-correctness-results.csv"; \
	fi

test-scalability: setup-results
	@echo "================================================================"
	@echo "Running Scalability Tests (Experiment 2)"
	@echo "================================================================"
	@cd $(NS3_DIR) && ./ns3 run --no-build test-scalability
	@if [ -f $(NS3_DIR)/test-scalability-results.csv ]; then \
		ALG=$$(head -1 $(NS3_DIR)/test-scalability-results.csv | grep -oP '(?<=Algorithm:)[^,]*' | head -1); \
		if [ -z "$$ALG" ]; then ALG="$(CURRENT_ALG)"; fi; \
		mkdir -p $(RESULTS_DIR)/$$ALG; \
		mv $(NS3_DIR)/test-scalability-results.csv $(RESULTS_DIR)/$$ALG/test-scalability-results.csv; \
		echo "✓ Results: $(RESULTS_DIR)/$$ALG/test-scalability-results.csv"; \
	fi

test-density: setup-results
	@echo "================================================================"
	@echo "Running Density Tests (Experiment 3)"
	@echo "================================================================"
	@cd $(NS3_DIR) && ./ns3 run --no-build test-density
	@if [ -f $(NS3_DIR)/test-density-results.csv ]; then \
		ALG=$$(head -1 $(NS3_DIR)/test-density-results.csv | grep -oP '(?<=Algorithm:)[^,]*' | head -1); \
		if [ -z "$$ALG" ]; then ALG="$(CURRENT_ALG)"; fi; \
		mkdir -p $(RESULTS_DIR)/$$ALG; \
		mv $(NS3_DIR)/test-density-results.csv $(RESULTS_DIR)/$$ALG/; \
		echo "✓ Results: $(RESULTS_DIR)/$$ALG/test-density-results.csv"; \
	fi

test-topology: setup-results
	@echo "================================================================"
	@echo "Running Topology Tests (Experiment 4)"
	@echo "================================================================"
	@cd $(NS3_DIR) && ./ns3 run --no-build test-topology
	@if [ -f $(NS3_DIR)/test-topology-results.csv ]; then \
		ALG=$$(head -1 $(NS3_DIR)/test-topology-results.csv | grep -oP '(?<=Algorithm:)[^,]*' | head -1); \
		if [ -z "$$ALG" ]; then ALG="$(CURRENT_ALG)"; fi; \
		mkdir -p $(RESULTS_DIR)/$$ALG; \
		mv $(NS3_DIR)/test-topology-results.csv $(RESULTS_DIR)/$$ALG/; \
		echo "✓ Results: $(RESULTS_DIR)/$$ALG/test-topology-results.csv"; \
	fi

test-realistic: setup-results
	@echo "================================================================"
	@echo "Running Realistic Scenario Tests (Experiment 5)"
	@echo "================================================================"
	@cd $(NS3_DIR) && ./ns3 run --no-build test-realistic
	@if [ -f $(NS3_DIR)/test-realistic-results.csv ]; then \
		ALG=$$(head -1 $(NS3_DIR)/test-realistic-results.csv | grep -oP '(?<=Algorithm:)[^,]*' | head -1); \
		if [ -z "$$ALG" ]; then ALG="$(CURRENT_ALG)"; fi; \
		mkdir -p $(RESULTS_DIR)/$$ALG; \
		mv $(NS3_DIR)/test-realistic-results.csv $(RESULTS_DIR)/$$ALG/; \
		echo "✓ Results: $(RESULTS_DIR)/$$ALG/test-realistic-results.csv"; \
	fi

test-all: setup-results
	@echo "================================================================"
	@echo "Running All Experiments"
	@echo "================================================================"
	@$(MAKE) test-correctness
	@$(MAKE) test-scalability
	@$(MAKE) test-density
	@$(MAKE) test-topology
	@$(MAKE) test-realistic
	@echo ""
	@echo "✓ All tests completed!"
	@echo "Results in: $(RESULTS_DIR)/$(CURRENT_ALG)/"

# ==============================================================================
# LARGE-SCALE TESTS
# ==============================================================================
# WARNING: Large-scale tests can take several hours!
# Use test-large-scale-quick for a quick test instead.
# ==============================================================================

test-large-scale-quick: setup-results
	@echo "================================================================"
	@echo "Running Large-Scale Tests (Quick Mode - Small Scale)"
	@echo "================================================================"
	@echo "Test scope: 625-1225 nodes (estimated time: 3-5 minutes)"
	@echo "================================================================"
	@cd $(NS3_DIR) && ./ns3 run --no-build "test-large-scale --quick"
	@if [ -f $(NS3_DIR)/test-large-scale-results.csv ]; then \
		ALG=$$(head -1 $(NS3_DIR)/test-large-scale-results.csv | grep -oP '(?<=Algorithm:)[^,]*' | head -1); \
		if [ -z "$$ALG" ]; then ALG="$(CURRENT_ALG)"; fi; \
		mkdir -p $(RESULTS_DIR)/$$ALG; \
		cp $(NS3_DIR)/test-large-scale-results.csv $(RESULTS_DIR)/$$ALG/test-large-scale-results-quick.csv; \
		echo "✓ Results: $(RESULTS_DIR)/$$ALG/test-large-scale-results-quick.csv"; \
	fi

test-large-scale: setup-results
	@echo "================================================================"
	@echo "⚠️  WARNING: Large-Scale Test"
	@echo "================================================================"
	@echo "This test will run 625-22500 nodes (estimated time: several hours)"
	@echo ""
	@echo "Are you sure you want to continue? Press Ctrl+C to cancel."
	@echo "Starting in 5 seconds..."
	@sleep 5
	@echo "================================================================"
	@echo "Running Large-Scale Tests (Full Scale)"
	@echo "================================================================"
	@cd $(NS3_DIR) && ./ns3 run --no-build "test-large-scale"
	@if [ -f $(NS3_DIR)/test-large-scale-results.csv ]; then \
		ALG=$$(head -1 $(NS3_DIR)/test-large-scale-results.csv | grep -oP '(?<=Algorithm:)[^,]*' | head -1); \
		if [ -z "$$ALG" ]; then ALG="$(CURRENT_ALG)"; fi; \
		mkdir -p $(RESULTS_DIR)/$$ALG; \
		cp $(NS3_DIR)/test-large-scale-results.csv $(RESULTS_DIR)/$$ALG/test-large-scale-results-full.csv; \
		echo "✓ Results: $(RESULTS_DIR)/$$ALG/test-large-scale-results-full.csv"; \
	fi

# ==============================================================================
# HELP
# ==============================================================================

help:
	@echo "================================================================"
	@echo "NS-3 Algorithm Comparison - Simplified Makefile"
	@echo "================================================================"
	@echo ""
	@echo "BUILD TARGETS (compile NS-3, quiet output):"
	@echo "  make breaking          Build with Breaking algorithm"
	@echo "  make dijkstra          Build with Dijkstra algorithm"
	@echo "  make configure         Configure NS-3 (first time only, verbose)"
	@echo "  make clean             Clean build files"
	@echo ""
	@echo "TEST TARGETS (run tests, no build, quiet output):"
	@echo "  make test-correctness      Run Exp1: Correctness verification"
	@echo "  make test-scalability      Run Exp2: Node scaling tests"
	@echo "  make test-density          Run Exp3: Graph density tests"
	@echo "  make test-topology         Run Exp4: Topology type tests"
	@echo "  make test-realistic        Run Exp5: Real-world scenarios"
	@echo "  make test-all              Run all experiments"
	@echo ""
	@echo "LARGE-SCALE TESTS:"
	@echo "  make test-large-scale-quick Run quick large-scale test (3-5 min)"
	@echo "  make test-large-scale       Run full large-scale test (hours!)"
	@echo ""
	@echo "TYPICAL WORKFLOW:"
	@echo "  1. make configure       # First time setup"
	@echo "  2. make breaking        # Build with Breaking (quiet)"
	@echo "  3. make test-all        # Run all tests (quiet)"
	@echo "  4. make dijkstra        # Rebuild with Dijkstra (quiet)"
	@echo "  5. make test-all        # Run all tests again (quiet)"
	@echo ""
	@echo "ALGORITHM SWITCHING:"
	@echo "  Uses sed to patch source code before building"
	@echo "  - global-route-manager-impl.cc: m_useBmssp value"
	@echo "  - test-*.cc files: algorithm name strings"
	@echo ""
	@echo "RESULTS:"
	@echo "  Results auto-saved to: $(RESULTS_DIR)/{breaking|dijkstra}/"
	@echo "  CSV format: test-{name}-results.csv"
	@echo ""
	@echo "================================================================"

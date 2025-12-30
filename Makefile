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
	cd $(NS3_DIR) && for f in scratch/test-*.cc; do sed -i 's/result\.algorithm = "Dijkstra"/result.algorithm = "Breaking"/g' $$f; done
	@echo "Building..."
	cd $(NS3_DIR) && ./ns3 build >/dev/null 2>&1
	@echo "breaking" > $(ALG_STATE_FILE)
	@echo "✓ Built with Breaking algorithm"
	@echo "  Now run: make test-correctness"

dijkstra:
	@echo "================================================================"
	@echo "Switching to Dijkstra algorithm..."
	@echo "================================================================"
	@echo "Patching source code to use Dijkstra algorithm..."
	cd $(NS3_DIR) && sed -i 's/m_useBmssp(true)/m_useBmssp(false)/g' src/internet/model/global-route-manager-impl.cc
	cd $(NS3_DIR) && for f in scratch/test-*.cc; do sed -i 's/result\.algorithm = "Breaking"/result.algorithm = "Dijkstra"/g' $$f; done
	@echo "Building..."
	cd $(NS3_DIR) && ./ns3 build >/dev/null 2>&1
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
	@cd $(NS3_DIR) && ./ns3 run --no-build test-correctness 2>/dev/null
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
	@cd $(NS3_DIR) && ./ns3 run --no-build test-scalability 2>/dev/null
	@if [ -f $(NS3_DIR)/test-scalability-2-results.csv ]; then \
		ALG=$$(head -1 $(NS3_DIR)/test-scalability-2-results.csv | grep -oP '(?<=Algorithm:)[^,]*' | head -1); \
		if [ -z "$$ALG" ]; then ALG="$(CURRENT_ALG)"; fi; \
		mkdir -p $(RESULTS_DIR)/$$ALG; \
		mv $(NS3_DIR)/test-scalability-2-results.csv $(RESULTS_DIR)/$$ALG/test-scalability-results.csv; \
		echo "✓ Results: $(RESULTS_DIR)/$$ALG/test-scalability-results.csv"; \
	fi

test-density: setup-results
	@echo "================================================================"
	@echo "Running Density Tests (Experiment 3)"
	@echo "================================================================"
	@cd $(NS3_DIR) && ./ns3 run --no-build test-density 2>/dev/null
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
	@cd $(NS3_DIR) && ./ns3 run --no-build test-topology 2>/dev/null
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
	@cd $(NS3_DIR) && ./ns3 run --no-build test-realistic 2>/dev/null
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
	@echo "  make test-correctness  Run Exp1: Correctness verification"
	@echo "  make test-scalability  Run Exp2: Node scaling tests"
	@echo "  make test-density      Run Exp3: Graph density tests"
	@echo "  make test-topology     Run Exp4: Topology type tests"
	@echo "  make test-realistic    Run Exp5: Real-world scenarios"
	@echo "  make test-all          Run all experiments"
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

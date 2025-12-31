# Breaking Algorithm Implementation Analysis

**Date**: 2024-12-31
**Status**: Pre-Fix Analysis

---

## 1. Test Results Summary (v1 - Before Fix)

### 1.1 Correctness Tests
| Test | Nodes | Breaking (ms) | Dijkstra (ms) | Status |
|------|-------|---------------|---------------|--------|
| Grid5x5 | 25 | ~5 | ~10 | ✅ PASS |
| Grid10x10 | 100 | ~700 | ~500 | ✅ PASS |
| Star20 | 20 | ~1 | ~0.2 | ✅ PASS |
| Complete10 | 10 | ~1 | ~1.6 | ✅ PASS |

**Conclusion**: All correctness tests pass.

---

### 1.2 Scalability Tests - **CRITICAL ISSUE**

| Test | Nodes | Breaking | Dijkstra | Speedup |
|------|-------|----------|----------|---------|
| Grid5x5 | 25 | 5.2ms | 10.4ms | **2.0x** ✅ |
| Grid10x10 | 100 | 693ms | 470ms | 0.68x ⚠️ |
| Grid15x15 | 225 | **813,399ms** (~13.5min) | 5,430ms | **0.007x** ❌ |
| Grid20x20 | 400 | **OOM/SIGKILL** | 32,072ms | **-** ❌ |

**Problem**: Breaking algorithm exhibits catastrophic performance degradation on grid graphs.

---

### 1.3 Density Tests - **Good Performance**

| Density | Edges | Breaking (ms) | Dijkstra (ms) | Speedup |
|---------|-------|---------------|---------------|---------|
| Sparse (2.3) | 229 | 190 | 825 | **4.3x** ✅ |
| Medium (4.1) | 407 | 490 | 3,206 | **6.5x** ✅ |
| Dense (5.9) | 585 | 980 | 14,001 | **14.3x** ✅ |
| VeryDense (10.6) | 1063 | 3,000 | 79,650 | **26.6x** ✅ |
| UltraDense (20.4) | 2042 | 11,000 | 2,088,437 | **190x** ✅ |

**Conclusion**: Breaking excels on dense graphs.

---

### 1.4 Topology Tests

| Topology | Nodes | Edges | Breaking (ms) | Dijkstra (ms) | Winner |
|----------|-------|-------|---------------|---------------|--------|
| Grid | 100 | 180 | 726 | 447 | Dijkstra |
| Random | 100 | 393 | 490 | 4,906 | **Breaking** (10x) |
| Star | 100 | 99 | 177 | 2.6 | Dijkstra |
| Tree | 100 | 99 | 83 | 30 | Dijkstra |

---

## 2. Root Cause Analysis

### 2.1 Algorithm Parameters

| n | k = ⌊log^(1/3)n⌋ | t = ⌊log^(2/3)n⌋ | l = ⌈log n / t⌉ |
|---|-------------------|-------------------|------------------|
| 25 | 2 | 3 | 3 |
| 100 | 2 | 3 | 3 |
| 225 | 2 | 3 | 3 |
| 400 | 2 | 3 | 4 |

**Issue**: k=2 is too small for sparse regular graphs to effectively prune the recursion tree.

---

### 2.2 Implementation vs Paper - Critical Discrepancies

### ❌ **P0 Bug #1: BaseCase Missing k+1 Exit Condition**

**Paper (Algorithm 2, Line 7)**:
```
while H is non-empty and |U_0| < k + 1
```

**Implementation (bmssp.cc:289)**:
```cpp
while (!pq.empty())  // ❌ Missing |U| < k+1 condition!
{
    // ...
    if (!alreadyInU) {
        U.push_back(u);
    }
    // Continues until heap is empty, not until k+1 nodes found
}
```

**Impact**: BaseCase explores ALL reachable nodes instead of stopping at k+1. On a 15×15 grid (225 nodes), this means exploring potentially hundreds of nodes per BaseCase call, multiplying through the recursion tree.

---

### ⚠️ **P0 Bug #2: No Recursion Depth Limit**

**Paper**: Implies maximum recursion depth of O(log n / t)

**Implementation**: No explicit recursion depth limit.

**Impact**: Combined with Bug #1, this leads to exponential explosion of operations.

---

### ⚠️ **P1 Bug #3: FindPivots Missing B Boundary Check**

**Paper**: FindPivots should ensure `d[u] < B` before relaxing edges

**Implementation (bmssp.cc:163)**:
```cpp
if (dis[u] != BMSSP_INF && dis[u] + w <= dis[v])  // Missing dis[u] < B check
```

---

### ⚠️ **P1 Bug #4: Partial Execution Check Timing**

**Paper**: Check |U| > k·2^{lt} should happen after U_i is computed

**Implementation**: Check happens after U = U ∪ U_i, which may already exceed threshold significantly.

---

## 3. Data Structure Issues

### BlockHeap Implementation

| Operation | Expected | Implementation | Status |
|-----------|----------|----------------|--------|
| Delete | O(1) | O(1) after fix ✅ | Fixed |
| FindMedianAndPartition | O(M) | O(M) with Quickselect ✅ | Fixed |
| Pull | O(|S'|) | O(M·|blocks|) ⚠️ | Needs optimization |

---

## 4. Fix Plan

### Phase 1: P0 Critical Fixes (Required for correctness)

#### Fix #1: BaseCase k+1 Exit Condition
```cpp
// In BaseCase() - bmssp.cc:289
while (!pq.empty() && U.size() < k + 1)  // Add k+1 check
{
    // ... rest of loop
}
```

#### Fix #2: Recursion Depth Limit
```cpp
// In BMSSP() - bmssp.cc:383
const int MAX_LEVEL = 5;  // Safety limit
if (level >= MAX_LEVEL)
{
    return BaseCase(B, S);
}
```

### Phase 2: P1 Important Fixes

#### Fix #3: FindPivots Boundary Check
#### Fix #4: Partial Execution Timing

### Phase 3: P2 Optimizations (Optional)

#### Optimization #1: Pull Operation with auxiliary heap
#### Optimization #2: Parameter adaptation for sparse graphs

---

## 5. Expected Results After Fixes

| Test | Before | After (Expected) |
|------|--------|------------------|
| Grid5x5 | 5.2ms | ~5ms |
| Grid10x10 | 693ms | ~500ms |
| Grid15x15 | 813s | ~5-10s |
| Grid20x20 | OOM | ~50-100s |

---

## 6. References

- Paper: "Breaking the Sorting Barrier for Directed Single-Source Shortest Paths"
- Implementation: `src/internet/model/bmssp.cc`, `bmssp.h`
- Test Results: `docs/benchmark/v1/results/`

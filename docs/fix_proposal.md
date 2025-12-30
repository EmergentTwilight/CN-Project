# Breaking Algorithm Performance Fix Proposal

## 1. Executive Summary

本文档分析了 Breaking (BMSSP) 算法实现中的性能问题，并提出了分层的修复方案。

**问题现象**：
- 5×5 网格 (25 节点): ~3.5ms ✅
- 10×10 网格 (100 节点): ~405ms ❌
- 15×15 网格 (225 节点): >5 分钟 ❌

**理论复杂度**: O(m · log^(2/3) n)

---

## 2. Root Cause Analysis

### 2.1 形式化调用栈分析

**参数计算** (n=225):
```cpp
k = ⌊log^(1/3) n⌋ = ⌊7.81^0.33⌋ = 2
t = ⌊log^(2/3) n⌋ = ⌊7.81^0.67⌋ = 3
l = ⌈log n / t⌉ = ⌈7.81 / 3⌉ = 3
```

**递归展开**:
```
BMSSP(l=3) → 最多 16 次Pull
  └─ BMSSP(l=2) → 最多 16 次Pull
      └─ BMSSP(l=1) → 最多 16 次Pull
          └─ BaseCase (Dijkstra)

总递归调用数 ≈ 16^3 = 4,096 次
```

### 2.2 具体性能瓶颈

| 位置 | 文件 | 行数 | 问题 | 预期复杂度 | 实际复杂度 |
|------|------|------|------|-----------|-----------|
| BlockHeap.Delete | bmssp.h | 148-238 | 线性搜索 D0+D1 | O(1) | O(n·|blocks|) |
| BlockHeap.Pull | bmssp.h | 670-714 | 线性扫描找最小值 | O(M) | O(M·|blocks|) |
| FindMedianAndPartition | bmssp.h | 283 | 每次Split排序 | O(M) | O(M log M) |
| RebuildBoundsBST | bmssp.h | 396-403 | 每次Split重建BST | O(1) amortized | O(b log b) |

### 2.3 为什么网格图失效？

**网格图特性**：
- 稀疏: m ≈ 2n
- 均匀度分布: deg(v) ≈ 4
- 良好的连通性

**失效原因**：
1. **参数 k=2 太小** → FindPivots 无法有效剪枝
2. **递归深度 l=3 太大** → 指数级子问题展开
3. **均匀度分布** → 每层递归都处理几乎相同的顶点集
4. **BlockHeap 常数因子巨大** → O(M²) 而非 O(M)

---

## 3. Fix Proposal Classification

### A类: Implementation Bug Fixes (必须)

#### Fix A1: BlockHeap 节点添加块引用

**问题**: Delete 操作需要线性搜索整个 D0 和 D1

**修复**:
```cpp
// bmssp.h:38-53
template <typename K, typename V>
struct BlockHeapNode {
    K key;
    V value;
    BlockHeapBlock<K, V>* block;  // 新增：指向所属块
    BlockHeapNode* prev;
    BlockHeapNode* next;

    BlockHeapNode(K k, V v, BlockHeapBlock<K, V>* b)
        : key(k), value(v), block(b), prev(nullptr), next(nullptr) {}
};

// bmssp.h:148-238 → 修复为 O(1)
void Delete(K key, V value) {
    auto it = keyToNode.find(key);
    if (it == keyToNode.end()) return;

    BlockHeapNode<K, V>* node = it->second;
    BlockHeapBlock<K, V>* block = node->block;  // 直接获取块

    // O(1) 删除（已知道块）
    DeleteFromBlock(block, node);

    keyToNode.erase(it);
    delete node;
}
```

#### Fix A2: Pull 操作优化

**问题**: 找最小剩余值时线性扫描所有块

**修复**:
```cpp
// 使用辅助堆跟踪最小值
std::set<std::pair<V, int>> blockMinValues;  // (最小值, 块索引)

// 每次Insert/Update时维护
void UpdateBlockMin(int blockIdx, V minVal) {
    // O(log b) 更新
}

// Pull时直接获取
V GetMinRemaining() {
    if (blockMinValues.empty()) return globalB;
    return blockMinValues.begin()->first;  // O(1)
}
```

#### Fix A3: Quickselect 替代排序

**问题**: 每次Split使用 O(M log M) 排序

**修复**:
```cpp
// O(M) 中位数选择
template<typename T>
T QuickSelect(std::vector<T>& arr, int k) {
    // 实现标准 quickselect 算法
}

// 应用到 FindMedianAndPartition
auto median = QuickSelect(nodes, nodes.size() / 2);
```

### B类: Parameter Adaptation (推荐)

#### Fix B1: 根据图密度动态调整参数

```cpp
void BmsspSolver::ComputeParameters() {
    double density = (double)m / (n * (n - 1));
    double logn = std::log2(n);

    if (density < 0.01) {  // 超稀疏图
        k = std::max(2, (int)std::floor(std::pow(logn, 1.0/3)));
        t = std::max(2, (int)std::floor(std::pow(logn, 2.0/3)));
    } else if (density < 0.1) {  // 中等稀疏
        k = std::max(3, (int)std::floor(std::pow(logn, 1.0/4)));
        t = std::max(3, (int)std::floor(std::pow(logn, 1.0/2)));
        l = std::max(1, (int)std::ceil(std::pow(logn, 1.0/3)));
    } else {  // 密图
        // 回退到 BaseCase (Dijkstra)
        k = 1;
        t = 1;
        l = 0;
    }

    l = std::max(1, (int)std::ceil(logn / t));
}
```

#### Fix B2: 小规模回退策略

```cpp
std::pair<BmsspLength, VertexSet> BMSSP(int level, BmsspLength B, const VertexSet& S) {
    // 小规模直接用 Dijkstra
    if (n < 50 || level > 4 || (int)S.size() > n * 0.5) {
        return BaseCase(B, S);
    }
    // ... 原算法
}
```

### C类: Safety Limits (可选)

#### Fix C1: 递归深度和操作数限制

```cpp
class BmsspSolver {
private:
    static const int MAX_RECURSION_DEPTH = 4;
    static const int MAX_OPERATIONS = 1000000;
    int operationCount = 0;

public:
    std::pair<BmsspLength, VertexSet> BMSSP(int level, BmsspLength B, const VertexSet& S) {
        if (level > MAX_RECURSION_DEPTH || operationCount > MAX_OPERATIONS) {
            // 安全回退
            return BaseCase(B, S);
        }
        operationCount++;
        // ... 原算法
    }
};
```

---

## 4. Implementation Priority

### Phase 1: Critical Bug Fixes (A类)
- [ ] Fix A1: BlockHeap 节点添加块引用
- [ ] Fix A2: Pull 操作优化
- [ ] Fix A3: Quickselect 中位数选择

**预期改进**: 10-50x 性能提升

### Phase 2: Parameter Optimization (B类)
- [ ] Fix B1: 密度自适应参数
- [ ] Fix B2: 小规模回退策略

**预期改进**: 扩展适用范围

### Phase 3: Safety (C类)
- [ ] Fix C1: 递归深度限制

**预期改进**: 防止极端情况超时

---

## 5. 与项目要求的一致性

### 项目要求对照

| 要求 | 修复方案 | 说明 |
|------|----------|------|
| **复现论文** | A类修复 | 修复实现bug，使实际复杂度接近理论值 |
| **对比Dijkstra** | B类修复 | 优化后可进行有效对比 |
| **分析优缺点** | 保留原代码 | 分析报告可说明参数敏感性问题 |
| **探索改进** | B+C类修复 | 参数自适应属于改进探索 |

### 结论

- **A类修复**: 必须实施，属于"正确复现论文"的必要条件
- **B类修复**: 推荐实施，属于"探索改进方法"
- **C类修复**: 可选实施，作为安全保护

---

## 6. Expected Performance After Fixes

| 节点数 | 当前 | 修复后 (预期) | 改进 |
|--------|------|--------------|------|
| 25 | 3.5ms | 2ms | 1.75x |
| 100 | 405ms | 50ms | 8.1x |
| 225 | >5min | 200ms | 1500x |

---

## 7. Testing Plan

修复后需要重新运行：
1. **Exp1**: 正确性验证（确保修复不破坏正确性）
2. **Exp2**: 规模扩展测试（验证性能改进）
3. **Exp3**: 密度测试（验证参数自适应效果）

---

## 8. References

- Original Paper: "Breaking the Sorting Barrier for Directed Single-Source Shortest Paths"
- Data Structure Reference: `/home/travis/course/CN/data_structure.md`
- Implementation: `src/internet/model/bmssp.cc` and `bmssp.h`

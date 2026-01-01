# Breaking 算法 v4：并行度分析与火焰图

**日期**: 2025-12-31
**版本**: v4 并行化设计
**主题**: TLS + 归约方案的并行度分析

---

## 概要

本文档通过调用栈分析和火焰图可视化，详细分析 Breaking 算法采用 TLS + 归约方案时的并行度潜力，包括：
- 哪些位置可以并行
- 哪些位置必须串行
- 最大并行任务数量
- 并行度取决于哪些参数

**核心结论**:
- **理论最大并行度**: M = 2^{(l-1)×t}
- **实际并行度**: 受限于 k、n、图结构
- **典型场景并行度**: 2 - 128 个并行任务

---

## 目录

1. [算法调用栈分析](#1-算法调用栈分析)
2. [火焰图可视化](#2-火焰图可视化)
3. [并行度计算](#3-并行度计算)
4. [实际案例分析](#4-实际案例分析)
5. [并行效率分析](#5-并行效率分析)

---

## 1. 算法调用栈分析

### 1.1 完整调用栈

```
Run(source)
└─ BMSSP(l=2, B=∞, S={source})
   ├─ FindPivots(...)              // 串行
   │  ├─ Bellman-Ford 迭代 (k 次)   // 可部分并行
   │  └─ DFS 计算树大小             // 可并行
   ├─ BlockHeap 初始化              // 串行
   └─ while (!D.IsEmpty() && ...)  // 主循环 - 串行
      ├─ D.Pull()                   // 返回 S_i, 最多 M 个节点
      ├─ BMSSP(l=1, B_i, S_i)       // 🔥 关键并行点
      │  ├─ FindPivots(...)
      │  └─ while (!D.IsEmpty() && ...)
      │     ├─ D.Pull()             // 返回 S_j, 最多 M/2 个节点
      │     └─ BMSSP(l=0, B_j, S_j) // 🔥 BaseCase - 并行点
      │        └─ BaseCase(...)     // 🔥 可对 |S_j| 个节点并行
      ├─ Relax edges (串行)          // 依赖 BMSSP 结果
      └─ D.BatchPrepend(...)         // 串行
```

### 1.2 串行依赖关系

```
BMSSP(level, B, S):
┌─────────────────────────────────────────────────────────┐
│ 1. FindPivots(B, S)                                    │ 串行
│    └─ 计算 pivots P 和可达集合 W                         │
│                                                          │
│ 2. 初始化 BlockHeap D                                   │ 串行
│    └─ Insert pivots P into D                            │
│                                                          │
│ 3. Main Loop: while (|U| < k·2^{level×t} && !D.Empty) │ 串行迭代
│    │                                                    │
│    ├─ 3.1: S_i, B_i = D.Pull()                         │ 串行
│    │       返回最多 M = 2^{(level-1)×t} 个节点          │
│    │                                                    │
│    ├─ 3.2: BMSSP(level-1, B_i, S_i) ◄─────┐           │ 🔥 可并行
│    │       │                               │           │
│    │       └─ 递归处理 S_i 中的节点         │           │
│    │                                        │           │
│    ├─ 3.3: Relax edges from U_i  ◀─────────┘           │ 串行，依赖 3.2
│    │       更新距离，插入 D                                │
│    │                                                    │
│    └─ 3.4: D.BatchPrepend(...)                          │ 串行
└─────────────────────────────────────────────────────────┘
```

### 1.3 并行机会识别

| 位置 | 可行性 | 并行度 | 依赖 |
|------|--------|--------|------|
| **FindPivots 中的 DFS** | ✅ 可行 | O(|roots|) | 树结构 |
| **主循环迭代之间** | ❌ 不可行 | 1 | 数据依赖 |
| **单次迭代内的 BMSSP** | ✅ 可行 | |S_i| ≤ M | 无 |
| **BaseCase 处理多个节点** | ✅ 可行 | |S_j| | 无 |
| **边松弛** | ⚠️ 部分可行 | O(\|U_i\|) | 需要合并 |

---

## 2. 火焰图可视化

### 2.1 算法执行火焰图

```
深度:  0         1         2         3         4
       │         │         │         │         │
       ▼         ▼         ▼         ▼         ▼
Run ──► BMSSP(l=2)
       │         │
       │         ├─► FindPivots (串行)
       │         │   ├─► BF-iter-1
       │         │   ├─► BF-iter-2
       │         │   └─► DFS (roots)     ⚡ 可并行
       │         │
       │         ├─► Init D (串行)
       │         │
       │         └─► Main Loop
       │                │
       │                ├─► Iter-1
       │                │   ├─► Pull M=2^t 节点
       │                │   ├─► BMSSP(l=1)
       │                │   │   ├─► FindPivots
       │                │   │   └─► Main Loop
       │                │   │       ├─► Pull M/2 节点
       │                │   │       ├─► BMSSP(l=0) ──► BaseCase ◄─────┐
       │                │   │       │                              │ 🔥
       │                │   │       └─► Relax edges                │ 并行点
       │                │   └─► Relax edges ◀────────────────────────┘
       │                │
       │                ├─► Iter-2
       │                │   ├─► Pull M=2^t 节点
       │                │   ├─► BMSSP(l=1) ──────────────────────────┐
       │                │   │                                          │ 🔥 并行点
       │                │   └─► Relax edges ◀──────────────────────────┘
       │                │
       │                └─► Iter-3...
```

### 2.2 BaseCase 详细火焰图

```
BaseCase(B, S)  where S = {s1, s2, ..., sm}
│
├─► 对每个 s_i ∈ S:
│   ├─► Initialize pq (局部)
│   ├─► pq.push(dis[s_i], s_i)
│   │
│   └─► while (!pq.empty() && |U| < k+1)
│       ├─► u = pq.pop()
│       ├─► Check: d == dis[u]?
│       │
│       └─► For each edge (u → v):
│           ├─► newDist = dis[u] + w
│           ├─► if (newDist < dis[v]):
│           │   ├─► dis[v] = newDist     ⚡ 竞争写入
│           │   └─► parent[v] = u        ⚡ 竞争写入
│           │
│           └─► pq.push(dis[v], v)       (局部，无竞争)
```

**关键观察**:
- `pq` 是线程局部的，无竞争
- `dis[v]` 和 `parent[v]` 是共享的，需要处理竞争

### 2.3 TLS + 归约方案火焰图

```
Parallel BaseCase with TLS + Reduction:
│
├─► Phase 1: 并行执行 (TLS)
│   │
│   ├─ Thread-0 ──► BaseCase(B, {s1})
│   │   ├─► 使用 dis_local[0][], parent_local[0][]
│   │   ├─► 使用 pq_local[0]
│   │   └─► 结果: U_local[0]
│   │
│   ├─ Thread-1 ──► BaseCase(B, {s2})
│   │   ├─► 使用 dis_local[1][], parent_local[1][]
│   │   ├─► 使用 pq_local[1]
│   │   └─► 结果: U_local[1]
│   │
│   ├─ Thread-2 ──► BaseCase(B, {s3})
│   │   └─► ...
│   │
│   └─ Thread-m ──► BaseCase(B, {sm})
│       └─► ...
│
└─► Phase 2: 归约 (串行或并行归约)
    ├─► Merge all U_local[i] into U
    ├─► For each v: dis[v] = min(dis_local[i][v])
    └─► For each v: parent[v] = argmin dis_local[i][v]
```

---

## 3. 并行度计算

### 3.1 参数定义

| 参数 | 定义 | 计算公式 | 典型值 (n=100) |
|------|------|---------|---------------|
| **n** | 图节点数 | 输入 | 100 |
| **k** | 枢轴参数 | ⌊log^(1/3)n⌋ | ⌊log2(100)^(1/3)⌋ = ⌊4.6⌋ = 4 |
| **t** | 层级参数 | ⌊log^(2/3)n⌋ | ⌊log2(100)^(2/3)⌋ = ⌊3.7⌋ = 3 |
| **l** | 递归深度 | ⌈log n / t⌉ | ⌈log2(100) / 3⌉ = ⌈6.6/3⌉ = 3 |
| **M_level** | level 层的 Pull 大小 | 2^{(level-1)×t} | 见下表 |

### 3.2 M 参数随层级变化

| level | M = 2^{(level-1)×t} (t=3) | 说明 |
|-------|------------------------|------|
| l=3 | 2^{2×3} = 2^6 = **64** | 顶层，最大并行度 |
| l=2 | 2^{1×3} = 2^3 = **8** | 中层 |
| l=1 | 2^{0×3} = 2^0 = **1** | 底层，单节点 |
| l=0 | BaseCase | **k+1 = 5** | BaseCase 最大 U |

### 3.3 并行度分析

#### 主循环迭代内 (level = l-1)

```cpp
while ((int)U.size() < k * (1 << (level * t)) && !D.IsEmpty())
{
    // Pull 返回最多 M = 2^{(level-1)×t} 个节点
    auto [S_i, B_i] = D.Pull();  // |S_i| ≤ M

    // 对 S_i 中每个节点递归调用
    // 如果 level-1 = 0，则调用 BaseCase
    // 如果 level-1 > 0，则继续递归
}
```

**并行点 1: BaseCase level-1 = 0**

当递归到 level-1 = 0 时：
```cpp
auto [S_i, B_i] = D.Pull();  // S_i 最多 M = 2^{(l-2)×t} 个节点

// 当前: 串行对每个节点调用 BaseCase
for (int x : S_i) {
    auto result = BaseCase(B_i, {x});  // 串行
}

// 🔥 并行化: 对 S_i 中所有节点并行调用 BaseCase
#pragma omp parallel for
for (int j = 0; j < S_i.size(); j++) {
    auto result = BaseCase(B_i, {S_i[j]});  // 并行
}
```

**最大并行度** = |S_i| ≤ M = 2^{(l-2)×t}

#### 主循环迭代之间 (串行)

主循环的迭代必须串行执行，因为：
1. 每次迭代依赖上一次的 `B_prime_i`
2. `U` 累积需要上一次的 `U_i`
3. BlockHeap D 的状态随迭代变化

```cpp
// 迭代之间有数据依赖
B_prime_i = ...  // 上次迭代的结果

// 本次迭代
auto [S_i, B_i] = D.Pull();
auto [B_prime_i_curr, U_i] = BMSSP(level-1, B_i, S_i);
B_prime_i = B_prime_i_curr;  // 更新状态

U.insert(U.end(), U_i.begin(), U_i.end());  // 累积
```

### 3.4 最大并行度公式

#### 理论最大并行度

在单次主循环迭代内，当 level-1 = 0 时：

```
P_max = |S_i| ≤ M = 2^{(l-2)×t}
```

对于 n = 100 (k=4, t=3, l=3):
```
P_max = 2^{(3-2)×3} = 2^3 = 8
```

#### 实际并行度

实际并行度受以下因素限制：

| 限制因素 | 影响 | 典型值 |
|---------|------|--------|
| **|S_i| 实际大小** | Pull 可能返回少于 M 个节点 | 1 - M |
| **k 参数** | BaseCase 限制 U 大小为 k+1 | k+1 ≈ 5 |
| **图结构** | 稀疏图并行度低 | - |
| **硬件核心数** | 物理限制 | 2 - 128 |

**保守估计**:
```
P_actual ≈ min(M, k, |S_i|, num_cores)
```

---

## 4. 实际案例分析

### 4.1 案例 1: n = 100 节点网格

**参数**:
```
n = 100
k = ⌊log2(100)^(1/3)⌋ = ⌊4.60⌋ = 4
t = ⌊log2(100)^(2/3)⌋ = ⌊3.72⌋ = 3
l = ⌈log2(100) / 3⌉ = ⌈6.64 / 3⌉ = 3
```

**层级并行度**:

| level | 函数 | M | 可并行 | 说明 |
|-------|------|---|--------|------|
| 3 | BMSSP(l=3) | - | ❌ | 顶层，主循环串行 |
| 2 | BMSSP(l=2) | 8 | ⚠️ | 中层，迭代内可部分并行 |
| 1 | BMSSP(l=1) | 1 | ✅ | 底层，S_i 最多 8 个节点 |
| 0 | BaseCase | k+1=5 | ✅ | **P_max = 8** |

**火焰图**:

```
Run(source)
└─ BMSSP(l=3)
   └─ while (iter-1 to 8)
      ├─ Pull: |S_1| = 8 (max)
      ├─ BMSSP(l=2, B_1, S_1)  ◄──────┐
      │   ├─ FindPivots              │
      │   └─ while (iter-1 to 32)   │
      │      ├─ Pull: |S_1| = 1      │
      │      └─ BMSSP(l=1, B_1, {x}) │
      │         └─ while (iter-1 to 8)│
      │            ├─ Pull: |S_1| = 1 │
      │            └─ BaseCase(B, {x})│ ◄─🔥 P=1 (单节点)
      │                                    │
      ├─ Relax edges ◀────────────────────────┘ 串行依赖
      │
      ├─ Pull: |S_2| = 6 (actual)
      ├─ BMSSP(l=2, B_2, S_2)  ◄──────┐
      │   └─ ... (同上)                │
      │                                │ 🔥 理论 P_max = 8
      ├─ Relax edges ◀──────────────────┘  但实际 P ≈ 4-6
      │
      └─ ...
```

**实际并行度**: 4 - 6 (受 |S_i| 实际大小限制)

### 4.2 案例 2: n = 400 节点网格

**参数**:
```
n = 400
k = ⌊log2(400)^(1/3)⌋ = ⌊5.35⌋ = 5
t = ⌊log2(400)^(2/3)⌋ = ⌊5.69⌋ = 5
l = ⌈log2(400) / 5⌉ = ⌈8.64 / 5⌉ = 2
```

**层级并行度**:

| level | M | 可并行 | 说明 |
|-------|---|--------|------|
| 2 | BMSSP(l=2) 主循环 | ❌ | 串行 |
| 1 | BMSSP(l=1) 主循环 | ⚠️ | S_i 最多 2^0 = 1 个节点 |
| 0 | BaseCase | ✅ | **P_max = 1** (受限) |

**问题**: t=5 较大，导致 l=2 较小，并行度受限

**实际并行度**: 1 - 2 (很低)

### 4.3 案例 3: n = 1000 节点密集图

**参数**:
```
n = 1000
k = ⌊log2(1000)^(1/3)⌋ = ⌊5.48⌋ = 5
t = ⌊log2(1000)^(2/3)⌋ = ⌊6.00⌋ = 6
l = ⌈log2(1000) / 6⌉ = ⌈9.97 / 6⌉ = 2
```

**层级并行度**:

| level | M | 可并行 | 说明 |
|-------|---|--------|------|
| 2 | BMSSP(l=2) 主循环 | ❌ | 串行 |
| 1 | BMSSP(l=1) | 2^0 = 1 | ⚠️ 受限 |
| 0 | BaseCase | k+1 = 6 | **P_max = 6** |

**实际并行度**: 4 - 6

### 4.4 参数敏感性分析

#### n 对并行度的影响

```
n = 100:   k=4, t=3, l=3, P_max=8   ✓ 良好
n = 225:   k=4, t=3, l=3, P_max=8   ✓ 良好
n = 400:   k=5, t=5, l=2, P_max=1   ✗ 较差
n = 1000:  k=5, t=6, l=2, P_max=6   △ 中等
```

**观察**: 并非 n 越大并行度越高

#### 关键参数 t 对并行度的影响

```
P_max = 2^{(l-2)×t}

由于 l = ⌈log2(n) / t⌉，代入得:
P_max = 2^{(⌈log2(n)/t⌉-2)×t}
```

对于固定的 t：
- t 较小 → l 较大 → (l-2)×t 较大 → P_max 较大
- t 较大 → l 较小 → (l-2)×t 较小 → P_max 较小

**最优**: t 较小 (2-4)，l 较大 (3-5)

---

## 5. 并行效率分析

### 5.1 并行效率公式

```
加速比 = T_serial / T_parallel
效率   = 加速比 / 线程数
```

### 5.2 理论加速比分析

#### 串行时间

```
T_serial = T_FindPivots + T_Init + Σ(T_Pull + T_BMSSP + T_Relax + T_BatchPrepend)
```

#### 并行时间 (TLS + 归约)

```
T_parallel = T_FindPivots + T_Init + Σ(T_Pull + T_BMSSP_parallel + T_Relax + T_BatchPrepend + T_Reduce)
```

其中：
```
T_BMSSP_parallel ≈ T_BMSSP / P + T_overhead
T_Reduce ≈ O(n × num_threads)  // 归约开销
```

#### 理论加速比

```
Speedup ≈ P / (1 + f)
其中 f = T_overhead / T_useful
```

对于 TLS + 归约：
- `f` 主要来自归约开销和副本初始化
- `f ≈ 0.1 - 0.3`（估计）

### 5.3 预期性能

| 核心数 | P_max | 预期加速比 | 效率 |
|-------|-------|-----------|------|
| 2 | 4-8 | 1.5x - 1.8x | 75% - 90% |
| 4 | 4-8 | 2.5x - 3.5x | 62% - 87% |
| 8 | 4-8 | 3.5x - 5.5x | 44% - 68% |
| 16 | 4-8 | 4x - 6x | 25% - 37% |

**观察**:
- 当核心数 ≤ P_max 时，效率较高
- 当核心数 > P_max 时，效率下降（资源浪费）

### 5.4 实际并行度估算

#### 最佳线程数

```
num_threads_optimal ≈ min(P_max, num_cores)
```

| 场景 | n | P_max | 最佳线程数 |
|------|---|-------|-----------|
| 小型图 | 100 | 8 | 4 - 8 |
| 中型图 | 225 | 8 | 4 - 8 |
| 大型图 | 400 | 1 - 4 | 2 - 4 |
| 超大图 | 1000 | 6 | 4 - 6 |

#### 动态调整策略

```cpp
int ComputeOptimalThreads(int n)
{
    double logn = std::log2(n);
    int t = std::max(1, (int)std::floor(std::pow(logn, 2.0 / 3.0)));
    int l = std::max(1, (int)std::ceil(logn / t));

    // P_max = 2^{(l-2)×t}
    int P_max = (l >= 2) ? (1 << ((l - 2) * t)) : 1;

    // 限制在合理范围
    int num_cores = std::thread::hardware_concurrency();
    return std::min({P_max, num_cores, 16});  // 最多 16 线程
}
```

---

## 6. 总结与建议

### 6.1 核心发现

1. **并行度范围**: 1 - 128，典型为 4 - 16
2. **关键参数**: P_max = 2^{(l-2)×t}
3. **最佳并行点**: BaseCase 处理多个节点时
4. **串行瓶颈**: 主循环迭代之间、FindPivots

### 6.2 TLS + 归约方案评价

| 指标 | 评分 | 说明 |
|------|------|------|
| 并行度潜力 | ⭐⭐⭐⭐ | 中等，4-16 个线程 |
| 实现复杂度 | ⭐⭐⭐⭐⭐ | 低 |
| 扩展性 | ⭐⭐⭐ | 中等，受 P_max 限制 |
| 性能预期 | ⭐⭐⭐⭐ | 3x - 6x 加速 |

### 6.3 实现建议

1. **动态线程数**: 根据图大小自动调整
   ```cpp
   int num_threads = ComputeOptimalThreads(n);
   ```

2. **任务粒度**: 如果 |S_i| < num_threads，减少线程数
   ```cpp
   num_threads = std::min(num_threads, (int)S_i.size());
   ```

3. **监控与调优**: 记录实际并行度，动态优化

### 6.4 进一步优化方向

1. **FindPivots 并行化**: DFS 计算树大小可并行
2. **边松弛并行化**: 使用 TLS + 归约
3. **流水线并行**: 重叠计算和通信

---

**文档版本**: 1.0
**最后更新**: 2025-12-31
**作者**: Breaking 算法 v4 设计组
**状态**: 待审核

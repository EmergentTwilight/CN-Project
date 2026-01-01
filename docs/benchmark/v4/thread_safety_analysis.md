# Breaking 算法 v4：并行 BaseCase 线程安全分析

**日期**: 2025-12-31
**版本**: v4 并行化设计
**状态**: 线程安全分析与设计

---

## 概要

本文档详细分析了 Breaking 算法在并行化 BaseCase 操作时可能遇到的线程安全问题，并提供了相应的解决方案。这是实现 v4 并行优化前的关键设计文档。

**目标**: 在 FindPivots 中并行执行多个 BaseCase 调用，实现 2x - 4x 加速。

---

## 目录

1. [当前代码结构分析](#1-当前代码结构分析)
2. [线程安全问题识别](#2-线程安全问题识别)
3. [数据竞争详细分析](#3-数据竞争详细分析)
4. [解决方案设计](#4-解决方案设计)
5. [实现方案选择](#5-实现方案选择)
6. [验证与测试](#6-验证与测试)

---

## 1. 当前代码结构分析

### 1.1 BmsspSolver 类成员变量

```cpp
class BmsspSolver {
private:
    int n;                              // 图节点数
    int k, t, l;                        // 算法参数

    // 图数据结构（只读）
    std::vector<int> head;              // 邻接表头指针
    std::vector<BmsspEdge> edge;        // 边数组
    int num_edge;

    // 共享可变状态（线程安全问题）
    std::vector<BmsspLength> dis;       // 距离数组
    std::vector<int> parent;            // 父节点数组

    int m_source;                       // 源节点

    // 内部算法函数
    void ComputeParameters();
    std::pair<VertexSet, VertexSet> FindPivots(...);
    std::pair<BmsspLength, VertexSet> BaseCase(...);
    std::pair<BmsspLength, VertexSet> BMSSP(...);
};
```

### 1.2 BaseCase 函数分析

**函数签名**:
```cpp
std::pair<BmsspLength, VertexSet> BaseCase(BmsspLength B, const VertexSet& S);
```

**关键代码段**:
```cpp
// bmssp.cc:270-375
std::pair<BmsspLength, VertexSet>
BmsspSolver::BaseCase(BmsspLength B, const VertexSet& S)
{
    VertexSet U;                        // 局部变量
    int x = S[0];

    // 优先队列（局部）
    std::priority_queue<P, std::vector<P>, std::greater<P>> pq;
    std::unordered_set<int> inHeap;

    pq.push({dis[x], x});               // 读取共享 dis[x]
    inHeap.insert(x);

    // Dijkstra 主循环
    while (!pq.empty() && (int)U.size() < k + 1)
    {
        auto [d, u] = pq.top();
        pq.pop();

        if (d != dis[u])               // 检查共享 dis[u]
            continue;

        U.push_back(u);                // 局部写入

        // 边松弛 - 关键区域
        for (int ei = head[u]; ei; ei = edge[ei].next)
        {
            int v = edge[ei].to;
            int w = edge[ei].weight;

            // 读取共享 dis[u], dis[v]
            if (dis[u] != BMSSP_INF && dis[u] + w <= dis[v] && dis[u] + w < B)
            {
                BmsspLength newDist = dis[u] + w;

                // 写入共享 dis[v], parent[v]
                if (newDist < dis[v])
                {
                    dis[v] = newDist;      // ⚠️ 竞争写入
                    parent[v] = u;         // ⚠️ 竞争写入
                }

                // 局部优先队列操作
                if (inHeap.find(v) == inHeap.end())
               []                    {
                    pq.push({dis[v], v});  // ⚠️ 读取共享 dis[v]
                    inHeap.insert(v);
                }
                else
                {
                    pq.push({dis[v], v});  // ⚠️ 读取共享 dis[v]
                }
            }
        }
    }

    // 后处理：读取共享 dis[]
    BmsspLength B_prime = 0;
    for (int v : U)
    {
        if (dis[v] > B_prime)            // ⚠️ 读取共享 dis[v]
            B_prime = dis[v];
    }

    return {B_prime, result};
}
```

### 1.3 FindPivots 调用模式

```cpp
// bmssp.cc:133-265
std::pair<VertexSet, VertexSet>
BmsspSolver::FindPivots(BmsspLength B, const VertexSet& S)
{
    // ... Bellman-Ford 迭代 ...

    // 计算树大小
    std::unordered_map<int, int> treeSize;

    std::function<int(int)> dfs = [&](int u) -> int {
        int size = 1;
        for (int v : children[u])
        {
            size += dfs(v);               // 递归调用
        }
        treeSize[u] = size;
        return size;
    };

    for (int r : roots)
    {
        dfs(r);                          // 串行执行
    }

    // 收集 pivots: roots with tree size >= k
    VertexSet P;
    for (int r : roots)
    {
        if (inS.count(r) && treeSize[r] >= k)
        {
            P.push_back(r);
        }
    }

    return {P, W};
}
```

**并行化机会**: 在 BMSSP 主循环中，每次 Pull 返回的 S_i 包含多个 pivot，这些 pivot 的 BaseCase 调用是独立的：

```cpp
// bmssp.cc:419-493 (BMSSP 主循环)
while ((int)U.size() < k * (1 << (level * t)) && !D.IsEmpty())
{
    i++;

    // Pull from D
    auto [S_i, B_i] = D.Pull();          // S_i 可能包含多个节点

    // 当前: 串行递归
    auto [B_prime_i_curr, U_i] = BMSSP(level - 1, B_i, S_i);

    // 🎯 并行化机会: S_i 中每个节点可独立执行 BaseCase
}
```

---

## 2. 线程安全问题识别

### 2.1 问题分类

| 问题类型 | 严重程度 | 位置 | 影响 |
|---------|---------|------|------|
| **数据竞争** | 🔴 严重 | `dis[v]`, `parent[v]` | 错误结果 |
| **丢失更新** | 🔴 严重 | 边松弛操作 | 非最优解 |
| **优先队列非线程安全** | 🟡 中等 | `std::priority_queue` | 崩溃 |
| **内存可见性** | 🟡 中等 | 共享变量读写 | 不一致状态 |
| **死锁风险** | 🟢 低 | 锁设计不当 | 性能下降/挂起 |

### 2.2 关键竞争点

#### 竞争点 1: `dis[]` 数组写入

```cpp
// 多个线程可能同时执行
if (newDist < dis[v])      // ⚠️ 读-检查-写 非原子
{
    dis[v] = newDist;      // ⚠️ 竞争写入
    parent[v] = u;         // ⚠️ 竞争写入
}
```

**问题**:
1. **TOCTOU (Time-of-check-time-of-use)**: 检查和写入之间可能被其他线程修改
2. **丢失更新**: 两个线程同时发现更优路径，但只有一个生效
3. **非原子更新**: 64 位 `BmsspLength` 可能不是原子操作

#### 竞争点 2: `parent[]` 数组写入

```cpp
parent[v] = u;             // ⚠️ 必须与 dis[v] 同步更新
```

**问题**:
1. **不一致状态**: `dis[v]` 和 `parent[v]` 可能不匹配
2. **路径追踪错误**: GetNextHop 可能返回错误路径

#### 竞争点 3: `dis[]` 数组读取

```cpp
if (d != dis[u])           // ⚠️ 读取可能与写入并发
    continue;

BmsspLength newDist = dis[u] + w;  // ⚠️ 读取可能与写入并发
```

**问题**:
1. **脏读**: 可能读取到部分更新的值
2. **不一致状态**: 读取到 `dis[u]` 但 `dis[v]` 已过期

#### 竞争点 4: 优先队列与 `dis[]` 交互

```cpp
pq.push({dis[v], v});      // ⚠️ dis[v] 可能在读取后被修改
```

**问题**:
1. **过期条目**: 优先队列中的距离值可能与 `dis[]` 不一致
2. **无效检查**: `if (d != dis[u])` 可能误判

---

## 3. 数据竞争详细分析

### 3.1 场景 1: 两个线程松弛到同一节点

**初始状态**:
```
dis[v] = 100
parent[v] = old_parent
```

**线程 A 执行**:
```cpp
// Thread A: u1 -> v, w1 = 20
BmsspLength newDist_A = dis[u1] + 20;  // 假设 = 80
if (newDist_A < dis[v])  // 80 < 100 ✓
{
    // 被中断...
}
```

**线程 B 执行**:
```cpp
// Thread B: u2 -> v, w2 = 15
BmsspLength newDist_B = dis[u2] + 15;  // 假设 = 70
if (newDist_B < dis[v])  // 70 < 100 ✓
{
    dis[v] = 70;         // ✓ 更新成功
    parent[v] = u2;      // ✓ 更新成功
}
```

**线程 A 继续**:
```cpp
    dis[v] = 80;         // ✗ 错误！覆盖了更优的 70
    parent[v] = u1;      // ✗ 与 dis[v] 不一致
}
```

**结果**: 错误的最终状态
```
dis[v] = 80      (应该是 70)
parent[v] = u1   (应该是 u2)
```

### 3.2 场景 2: TOCTOU 竞争条件

**代码**:
```cpp
if (newDist < dis[v])     // 检查点
{
    // ... 其他操作 ...
    dis[v] = newDist;     // 使用点
}
```

**时间线**:

| 时间 | 线程 A | 线程 B | dis[v] |
|------|--------|--------|--------|
| T1 | 检查: 80 < 100 ✓ | 等待 | 100 |
| T2 | 被中断 | 检查: 70 < 100 ✓ | 100 |
| T3 | 等待 | 更新: dis[v] = 70 | 70 |
| T4 | 更新: dis[v] = 80 | 完成 | 80 |

**结果**: 更优的距离 70 被覆盖为 80

### 3.3 场景 3: 优先队列过期条目

**代码**:
```cpp
pq.push({dis[v], v});     // dis[v] = 80 时推入
// ... 其他线程更新 dis[v] = 60 ...
// 从 pq 弹出时
auto [d, u] = pq.top();   // d = 80 (过期)
if (d != dis[u])          // 80 != 60, 跳过
    continue;
```

**问题**: 大量过期条目导致性能下降

---

## 4. 解决方案设计

### 4.1 方案概述

| 方案 | 原子操作 | 锁 | 线程局部存储 | 性能 | 复杂度 |
|------|---------|-----|-------------|------|--------|
| **A: 粗粒度锁** | ✗ | ✅ | ✗ | 低 | 低 |
| **B: 细粒度锁** | ✗ | ✅ | ✗ | 中 | 中 |
| **C: 原子操作** | ✅ | ✗ | ✗ | 高 | 高 |
| **D: TLS + 归约** | ✅ | ✗ | ✅ | **最高** | 中 |
| **E: 无锁算法** | ✅ | ✗ | 部分 | 最高 | 最高 |

### 4.2 方案 A: 粗粒度互斥锁

**设计**:
```cpp
class BmsspSolver {
private:
    std::mutex globalMutex;  // 全局锁

    std::pair<BmsspLength, VertexSet> BaseCase(BmsspLength B, const VertexSet& S)
    {
        VertexSet U;
        // ...

        while (!pq.empty() && (int)U.size() < k + 1)
        {
            auto [d, u] = pq.top();
            pq.pop();

            // 🔒 加锁保护共享状态
            {
                std::lock_guard<std::mutex> lock(globalMutex);

                if (d != dis[u])
                    continue;

                U.push_back(u);

                for (int ei = head[u]; ei; ei = edge[ei].next)
                {
                    int v = edge[ei].to;
                    int w = edge[ei].weight;

                    if (dis[u] != BMSSP_INF && dis[u] + w <= dis[v] && dis[u] + w < B)
                    {
                        BmsspLength newDist = dis[u] + w;
                        if (newDist < dis[v])
                        {
                            dis[v] = newDist;
                            parent[v] = u;
                        }

                        if (inHeap.find(v) == inHeap.end())
                        {
                            pq.push({dis[v], v});
                            inHeap.insert(v);
                        }
                        else
                        {
                            pq.push({dis[v], v});
                        }
                    }
                }
            }
            // 🔒 解锁
        }

        return {B_prime, result};
    }
};
```

**优点**:
- ✅ 实现简单
- ✅ 保证正确性
- ✅ 无死锁风险

**缺点**:
- ❌ 串行化执行，性能差
- ❌ 扩展性差
- ❌ 基本无并行加速

**预期性能**: 0.8x - 1.2x（可能比单线程更慢）

---

### 4.3 方案 B: 细粒度锁（节点级锁）

**设计**:
```cpp
class BmsspSolver {
private:
    std::vector<std::mutex> nodeMutexes;  // 每个节点一个锁

    std::pair<BmsspLength, VertexSet> BaseCase(BmsspLength B, const VertexSet& S)
    {
        // ...

        while (!pq.empty() && (int)U.size() < k + 1)
        {
            auto [d, u] = pq.top();
            pq.pop();

            // 🔒 锁定源节点 u
            std::lock_guard<std::mutex> lockU(nodeMutexes[u]);

            if (d != dis[u])
                continue;

            U.push_back(u);

            for (int ei = head[u]; ei; ei = edge[ei].next)
            {
                int v = edge[ei].to;
                int w = edge[ei].weight;

                // 🔒 锁定目标节点 v
                std::lock_guard<std::mutex> lockV(nodeMutexes[v]);

                // 为避免死锁，始终按节点顺序加锁
                // std::lock(nodeMutexes[u], nodeMutexes[v]);

                if (dis[u] != BMSSP_INF && dis[u] + w <= dis[v] && dis[u] + w < B)
                {
                    BmsspLength newDist = dis[u] + w;
                    if (newDist < dis[v])
                    {
                        dis[v] = newDist;
                        parent[v] = u;
                    }

                    // 优先队列操作仍需要锁
                    // ...
                }
            }
        }

        return {B_prime, result};
    }
};
```

**优点**:
- ✅ 并行度更高
- ✅ 减少锁竞争

**缺点**:
- ❌ 实现复杂
- ❌ 死锁风险
- ❌ 锁开销大
- ❌ 优先队列仍需全局锁

**预期性能**: 1.5x - 2.5x

---

### 4.4 方案 C: 原子操作（CAS 循环）

**设计**:
```cpp
#include <atomic>

class BmsspSolver {
private:
    std::vector<std::atomic<BmsspLength>> dis_atomic;
    std::vector<std::atomic<int>> parent_atomic;

    std::pair<BmsspLength, VertexSet> BaseCase(BmsspLength B, const VertexSet& S)
    {
        // ...

        while (!pq.empty() && (int)U.size() < k + 1)
        {
            auto [d, u] = pq.top();
            pq.pop();

            // 原子读取
            BmsspLength current_dis_u = dis_atomic[u].load(std::memory_order_acquire);
            if (d != current_dis_u)
                continue;

            U.push_back(u);

            for (int ei = head[u]; ei; ei = edge[ei].next)
            {
                int v = edge[ei].to;
                int w = edge[ei].weight;

                BmsspLength dis_u = dis_atomic[u].load(std::memory_order_acquire);
                if (dis_u != BMSSP_INF)
                {
                    BmsspLength newDist = dis_u + w;

                    // 原子 CAS 循环
                    BmsspLength old_dis_v = dis_atomic[v].load(std::memory_order_acquire);
                    while (newDist < old_dis_v && newDist < B)
                    {
                        // 尝试原子更新
                        if (dis_atomic[v].compare_exchange_weak(
                                old_dis_v, newDist,
                                std::memory_order_acq_rel,
                                std::memory_order_acquire))
                        {
                            // 成功更新 dis[v]
                            parent_atomic[v].store(u, std::memory_order_release);

                            // 添加到优先队列
                            if (inHeap.find(v) == inHeap.end())
                            {
                                pq.push({newDist, v});
                                inHeap.insert(v);
                            }
                            else
                            {
                                pq.push({newDist, v});
                            }
                            break;
                        }
                        // CAS 失败，重新读取 dis[v]
                        old_dis_v = dis_atomic[v].load(std::memory_order_acquire);
                    }
                }
            }
        }

        return {B_prime, result};
    }
};
```

**优点**:
- ✅ 无锁设计
- ✅ 高并发性能
- ✅ 保证正确性

**缺点**:
- ❌ CAS 失败重试开销
- ❌ 内存顺序复杂
- ❌ ABA 问题风险
- ❌ 优先队列仍需线程局部

**预期性能**: 2.5x - 4x

---

### 4.5 方案 D: 线程局部存储 + 归约（推荐）

**核心思想**:
1. 每个线程维护自己的 `dis_local` 和 `parent_local` 副本
2. 并行执行边松弛，只写入局部副本
3. 最后合并结果，取最优值

**设计**:
```cpp
class BmsspSolver {
private:
    // 主数据结构
    std::vector<BmsspLength> dis;
    std::vector<int> parent;

    // 线程局部数据
    struct ThreadLocalData {
        std::vector<BmsspLength> dis_local;
        std::vector<int> parent_local;
        std::priority_queue<P, std::vector<P>, std::greater<P>> pq_local;
        std::unordered_set<int> inHeap_local;
        VertexSet U_local;
    };

    std::pair<BmsspLength, VertexSet> ParallelBaseCase(
        BmsspLength B,
        const VertexSet& S,
        int numThreads)
    {
        // 初始化线程局部数据
        std::vector<ThreadLocalData> threadData(numThreads);
        for (auto& td : threadData)
        {
            td.dis_local.assign(dis.begin(), dis.end());  // 复制初始状态
            td.parent_local.assign(parent.begin(), parent.end());
        }

        // 获取起始节点
        int x = S[0];

        // 并行初始化优先队列
        #pragma omp parallel for
        for (int tid = 0; tid < numThreads; tid++)
        {
            auto& td = threadData[tid];
            td.pq_local.push({td.dis_local[x], x});
            td.inHeap_local.insert(x);
        }

        // 并行 Dijkstra 主循环
        bool allEmpty = false;
        while (!allEmpty)
        {
            // 每个线程处理自己的优先队列
            #pragma omp parallel for schedule(dynamic)
            for (int tid = 0; tid < numThreads; tid++)
            {
                auto& td = threadData[tid];

                if (td.pq_local.empty() || td.U_local.size() >= k + 1)
                    continue;

                auto [d, u] = td.pq_local.top();
                td.pq_local.pop();

                if (d != td.dis_local[u])
                    continue;

                td.U_local.push_back(u);

                // 边松弛（只写入线程局部数据）
                for (int ei = head[u]; ei; ei = edge[ei].next)
                {
                    int v = edge[ei].to;
                    int w = edge[ei].weight;

                    if (td.dis_local[u] != BMSSP_INF &&
                        td.dis_local[u] + w <= td.dis_local[v] &&
                        td.dis_local[u] + w < B)
                    {
                        BmsspLength newDist = td.dis_local[u] + w;
                        if (newDist < td.dis_local[v])
                        {
                            td.dis_local[v] = newDist;
                            td.parent_local[v] = u;
                        }

                        if (td.inHeap_local.find(v) == td.inHeap_local.end())
                        {
                            td.pq_local.push({td.dis_local[v], v});
                            td.inHeap_local.insert(v);
                        }
                        else
                        {
                            td.pq_local.push({td.dis_local[v], v});
                        }
                    }
                }
            }

            // 检查是否所有线程都完成
            allEmpty = true;
            for (const auto& td : threadData)
            {
                if (!td.pq_local.empty() && td.U_local.size() < k + 1)
                {
                    allEmpty = false;
                    break;
                }
            }
        }

        // 归约阶段：合并所有线程的结果
        BmsspLength B_prime = B;
        VertexSet U_merged;

        for (const auto& td : threadData)
        {
            // 合并 U 集合
            for (int v : td.U_local)
            {
                bool alreadyIn = false;
                for (int u : U_merged)
                {
                    if (u == v)
                    {
                        alreadyIn = true;
                        break;
                    }
                }
                if (!alreadyIn)
                {
                    U_merged.push_back(v);
                }
            }

            // 合并距离更新（取最小值）
            for (size_t i = 0; i < dis.size(); i++)
            {
                if (td.dis_local[i] < dis[i])
                {
                    dis[i] = td.dis_local[i];
                    parent[i] = td.parent_local[i];
                }
            }

            // 更新 B_prime
            for (int v : td.U_local)
            {
                if (td.dis_local[v] > B_prime)
                {
                    B_prime = td.dis_local[v];
                }
            }
        }

        return {B_prime, U_merged};
    }
};
```

**优点**:
- ✅ **零锁设计**：完全避免锁竞争
- ✅ **缓存友好**：线程局部数据亲和性好
- ✅ **扩展性好**：线性扩展到多核
- ✅ **正确性保证**：归约阶段保证最优解

**缺点**:
- ❌ 内存开销：每个线程一份副本
- ❌ 归约开销：需要合并结果
- ❌ 初始化开销：需要复制初始状态

**预期性能**: 3x - 6x（取决于核心数）

---

### 4.6 方案 E: 无锁算法（无锁队列 + 原子操作）

**设计**:
```cpp
#include <atomic>
#include <lockfree_queue.h>

class BmsspSolver {
private:
    std::vector<std::atomic<BmsspLength>> dis_atomic;
    std::vector<std::atomic<int>> parent_atomic;

    // 使用无锁优先队列（需要第三方库或自行实现）
    // 例如: folly::PriorityQueue 或 boost::lockfree::queue

    std::pair<BmsspLength, VertexSet> LockFreeBaseCase(
        BmsspLength B,
        const VertexSet& S)
    {
        // 使用无锁优先队列
        folly::PriorityQueue<PrioNode> pq;

        int x = S[0];
        pq.push({dis_atomic[x].load(), x});

        VertexSet U;

        while (!pq.empty() && (int)U.size() < k + 1)
        {
            auto [d, u] = pq.pop();

            // 原子读取 + 过期检查
            BmsspLength current_dis = dis_atomic[u].load(std::memory_order_acquire);
            if (d != current_dis)
                continue;

            U.push_back(u);

            // 边松弛
            for (int ei = head[u]; ei; ei = edge[ei].next)
            {
                int v = edge[ei].to;
                int w = edge[ei].weight;

                BmsspLength dis_u = dis_atomic[u].load(std::memory_order_acquire);
                if (dis_u != BMSSP_INF)
                {
                    BmsspLength newDist = dis_u + w;

                    if (newDist < B)
                    {
                        // 原子更新 dis[v]
                        UpdateDistanceAtomic(v, newDist, u);

                        // 无锁推入优先队列
                        pq.push({newDist, v});
                    }
                }
            }
        }

        return {B_prime, U};
    }

    void UpdateDistanceAtomic(int v, BmsspLength newDist, int parent)
    {
        BmsspLength oldDist = dis_atomic[v].load(std::memory_order_acquire);

        while (newDist < oldDist)
        {
            if (dis_atomic[v].compare_exchange_weak(
                    oldDist, newDist,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire))
            {
                parent_atomic[v].store(parent, std::memory_order_release);
                return;
            }
        }
    }
};
```

**优点**:
- ✅ 完全无锁
- ✅ 最高并发性能
- ✅ 无死锁风险

**缺点**:
- ❌ 实现极度复杂
- ❌ 需要无锁优先队列（非标准库）
- ❌ ABA 问题需要处理
- ❌ 内存回收复杂（ hazard pointers）

**预期性能**: 4x - 8x（但实现难度极高）

---

## 5. 实现方案选择

### 5.1 方案对比总结

| 方案 | 性能 | 复杂度 | 风险 | 推荐度 |
|------|------|--------|------|--------|
| A: 粗粒度锁 | ⭐ | ⭐ | 低 | ❌ |
| B: 细粒度锁 | ⭐⭐ | ⭐⭐⭐ | 中 | ❌ |
| C: 原子操作 | ⭐⭐⭐⭐ | ⭐⭐⭐⭐ | 中 | ⭐⭐⭐ |
| **D: TLS + 归约** | ⭐⭐⭐⭐⭐ | ⭐⭐⭐ | 低 | **⭐⭐⭐⭐⭐** |
| E: 无锁算法 | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ | 高 | ⭐⭐ |

### 5.2 推荐方案：D (TLS + 归约)

**选择理由**:

1. **性能优异**: 预期 3x - 6x 加速
2. **实现可行**: 复杂度可控
3. **正确性保证**: 归约阶段明确合并策略
4. **扩展性好**: 线性扩展到多核
5. **调试友好**: 线程局部数据易于调试

### 5.3 实现路线

**阶段 1: 基础设施 (1 天)**
- [ ] 添加 ThreadLocalData 结构
- [ ] 实现 OpenMP 集成
- [ ] 添加线程数配置

**阶段 2: 并行 BaseCase (2 天)**
- [ ] 实现线程局部初始化
- [ ] 实现并行 Dijkstra 循环
- [ ] 实现结果归约

**阶段 3: 测试验证 (1 天)**
- [ ] 正确性测试
- [ ] 性能测试
- [ ] 边界情况测试

---

## 6. 验证与测试

### 6.1 正确性验证

**测试用例**:

1. **基本正确性**: 与串行版本结果一致
2. **边界条件**:
   - 空图
   - 单节点图
   - 完全图
3. **并发压力**:
   - 大量线程同时松弛
   - 高竞争场景
4. **随机测试**:
   - 随机图拓扑
   - 随机权重

**验证方法**:
```cpp
void TestParallelCorrectness()
{
    // 串行版本
    BmsspSolver serialSolver;
    serialSolver.Init(n);
    // ... 添加边 ...
    serialSolver.Run(source);

    // 并行版本
    BmsspSolver parallelSolver;
    parallelSolver.Init(n);
    // ... 添加相同边 ...
    parallelSolver.RunParallel(source, numThreads);

    // 验证结果一致
    for (int i = 0; i < n; i++)
    {
        ASSERT_EQ(serialSolver.GetDistance(i),
                  parallelSolver.GetDistance(i));
    }
}
```

### 6.2 性能验证

**指标**:
1. **加速比**: `T_serial / T_parallel`
2. **效率**: `加速比 / 线程数`
3. **扩展性**: 不同核心数下的性能

**预期结果**:

| 核心数 | 预期加速比 | 效率 |
|-------|-----------|------|
| 2 | 1.6x - 1.8x | 80% - 90% |
| 4 | 2.8x - 3.5x | 70% - 87% |
| 8 | 4.5x - 6.0x | 56% - 75% |

### 6.3 线程安全测试

**工具**:
- ThreadSanitizer (TSan): 检测数据竞争
- Helgrind: 检测锁问题
- DRD: 检测死锁

**编译命令**:
```bash
g++ -fsanitize=thread -g -O2 bmssp.cc -o bmssp_tsan
./bmssp_tsan  # 运行测试
```

---

## 7. 风险与缓解

### 7.1 已识别风险

| 风险 | 影响 | 概率 | 缓解措施 |
|------|------|------|---------|
| 内存不足 | 高 | 中 | 限制线程数，使用动态分配 |
| 性能不达预期 | 中 | 低 | 基准测试，调优参数 |
| 调试困难 | 中 | 高 | 详细日志，线程感知调试器 |
| 平台兼容性 | 低 | 中 | 使用标准 OpenMP |

### 7.2 回退方案

如果 TLS + 归约方案遇到问题：
1. **短期**: 使用方案 C (原子操作)
2. **中期**: 使用方案 B (细粒度锁)
3. **长期**: 考虑无锁库 (如 folly)

---

## 8. 总结

### 8.1 关键发现

1. **主要竞争点**: `dis[]` 和 `parent[]` 数组的读写
2. **TOCTOU 风险**: 检查-使用模式存在竞争窗口
3. **优先队列问题**: 标准库容器非线程安全
4. **最佳方案**: TLS + 归约平衡性能和复杂度

### 8.2 下一步

1. ✅ 实现 ThreadLocalData 结构
2. ✅ 添加 OpenMP 并行支持
3. ✅ 实现结果归约逻辑
4. ✅ 运行正确性和性能测试
5. ✅ 性能调优

---

**文档版本**: 1.0
**最后更新**: 2025-12-31
**作者**: Breaking 算法 v4 设计组
**状态**: 待审核

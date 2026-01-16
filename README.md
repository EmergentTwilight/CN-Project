# Breaking Algorithm Implementation in NS-3

## 项目简介

本项目实现了 **Breaking (BMSSP)** 算法并将其集成到 NS-3 网络模拟器中，作为对 NS-3 原生 Dijkstra 最短路径算法的替代方案。Breaking 算法通过创新的层级分块策略，在稠密图和大规模网络场景下具有显著的性能优势。

### 算法背景

**Breaking 算法**（BMSSP: Breaking the Sorting Barrier for Directed Single-Source Shortest Paths）是一种突破性的最短路径算法，通过以下创新实现性能提升：

- **层级分块结构**：将顶点按距离分层，每层内部使用桶排序
- **BlockHeap 数据结构**：优化的优先队列，减少排序开销
- **时间复杂度**：O(m + n^1.5)，在稠密图上显著优于传统 Dijkstra
- **空间复杂度**：O(m + n log n)

---

## 项目结构

```
CN-Project/
├── ns-allinone-3.46.1/
│   └── ns-3.46.1/
│       ├── src/internet/model/
│       │   ├── global-route-manager-impl.cc    # NS-3 路由管理器实现
│       │   ├── bmssp.h                          # Breaking 算法头文件
│       │   ├── bmssp.cc                        # Breaking 算法实现
│       │   └── candidate-queue.h/cc            # Dijkstra 优先队列（链表实现）
│       └── scratch/
│           ├── test-correctness.cc            # 实验1: 正确性验证
│           ├── test-scalability.cc            # 实验2: 节点规模扩展性
│           ├── test-density.cc                 # 实验3: 图稠密度影响
│           ├── test-topology.cc               # 实验4: 拓扑结构类型
│           ├── test-realistic.cc              # 实验5: 真实网络场景
│           └── test-large-scale.cc            # 大规模性能测试
├── results/                                  # 测试结果输出目录
│   ├── breaking/
│   └── dijkstra/
├── docs/                                     # 项目文档
│   ├── benchmark/
│   └── project_requirements.md
├── Makefile                                  # 自动化构建和测试
└── README.md                                 # 本文件
```

---

## 核心实现

### 1. Breaking 算法集成 (`bmssp.h` / `bmssp.cc`)

**主要组件**：

```cpp
// BlockHeap: Breaking 算法的核心数据结构
template<typename K, typename V>
class BlockHeap {
    // D0MinValues: 存储最小距离的桶
    // D1MinSets: 存储次小值的集合
    // 支持高效的 Insert、ExtractMin、DecreaseKey 操作
};

// BmsspSolver: 算法执行器
class BmsspSolver {
    void Run(int sourceNode);  // 执行单源最短路径计算

private:
    int k;  // 层级参数: ⌊log₂(n)^(1/3)⌋
    int t;  // 水平参数: ⌊log₂(n)^(2/3)⌋
    int l;  // 层数参数: ⌈log₂(n)/t⌉
};
```

**算法流程**：
1. **初始化**：设置源节点距离为 0，其他节点为无穷大
2. **分层处理**：按距离范围将顶点分配到不同层级
3. **块内处理**：每层使用桶排序处理顶点
4. **路径重构**：回溯构建最短路径树

### 2. 算法切换机制

通过 `global-route-manager-impl.cc` 中的布尔标志切换算法：

```cpp
// GlobalRouteManagerImpl 类成员
bool m_useBmssp = true;  // true=Breaking, false=Dijkstra

void GlobalRouteManagerImpl::SPFCalculate(Ipv4Address root) {
    if (m_useBmssp) {
        SPFCalculateBMSSP(root);  // Breaking 算法
        return;
    }
    // 原有 Dijkstra 算法...
}
```

### 3. NS-3 Dijkstra 实现分析

NS-3 原生 Dijkstra 使用 `std::list` 作为优先队列：

| 操作 | 复杂度 | 说明 |
|------|--------|------|
| Push | O(n) | 链表查找插入位置 |
| Pop | O(1) | 删除表头 |
| Find | O(n) | 线性搜索 |
| Reorder | O(n log n) | 链表排序 |

**总复杂度**：O(mn log n) 或稠密图上 O(n³ log n)

Breaking 的优势：
- **Push**: O(1) 均摊
- **ExtractMin**: O(n^1/3)
- **DecreaseKey**: O(log n)

---

## 实验设计

### 实验 1: 正确性验证 (`test-correctness.cc`)

**目的**：验证 Breaking 算法与 Dijkstra 计算结果完全一致

**测试场景**：
- 5×5 网格图 (25 节点)
- 10×10 网格图 (100 节点)
- 星形图 (20 节点)
- 完全图 (10 节点)

**评估指标**：
- 路径正确性 (PASS/FAIL)
- 执行时间
- 运行次数与标准差

---

### 实验 2: 节点规模扩展性 (`test-scalability.cc`)

**目的**：分析算法性能随网络规模增长的扩展性

**测试场景**：
- 5×5 → 20×20 网格图 (25 → 400 节点)

**评估指标**：
- 时间复杂度增长趋势
- 每节点平均处理时间
- 边密度影响

---

### 实验 3: 图稠密度影响 (`test-density.cc`)

**目的**：分析边密度对算法性能的影响

**测试场景** (n=100)：
- 稀疏图 (m ≈ 1.5n)
- 中等图 (m ≈ 3n)
- 较密图 (m ≈ 5n)
- 密集图 (m ≈ 10n)
- 超密图 (m ≈ 20n)

**评估指标**：
- 不同密度下的执行时间
- Breaking 优势随密度的变化

---

### 实验 4: 拓扑结构类型 (`test-topology.cc`)

**目的**：分析不同网络拓扑对算法性能的影响

**测试场景** (n=100)：
- 网格图 (2D 规则)
- 随机图 (Erdős-Rényi)
- 星形图 (中心密集)
- 树状图 (无环结构)

**评估指标**：
- 不同拓扑下的性能表现
- 算法稳定性

---

### 实验 5: 真实网络场景 (`test-realistic.cc`)

**目的**：验证算法在实际网络场景中的表现

**测试场景**：
- **数据中心 Fat-Tree** (k=4): 典型叶脊网络，高连接度
- **校园网层级**: 三层架构 (Core-Building-Floor)
- **ISP 骨干网**: 高连通网状结构

**评估指标**：
- 实际场景适用性
- 大规模网络性能

---

### 大规模测试 (`test-large-scale.cc`)

**测试规模**：625 → 22,500 节点

**拓扑类型**：
- Grid (2D 网格)
- Random (随机图)
- FatTree (数据中心)
- SmallWorld (小世界网络)
- ScaleFree (无标度网络)

**监控指标**：
- 执行时间与内存使用
- 超时保护机制
- 峰值内存监控

---

## 构建与测试

### 快速开始

```bash
# 1. 配置 NS-3 (首次运行)
make configure

# 2. 编译 Breaking 算法
make breaking

# 3. 运行所有测试
make test-all

# 4. 切换到 Dijkstra 算法
make dijkstra

# 5. 再次运行测试进行对比
make test-all

# 6. 查看结果
ls results/breaking/
ls results/dijkstra/
```

### 可用命令

| 命令 | 说明 |
|------|------|
| `make breaking` | 编译 Breaking 算法 |
| `make dijkstra` | 编译 Dijkstra 算法 |
| `make test-correctness` | 运行正确性验证 |
| `make test-scalability` | 运行规模扩展性测试 |
| `make test-density` | 运行图稠密度测试 |
| `make test-topology` | 运行拓扑类型测试 |
| `make test-realistic` | 运行真实场景测试 |
| `make test-all` | 运行所有实验 |
| `make test-large-scale-quick` | 快速大规模测试 (3-5分钟) |
| `make test-large-scale` | 完整大规模测试 (数小时) |

---

## 测试结果

### 性能对比总结

| 场景 | 节点数 | Breaking | Dijkstra | 加速比 |
|------|--------|---------|----------|--------|
| 网格图 | 625 | 21.5 ms | 127.5 ms | **5.9x** |
| 网格图 | 900 | 55.4 ms | 374.6 ms | **6.8x** |
| 网格图 | 1225 | 129.0 ms | - | - |
| 数据中心 | 24 | 0.9 ms | 3.1 ms | **3.4x** |
| 校园网 | 27 | 1.5 ms | 5.7 ms | **3.8x** |
| ISP 骨干网 | 20 | 0.4 ms | 1.2 ms | **3.0x** |

### 关键发现

1. **正确性验证**：Breaking 与 Dijkstra 在所有测试场景下计算结果 100% 一致

2. **性能优势**：
   - 小规模网络 (≤100 节点): Breaking 略快或持平
   - 中等规模 (100-1000 节点): Breaking 明显领先
   - 大规模网络 (≥1000 节点): Breaking 显著优势

3. **密度影响**：图越稠密，Breaking 优势越明显

4. **拓扑影响**：
   - 规则拓扑 (网格/树): 差距较小
   - 复杂拓扑 (随机/数据中心): Breaking 稳定领先

---

## 技术细节

### 算法参数

Breaking 算法的参数根据图规模动态调整：

```cpp
// 对于 n 个节点的图
k = floor(log2(n)^(1/3));  // 层级参数
t = floor(log2(n)^(2/3));  // 水平参数
l = ceil(log2(n) / t);      // 层数参数
```

### 内存优化 (v6 版本)

实现了 **NodePool** 内存池优化：
- 块大小: 4KB
- 每块可容纳 ~100+ 节点
- 减少动态内存分配开销

### BlockHeap 数据结构

```cpp
// D0: 最小距离桶，O(1) 插入
std::vector<V> D0MinValues;

// D1: 次小值集合，O(log n) 操作
using D1MinSets = std::set<std::pair<K, BlockHeapBlock<K, V>*>>;
```

---

## 文件说明

### 核心实现文件

| 文件 | 说明 |
|------|------|
| `bmssp.h` | Breaking 算法头文件，定义 BlockHeap 和 BmsspSolver |
| `bmssp.cc` | Breaking 算法实现，包含主算法逻辑 |
| `global-route-manager-impl.cc` | NS-3 路由管理器，集成了算法切换逻辑 |
| `candidate-queue.h/cc` | NS-3 原生 Dijkstra 优先队列实现（基于链表） |

### 测试文件

| 文件 | 实验编号 | 说明 |
|------|---------|------|
| `test-correctness.cc` | 实验 1 | 正确性验证测试 |
| `test-scalability.cc` | 实验 2 | 节点规模扩展性测试 |
| `test-density.cc` | 实验 3 | 图稠密度影响测试 |
| `test-topology.cc` | 实验 4 | 拓扑结构类型测试 |
| `test-realistic.cc` | 实验 5 | 真实网络场景测试 |
| `test-large-scale.cc` | - | 大规模性能测试 |

### 配置文件

| 文件 | 说明 |
|------|------|
| `Makefile` | 自动化构建和测试脚本 |
| `project_requirements.md` | 项目需求和实验设计说明 |

---

## 依赖环境

- **NS-3**: 版本 3.46.1
- **编译器**: GCC 7.5+ (支持 C++14)
- **操作系统**: Linux (测试环境: Ubuntu 20.04)

---

## 开发历程

### 主要版本

| 版本 | 描述 |
|------|------|
| v1 | Breaking 算法初始实现 |
| v2 | 修复边界条件和内存问题 |
| v3 | 优化数据结构和排序策略 |
| v4 | 添加完整测试框架 |
| v5 | 大规模测试和性能分析 |
| v6 | 内存池优化 (NodePool) |

### Git 提交历史

```
9fa0ec4 并行和内存优化无效果 增加大规模测试
e2b4c19 benchmark v3 完成
2461d45 benchmark v2 完成
90a8ae7 benchmark v1 完成
d746477 实验设计和makefile完善
```

---

## 性能优化记录

### 尝试但未采用的优化

1. **并行计算**: Breaking 算法的层级结构使得并行化收益有限
2. **SIMD 指令**: 对于不规则的图结构，SIMD 加速效果不明显
3. **缓存优化**: 内存访问模式已经较好，缓存优化空间有限

### 成功的优化

1. **NodePool 内存池**: 减少内存分配开销
2. **容器容量预留**: 避免向量动态扩容
3. **固定随机种子**: 确保测试结果可复现

---

## 常见问题

### Q: 为什么 Breaking 在小规模图上没有明显优势？

A: Breaking 算法的常数开销较大（层级分块、桶维护），小规模图上这些开销超过了算法优势。随着规模增大，优势逐渐显现。

### Q: Breaking 算法是否适用于所有网络场景？

A: Breaking 在以下场景表现最佳：
- 稠密图 (边数 >> 节点数)
- 大规模网络 (1000+ 节点)
- 复杂拓扑 (数据中心、骨干网)

对于小规模稀疏图，Dijkstra 仍然是合理选择。

### Q: 如何切换算法？

A: 使用 Makefile 提供的命令：
```bash
make breaking   # 切换到 Breaking
make dijkstra   # 切换到 Dijkstra
```

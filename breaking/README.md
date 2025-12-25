# Breaking the Sorting Barrier for SSSP - BMSSP Algorithm Implementation

本项目实现了论文《Breaking the Sorting Barrier for Directed Single-Source Shortest Paths》中的核心算法和数据结构。

## 项目概述

本项目复现了 Duan 等人提出的有向图单源最短路径算法，首次在比较-加法模型下突破了 Dijkstra 算法的 $O(m + n \log n)$ 时间复杂度界限，达到了 $O(m \log^{2/3} n)$ 的时间复杂度。

## 主要组件

### 1. BlockHeap 数据结构 (`bmssp.h`)

BlockHeap 是论文 Lemma 3.3 中描述的关键数据结构，支持以下操作：

- **Initialize(M, B)**: 初始化数据结构，设置块大小 M 和上界 B
- **Insert(a, b)**: 插入键值对，均摊时间复杂度 $O(\max\{1, \log(N/M)\})$
- **BatchPrepend(L)**: 批量插入比当前所有值都小的键值对，时间复杂度 $O(L \cdot \max\{1, \log(L/M)\})$
- **Pull()**: 返回不超过 M 个最小键及分离上界，时间复杂度 $O(M)$

BlockHeap 使用两个块序列 D0 和 D1：
- D0 存储来自批量前置操作的元素
- D1 存储来自单个插入操作的元素
- D1 的块使用红黑树维护上界信息

### 2. BMSSP 算法 (`bmssp.cpp`)

实现了论文中的三个核心算法：

#### Algorithm 1: FindPivots(B, S)
- 从源点集合 S 中找到枢轴点集合 P
- 运行 k 轮 Bellman-Ford 式松弛
- 返回枢轴点集合 P（大小至多 |S|/k）和访问点集合 W

#### Algorithm 2: BaseCase(B, S)
- 基础情况：S 是单点集 {x}，且 x 已完成
- 运行类似 Dijkstra 的算法找到所有 d(v) < B 的顶点
- 若找到的顶点数 ≤ k，返回 B' = B；否则返回 B' = max d(v)

#### Algorithm 3: BMSSP(level, B, S)
- 主递归算法
- 使用分治策略将顶点集划分为 2^t 个部分
- 参数设置：
  - k = ⌊log^(1/3)(n)⌋
  - t = ⌊log^(2/3)(n)⌋
  - 层数 l = ⌈log(n)/t⌉

### 3. 验证程序 (`verify.cpp`)

包含完整的测试框架：

- **Dijkstra 实现**: 标准优先队列版本，用于验证正确性
- **随机图生成器**:
  - 随机连通图
  - 完全图
  - 网格图
  - 星形图
  - 路径图
  - 随机树

## 编译和运行

### 编译

```bash
make
```

或直接使用 g++：

```bash
g++ -std=c++17 -O2 -Wall -o verify verify.cpp bmssp.cpp
```

### 运行测试

```bash
# 默认参数：100个测试，最大顶点数50，最大边权100
make test

# 快速测试：10个测试，最大顶点数20，最大边权50
make test-quick

# 完整测试：500个测试，最大顶点数100，最大边权500
make test-full

# 自定义参数
./verify <测试数> <最大顶点数> <最大边权>
```

### 示例

```bash
# 运行200个测试，最大顶点数100，最大边权500
./verify 200 100 500
```

## 测试结果

程序会对每个测试用例：
1. 运行 Dijkstra 算法得到正确答案
2. 运行 BMSSP 算法
3. 比较两种算法的结果
4. 报告是否通过以及任何不匹配的情况

测试覆盖多种图类型，确保算法在各种情况下的正确性。

## 文件结构

```
breaking/
├── bmssp.h          # BlockHeap 数据结构和 BMSSP 函数声明
├── bmssp.cpp        # BMSSP 算法实现（FindPivots, BaseCase, BMSSP）
├── verify.cpp       # 验证程序，包含 Dijkstra 和测试框架
├── Makefile         # 构建文件
└── README.md        # 本文档
```

## 算法复杂度

- **时间复杂度**: $O(m \log^{2/3} n)$
  - 对于稀疏图（$m = O(n)$），优于 Dijkstra 的 $O(m + n \log n)$
- **空间复杂度**: $O(m + n)$

## 技术要点

1. **分治策略**: 将顶点集递归划分为更小的子问题
2. **前沿缩减**: 通过 FindPivots 减少需要处理的顶点数量
3. **部分排序**: BlockHeap 只需要部分排序，而非完全排序
4. **跨层次边重用**: 使用 `<=` 松弛条件确保下层松弛的边可被上层重用

## 参考文献

- Ran Duan, Jiayi Mao, Xiao Mao, Xinkai Shu, Longhui Yin. "Breaking the Sorting Barrier for Directed Single-Source Shortest Paths." arXiv:2504.17033, 2025.

## 实验要求完成情况

本项目完成了以下要求：

1. ✅ 理解相关背景知识，掌握论文原理
2. ✅ 基于论文提供的方法思路，复现论文算法
3. ✅ 将 BMSSP 算法与 Dijkstra 算法进行正确性和时间对比（正确性验证）

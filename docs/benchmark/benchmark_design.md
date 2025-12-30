# Breaking 算法实验设计方案

## 1. 设计背景与动机

### 1.1 研究背景

本项目旨在复现论文《Breaking the Sorting Barrier for Directed Single-Source Shortest Paths》中提出的 Breaking 算法。该算法首次在有向图的最短路径问题中突破了传统排序算法的下界，将时间复杂度从传统 Dijkstra 算法的 O(m + n log n) 降低到 O(m log^(2/3) n)。

### 1.2 应用场景

- **数据中心网络**：大规模服务器集群的路由计算，需要快速收敛
- **SDN 网络**：软件定义网络中的集中式路由决策
- **ISP 骨干网**：大规模网络拓扑下的路由优化

### 1.3 研究动机

传统 Dijkstra 算法受限于排序操作的下界 Ω(n log n)。Breaking 算法通过以下创新打破了这个瓶颈：

1. **分治策略**：将顶点集递归划分为 2^t 个子问题
2. **部分排序**：使用 BlockHeap 数据结构，只维护部分有序元素
3. **前沿缩减**：通过 FindPivots 减少需要处理的顶点数量

---

## 2. 算法对比分析

### 2.1 时间复杂度对比

| 算法 | 时间复杂度 | 空间复杂度 | 数据结构 |
|------|-----------|-----------|----------|
| Dijkstra (二叉堆) | O(m log n) | O(m + n) | 优先队列 |
| Dijkstra (斐波那契堆) | O(m + n log n) | O(m + n) | 斐波那契堆 |
| **Breaking** | **O(m log^(2/3) n)** | **O(m + n)** | **BlockHeap** |

### 2.2 算法特性对比

| 特性 | Dijkstra | Breaking |
|------|----------|----------|
| 排序方式 | 完全排序 | 部分排序 |
| 松弛条件 | `<` 严格小于 | `<=` 小于等于（允许边重用） |
| 算法策略 | 贪婪算法 | 分治递归 |
| 确定性 | 确定性 | 确定性 |

### 2.3 Breaking 算法核心组件

```
┌─────────────────────────────────────────────────────────────┐
│                    Breaking Algorithm                        │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐ │
│  │ FindPivots  │──│  BaseCase   │──│      BMSSP          │ │
│  │  (Algo 1)   │  │  (Algo 2)   │  │    (Algo 3)         │ │
│  └─────────────┘  └─────────────┘  └─────────────────────┘ │
│         │                 │                    │            │
│         └─────────────────┴────────────────────┘            │
│                           │                                 │
│                    ┌──────▼──────┐                         │
│                    │  BlockHeap  │                         │
│                    │  (Lemma 3.3)│                         │
│                    └─────────────┘                         │
└─────────────────────────────────────────────────────────────┘
```

**BlockHeap 操作**：
- `Initialize`: 初始化块结构
- `Insert`: 单个元素插入，O(max{1, log(N/M)})
- `BatchPrepend`: 批量前置，O(L·max{1, log(L/M)})
- `Pull`: 提取不超过 M 个最小元素，O(M)

**参数设置**：
- k = ⌊log^(1/3)(n)⌋
- t = ⌊log^(2/3)(n)⌋
- l = ⌈log(n)/t⌉

---

## 3. 实验场景设计依据

### 3.1 节点规模选择依据

基于 NS3 性能基准研究：

| 规模类别 | 节点数 | 典型应用 | 参考文献 |
|----------|--------|----------|----------|
| 小规模 | 25-100 | 实验验证、快速迭代 | [NS3 基准测试] |
| 中规模 | 100-500 | 典型研究规模 | [Weingärtner 2009] |
| 大规模 | 500-1000 | 接近实际场景 | [分布式 NS3] |
| 超大规模 | 1000+ | 压力测试 | [十亿节点模拟] |

**参考资料**：
- [A Benchmark Model for Parallel ns3](https://www.researchgate.net/publication/261851336_A_Benchmark_Model_for_Parallel_ns3)
- [The Quest for One Billion Node Simulation](https://www.nsnam.org/wp-content/uploads/2015/03/WNS3_2015-clean.pdf)

**实际测试数据**：
- 5700 节点模拟 1.5 秒网络行为需要 10.4 小时
- 1 百万节点需要 33GB 内存
- 因此建议最大测试规模：500-1000 节点

### 3.2 图稠密度设计依据

| 密度类别 | 边数范围 | 图类型 | 典型应用 |
|----------|----------|--------|----------|
| 稀疏图 | m ≈ 1.5n | 树状、网格 | 传感器网络 |
| 中等图 | m ≈ 3-5n | 随机连接 | 一般网络 |
| 密集图 | m ≈ 10n | 高连通 | 数据中心 |
| 很密图 | m ≈ 20n | 接近完全图 | 特殊场景 |
| 完全图 | m = n(n-1)/2 | 完全连接 | 理论分析 |

**理论依据**：
- Breaking 算法的优势在于减少排序开销
- 边数越多，排序在总时间中的占比越高
- 因此在密集图中 Breaking 应该更有优势

### 3.3 拓扑类型选择依据

| 拓扑类型 | 特征 | 实际应用 |
|----------|------|----------|
| 网格图 | 2D 规则，4/8 连通 | 数据中心机架布局 |
| 随机图 | 随机连接 | 通用网络模型 |
| 小世界图 | 高聚类 + 短路径 | 社交网络、P2P |
| 无标度图 | 幂律度分布 | Internet AS 级拓扑 |
| 树状图 | 无环，唯一路径 | 层级网络 |
| 层级图 | 多层结构 | 校园网、企业网 |

**实际网络场景参考**：
- [Fat-Tree 拓扑](https://www.sciencedirect.com/topics/computer-science/fat-tree-topology) - 数据中心主流拓扑
- [AI 数据中心架构](https://medium.com/@FIBERSTAMP_30578/ai-data-center-network-architecture-400-800g-optical-transceivers-980b7d94616e) - 400-800G 光模块应用

---

## 4. 实验设计与预期结果

### 4.1 实验一：正确性验证

**目标**：验证 Breaking 算法与 Dijkstra 算法结果 100% 一致

**测试场景**：
- 5×5 网格图 (25 节点)
- 10×10 网格图 (100 节点)
- 50 节点随机图
- 20 节点星形图
- 10 节点完全图

**验证方法**：
```cpp
for (每个源节点 s):
    d_breaking = BMSSP(s)
    d_dijkstra = Dijkstra(s)

    for (每个目标节点 t):
        assert(d_breaking[t] == d_dijkstra[t])
        assert(path_breaking[s][t] == path_dijkstra[s][t])
```

**成功标准**：所有测试场景 100% 一致

### 4.2 实验二：节点规模影响测试

**目标**：验证算法时间复杂度理论

**测试场景**（固定图密度）：
| 节点数 | 拓扑 | 预期边数 |
|--------|------|----------|
| 25 | 5×5 网格 | ~40 |
| 100 | 10×10 网格 | ~180 |
| 225 | 15×15 网格 | ~420 |
| 400 | 20×20 网格 | ~760 |
| 900 | 30×30 网格 | ~1740 |

**预期结果**：
- Dijkstra: O(m + n log n) ≈ O(n log n)
- Breaking: O(m log^(2/3) n) ≈ O(n log^(2/3) n)
- 当 n > 100 时，Breaking 应该更快

**分析方法**：
- 绘制 log-log 坐标图
- 拟合曲线验证复杂度
- 计算加速比 = Time_Dijkstra / Time_Breaking

### 4.3 实验三：图稠密度影响测试

**目标**：分析边密度对算法性能的影响

**测试场景**（固定 n=100）：
| 场景 | 边数 | 密度特征 |
|------|------|----------|
| 稀疏图 | ~150 | m ≈ 1.5n |
| 中等图 | ~300 | m ≈ 3n |
| 较密图 | ~500 | m ≈ 5n |
| 密集图 | ~1000 | m ≈ 10n |
| 很密图 | ~2000 | m ≈ 20n |
| 完全图 | 4950 | m = n(n-1)/2 |

**预期结果**：
- 边数越多，Breaking 的优势越明显
- 在完全图中，Breaking 应该显著快于 Dijkstra

### 4.4 实验四：拓扑类型影响测试

**目标**：分析不同网络拓扑下的算法表现

**测试场景**（固定 n=100）：
- 网格图：~180 边
- 随机图：~300 边
- 小世界图：~300 边
- 无标度图：~300 边
- 树状图：99 边
- 层级图：~200 边

**预期结果**：
- 规则拓扑（网格）中，两者差距较小
- 随机/复杂拓扑中，Breaking 更稳定
- 无环图（树状）中，Dijkstra 可能更快

### 4.5 实验五：实际网络场景模拟

**目标**：验证算法在实际场景中的表现

**测试场景**：
| 场景 | 拓扑 | 节点数 | 应用背景 |
|------|------|--------|----------|
| 数据中心 | Fat-Tree | 128 | k=4 Fat-Tree |
| 校园网 | 层级 | 100-200 | 多层结构 |
| ISP 骨干网 | 高连通 | 50-100 | 实际拓扑数据 |
| 物联网 | 树状/网状 | 100+ | 大规模传感器 |

---

## 5. 数据收集与分析方法

### 5.1 数据输出格式

**CSV 格式示例**：
```csv
Experiment_ID,Topology,Nodes,Edges,Algorithm,Time_us,Time_ms,Memory_kb,Verified
Exp1_1,Grid5x5,25,40,Breaking,500,0.5,1024,Yes
Exp1_1,Grid5x5,25,40,Dijkstra,300,0.3,1024,Yes
Exp2_2,Grid10x10,100,180,Breaking,2100,2.1,5120,Yes
Exp2_2,Grid10x10,100,180,Dijkstra,3500,3.5,5120,Yes
```

### 5.2 分析指标

| 指标 | 计算公式 | 说明 |
|------|----------|------|
| 加速比 | Time_Dijkstra / Time_Breaking | 值越大，Breaking 越快 |
| 时间差 | Time_Dijkstra - Time_Breaking | 绝对时间节省 |
| 复杂度拟合 | log(Time) vs log(n) | 验证理论复杂度 |
| 正确性 | 一致路径数 / 总路径数 | 应为 100% |

### 5.3 图表设计

**图表 1：节点规模 vs 运行时间**
- X 轴：节点数（对数坐标）
- Y 轴：运行时间（对数坐标）
- 两条曲线：Breaking 和 Dijkstra
- 预期：Breaking 斜率更小

**图表 2：边数 vs 加速比**
- X 轴：边数（或 m/n）
- Y 轴：加速比
- 预期：边数越多，加速比越高

**图表 3：拓扑类型对比**
- X 轴：拓扑类型
- Y 轴：运行时间（分组柱状图）
- 预期：Breaking 在大多数拓扑中更快

**图表 4：实际场景性能**
- X 轴：场景
- Y 轴：加速比
- 预期：数据中心等密集场景优势明显

---

## 6. 实现细节

### 6.1 算法切换机制

在 NS3 中通过 `m_useBmssp` 标志切换：

```cpp
// 文件：src/internet/model/global-route-manager-impl.cc
GlobalRouteManagerImpl::GlobalRouteManagerImpl()
    : m_useBmssp(true)  // true = Breaking, false = Dijkstra
```

**切换流程**：
```bash
# 使用 Breaking 算法
make build
make run PROGRAM=test-correctness

# 切换到 Dijkstra
# 修改 m_useBmssp = false
make build
make run PROGRAM=test-correctness
```

### 6.2 时间测量方法

```cpp
#include <chrono>

auto start = std::chrono::high_resolution_clock::now();
Ipv4GlobalRoutingHelper::PopulateRoutingTables();
auto end = std::chrono::high_resolution_clock::now();

auto time_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
auto time_ms = time_us / 1000.0;
```

### 6.3 正确性验证方法

```cpp
// 导出路由表进行对比
Ptr<OutputStreamWrapper> routingStream =
    Create<OutputStreamWrapper>("test.routes", std::ios::out);
Ipv4GlobalRoutingHelper::PrintRoutingTableAllAt(Seconds(0.1), routingStream);

// UDP Echo 测试验证连通性
UdpEchoServerHelper echoServer(9);
ApplicationContainer serverApps = echoServer.Install(destinationNode);
```

---

## 7. 预期交付成果

### 7.1 实验报告（1000+ 字）

1. **引言**：研究背景和动机
2. **算法原理**：Breaking 算法核心思想
3. **实验设计**：测试场景和指标
4. **实验结果**：数据分析和图表
5. **结论与讨论**：算法优缺点分析

### 7.2 Presentation PPT

1. 研究背景
2. 算法对比
3. 实验设计
4. 结果展示
5. 结论

### 7.3 源代码

- 完整的测试套件（5 个测试文件）
- 数据收集脚本
- 图表生成脚本

### 7.4 Demo 视频

- 正确性验证演示
- 性能对比展示
- 实际场景运行

---

## 8. 参考资料

### 8.1 论文与文献

- Breaking the Sorting Barrier for Directed Single-Source Shortest Paths
- NS3 官方文档：https://www.nsnam.org/

### 8.2 性能基准研究

- [A Benchmark Model for Parallel ns3](https://www.researchgate.net/publication/261851336_A_Benchmark_Model_for_Parallel_ns3)
- [The Quest for One Billion Node Simulation](https://www.nsnam.org/wp-content/uploads/2015/03/WNS3_2015-clean.pdf)
- [Performance of Distributed ns-3 Network Simulator](https://scispace.com/pdf/performance-of-distributed-ns-3-network-simulator-reh0kdpoyd.pdf)

### 8.3 网络拓扑研究

- [Fat-Tree Datacenter Network Topology](https://www.sciencedirect.com/topics/computer-science/fat-tree-topology)
- [Revolutionizing Datacenter Networks via Reconfigurable](https://arxiv.org/html/2502.16228v1)
- [AI Data Center Network Architecture Requirements](https://medium.com/@FIBERSTAMP_30578/ai-data-center-network-architecture-400-800g-optical-transceivers-980b7d94616e)

### 8.4 NS3 相关资源

- [How to Calculate Network Scalability Solutions in Ns3](https://ns3simulation.com/how-to-calculate-network-scalability-solutions-in-ns3/)
- [NS3 Model Library](https://www.nsnam.org/docs/release/3.30/models/ns-3-model-library.pdf)
- [A Performance Comparison of Recent Network Simulators](https://www.researchgate.net/publication/224574793_A_Performance_Comparison_of_Recent_Network_Simulators)

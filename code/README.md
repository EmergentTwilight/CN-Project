AI根据我的电脑写的readme,具体的有些地方只在我的电脑上适用,得做调整.包括.sh的脚本文件,也要做适配性调整


# Breaking the Sorting Barrier 实验代码

## 项目概述

本项目实现了两种最短路径算法的对比实验：
1. 传统 Dijkstra 算法
2. Breaking the Sorting Barrier 算法（Duan et al.）

## 快速开始

### 使用测试脚本（推荐）

```bash
cd /media/zyb/resource/STUDYYYYYYY/3autumn/CN/big/CN-Project/code

# 快速测试（推荐首次使用）
./run_tests.sh quick

# 完整测试
./run_tests.sh full

# 查看帮助
./run_tests.sh --help
```

### 测试模式

| 命令 | 功能 | 时间 |
|------|------|------|
| `./run_tests.sh quick` | 快速正确性验证 | ~2秒 |
| `./run_tests.sh performance` | 性能对比测试 | ~3分钟 |
| `./run_tests.sh report` | 生成报告 | ~5分钟 |
| `./run_tests.sh full` | 完整测试 | ~8分钟 |

### 常用选项

```bash
# 不重新编译
./run_tests.sh -n quick

# 显示详细输出
./run_tests.sh -v full

# 自定义测试规模
./run_tests.sh full --size=200
```

## 代码版本说明

### `main.cc` - 算法性能测试版本

**类型**: C++算法练习  
**功能**: 在NS-3框架内运行算法，进行性能测试和正确性验证  
**用途**: 快速算法性能对比

**特点**:
- ✅ 纯算法计算，速度快
- ✅ 支持多种拓扑（随机、分层、网格）
- ✅ 生成性能报告
- ❌ 不进行实际网络仿真

### `main_ns3_simulation.cc` - NS-3仿真版本

**类型**: 真正的NS-3网络仿真  
**功能**: 完整的网络仿真实验

**特点**:
- ✅ 创建真实的NS-3拓扑（NodeContainer + PointToPointHelper）
- ✅ 从NS-3拓扑提取Graph用于算法计算
- ✅ 将算法结果注入NS-3路由表
- ✅ 运行仿真验证路由正确性（UDP回显测试）

**使用方法**:
```bash
# 方法1: 替换主文件
cp main_ns3_simulation.cc main.cc
./run_tests.sh quick

# 方法2: 同时编译两个版本（需修改CMakeLists.txt）
./ns3 run scratch/main_ns3_simulation
```

## 重要说明：NS-3仿真集成

### 问题说明

**注意**: `main.cc` 当前版本**无法起到真正的NS-3仿真效果**，它只是一个"在NS-3框架内运行的C++算法练习"。

**原因**:
1. ❌ 没有创建NS-3拓扑（只有纯C++的Graph对象）
2. ❌ 没有运行仿真（没有 `Simulator::Run()`）
3. ❌ 算法结果未注入NS-3（predecessors数组只存在内存中）
4. ❌ 无法验证路由（无法通过实际发包验证）

### 解决方案

使用 `main_ns3_simulation.cc` 可以获得真正的NS-3仿真效果：

| 特性 | main.cc | main_ns3_simulation.cc |
|------|---------|------------------------|
| 拓扑创建 | 纯C++ Graph对象 | NS-3 NodeContainer |
| 算法运行 | ✅ | ✅ |
| 路由注入 | ❌ | ✅ InstallRoutesToNS3() |
| 仿真运行 | ❌ | ✅ Simulator::Run() |
| 验证方式 | 只比对距离数组 | UDP回显测试 |

### 关键函数

**`InstallRoutesToNS3()`** - 路由注入函数（在 `main_ns3_simulation.cc` 中）

将算法计算出的路径注入到NS-3路由表：

```cpp
void InstallRoutesToNS3(
    uint32_t sourceId,                          // 源节点ID
    const std::vector<uint32_t>& predecessors,  // 算法算出的前驱数组
    NodeContainer& nodes,                        // NS-3节点容器
    const std::vector<Ipv4Address>& nodeIps     // 节点ID到IP的映射
)
```

## 程序参数

程序支持以下命令行参数：

- `--correctness=true/false` - 运行算法正确性测试（默认: true）
- `--performance=true/false` - 运行性能对比测试（默认: true）
- `--report=true/false` - 生成性能报告（默认: true）
- `--size=<数字>` - 设置测试规模（默认: 100）

### 使用示例

```bash
# 只运行正确性测试
./run_tests.sh quick

# 只运行性能测试
./run_tests.sh performance

# 自定义参数
./run_tests.sh custom --size=200 --correctness=true
```

## 程序功能

### 1. 算法正确性验证
- 在小规模测试图上验证 Dijkstra 和 Breaking Sorting Barrier 算法结果一致

### 2. 综合性能测试
- 测试多种网络拓扑：
  - 随机拓扑（20、100、500 节点）
  - 分层拓扑（数据中心风格）
  - 网格拓扑（10x10）
- 收集性能指标：执行时间、内存使用

### 3. 性能报告生成
- 生成 CSV 格式结果文件：`performance_results.csv`
- 生成 JSON 格式结果文件：`performance_results.json`
- 生成详细报告：`performance_results_detailed.csv`
- 生成实验报告：`experiment_report.txt`

## 输出文件

程序运行后会在 NS-3 目录（`/home/zyb/ns-3.41/`）生成以下文件：

- `performance_results.csv` - 性能结果（CSV 格式）
- `performance_results.json` - 性能结果（JSON 格式）
- `performance_results_detailed.csv` - 详细性能结果
- `experiment_report.txt` - 实验报告

## 代码结构

```
code/
├── main.cc                          # 主程序（算法性能测试版本）
├── main_ns3_simulation.cc          # NS-3仿真版本
├── algorithm-lib/                   # 算法库
│   ├── shortest-path-algorithms.h  # 算法头文件
│   ├── shortest-path-algorithms.cc # 算法实现
│   └── CMakeLists.txt              # 库构建配置
├── CMakeLists.txt                   # 主程序构建配置
├── run_tests.sh                     # 测试脚本（推荐）⭐
├── run_code.sh                      # 基础运行脚本
└── README.md                        # 本文件
```

## 故障排除

### 问题 1: 找不到 NS-3 目录

**解决方案**: 修改 `run_code.sh` 或 `run_tests.sh` 中的 `NS3_DIR` 变量为实际的 NS-3 安装路径。

### 问题 2: 编译错误

**可能原因**:
- NS-3 未正确安装
- 缺少必要的依赖库

**解决方案**:
```bash
cd /home/zyb/ns-3.41
./ns3 configure
./ns3 build
```

### 问题 3: 运行时找不到算法库

**解决方案**: 确保 `algorithm-lib` 目录和 `CMakeLists.txt` 都已正确复制到 scratch 目录。

## 实验建议

1. **算法性能对比**: 使用 `main.cc` 进行快速性能测试
2. **路由收敛验证**: 使用 `main_ns3_simulation.cc` 进行完整的网络仿真
3. **演示**: 使用 `main_ns3_simulation.cc` 进行Demo，展示真实的网络仿真效果

## 注意事项

- 确保 NS-3 已正确安装并配置
- 首次运行可能需要较长时间进行编译
- 性能测试可能需要几分钟时间，取决于测试规模
- 生成的结果文件会保存在运行程序的目录中

    ✅ 核心算法移植：将 C++ 版 BMSSP 算法封装为 NS-3 类。
    ✅ 数据结构适配：实现了从 NS-3 LSA 到 BMSSP Graph 的转换。
    ✅ 路由表写入：修复了 NextHop 查找逻辑，支持生成正确的 Global Routing Table。
    ✅ 正确性验证：在 100 节点网格拓扑下，BMSSP 与 Dijkstra 的端到端延迟完全一致，且 Ping 通率 100%。


### 修改的文件列表

请重点关注以下路径的文件变更：

##### A. 核心算法文件 (新增)

    src/internet/model/bmssp.h：算法头文件，定义了 BmsspSolver 类及图结构。
    src/internet/model/bmssp.cc：算法核心实现，包含 SPFCalculateBMSSP 逻辑。

##### B. 路由模块集成 (修改)

    src/internet/model/ipv4-global-routing.cc：
        修改了 SPFCalculate() 函数的调用逻辑。
        目前已接入 SPFCalculateBMSSP() 接口。
    src/internet/wscript：
        修改了编译脚本，加入了 bmssp.cc 的编译支持。

##### C. 测试脚本 (新增)

    scratch/benchmark.cc：
        专用的测试脚本，支持简单的 4 节点测试和复杂的 Grid (网格) 压力测试。
        集成了 UDP Echo 服务，用于验证真实发包情况。



## 项目设置

### 首次克隆项目后

**重要：** 本项目不包含编译后的文件（`build/` 目录），你需要本地编译。

1. **进入 NS-3 目录**：
   ```sh
   cd ns-allinone-3.46.1/ns-3.46.1
   ```

2. **配置项目**（首次运行需要）：
   ```sh
   ./ns3 configure
   ```

3. **编译项目**：
   ```sh
   ./ns3 build
   ```

4. **运行测试**：
   ```sh
   ./ns3 run benchmark
   ```

### 使用项目根目录的 Makefile（推荐）

你也可以在项目根目录使用 Makefile：

```sh
# 编译并运行 benchmark
make run PROGRAM=benchmark

# 或者只编译
make build
```

### 关于 .gitignore

项目已配置 `.gitignore`，会自动忽略：
- `build/` 目录（编译产物）
- `*.o`, `*.so` 等二进制文件
- `*.pcap`, `*.tr`, `*.routes` 等仿真输出文件
- IDE 配置文件（`.vscode/`, `.idea/` 等）

**注意：** 不要上传 `build/` 文件夹到 Git。拉取代码后需要本地编译。
### 启用bmssp
是否启用bmssp算法的开关在
ns-allinone-3.46.1/ns-3.46.1/src/internet/model/global-route-manager-impl.cc
的第538行

m_useBmssp(false)  // true启用 BMSSP 算法，设为 false 则使用原有 Dijkstra

请修改这里的true/false以测试bmssp或者dijkstra算法

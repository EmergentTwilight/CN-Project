# Dijkstra 示例程序运行说明

## 快速运行

### 方法 1: 使用运行脚本（推荐）

在 `dijkstra` 目录下运行：

```bash
./run_dijkstra.sh
```

### 方法 2: 手动运行

1. 复制文件到 ns-3 的 scratch 目录：
```bash
cp dijkstra_example.cc /home/zyb/ns-3.41/scratch/dijkstra-example.cc
```

2. 进入 ns-3 目录并构建：
```bash
cd /home/zyb/ns-3.41
./ns3 build
```

3. 运行程序：
```bash
./ns3 run scratch/dijkstra-example --no-build
```

## 程序说明

这个示例程序演示了 NS-3 中使用 Dijkstra 算法进行路由选择：

- 创建了 3 个节点的简单拓扑：Node 0 <-> Node 1 <-> Node 2
- 使用 `Ipv4GlobalRoutingHelper::PopulateRoutingTables()` 触发 Dijkstra 算法计算路由表
- 通过 UDP 回显应用验证路由是否正确工作

## 查看详细输出

如果需要查看更详细的日志输出，可以在运行前设置日志级别：

```bash
cd /home/zyb/ns-3.41
NS_LOG="UdpEchoClientApplication=level_info:UdpEchoServerApplication=level_info" ./ns3 run scratch/dijkstra-example --no-build
```

## 注意事项

- 确保 NS-3 已正确安装在 `/home/zyb/ns-3.41/`
- 如果 NS-3 安装在其他位置，请修改 `run_dijkstra.sh` 中的 `NS3_DIR` 变量


/* scratch/test-topology.cc - Breaking vs Dijkstra Topology Type Test
 *
 * 实验 4：拓扑结构类型影响测试
 * 目标：分析不同网络拓扑对算法性能的影响
 *
 * 测试场景（固定节点数 n=100）：
 * - 网格图 (2D 规则)
 * - 随机图 (随机连接)
 * - 星形图 (中心密集)
 * - 树状图 (无环)
 *
 * 预期结果：
 * - 规则拓扑中，两者差距较小
 * - 随机/复杂拓扑中，Breaking 更稳定
 *
 * 输出：CSV 格式结果到 test-topology-results.csv
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/point-to-point-layout-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv4-address-generator.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <random>
#include <queue>
#include <vector>
#include <cmath>
#include <functional>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TopologyTest");

// ================================================================
// 结果记录结构
// ================================================================
struct TopologyResult
{
    std::string testName;
    std::string topologyType;
    std::string topologyDesc;
    uint32_t nodes;
    uint32_t edges;
    double avgPathLength;
    std::string algorithm;
    double time_ms;
    double time_us;
    int num_runs;        // 运行次数
    double std_dev_ms;   // 标准差（毫秒）

    std::string ToCsv() const
    {
        std::stringstream ss;
        ss << testName << "," << topologyType << "," << topologyDesc << ","
           << nodes << "," << edges << ","
           << std::fixed << std::setprecision(2) << avgPathLength << ","
           << algorithm << ","
           << std::fixed << std::setprecision(3) << time_ms << ","
           << std::fixed << std::setprecision(1) << time_us << ","
           << num_runs << "," << std::fixed << std::setprecision(3) << std_dev_ms;
        return ss.str();
    }
};

// ================================================================
// CSV 头部
// ================================================================
const std::string CSV_HEADER =
    "TestName,TopologyType,TopologyDesc,Nodes,Edges,AvgPathLength,Algorithm,Time_ms,Time_us,NumRuns,StdDev_ms";

// ================================================================
// 计时和统计辅助函数
// ================================================================

// 运行计时函数，支持多次运行取平均值
// 如果单次运行时间 < threshold_ms 秒，则运行 num_iterations 次取平均
struct TimingResult
{
    double avg_time_ms;
    double avg_time_us;
    double std_dev_ms;
    int num_runs;
};

TimingResult TimeFunction(std::function<void()> func,
                          double threshold_ms = 5000.0,  // 5秒阈值
                          int num_iterations = 10)        // 快速测试运行10次
{
    std::vector<double> times_ms;

    // 第一次运行，检测时间
    {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = end - start;
        times_ms.push_back(elapsed.count());
    }

    // 如果第一次运行时间小于阈值，再运行多次
    if (times_ms[0] < threshold_ms)
    {
        int additional_runs = num_iterations - 1;
        for (int i = 0; i < additional_runs; i++)
        {
            auto start = std::chrono::high_resolution_clock::now();
            func();
            auto end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> elapsed = end - start;
            times_ms.push_back(elapsed.count());
        }
    }

    // 计算平均值和标准差
    double sum = 0;
    for (double t : times_ms)
    {
        sum += t;
    }
    double avg = sum / times_ms.size();

    // 计算标准差
    double variance = 0;
    for (double t : times_ms)
    {
        variance += (t - avg) * (t - avg);
    }
    variance /= times_ms.size();
    double std_dev = std::sqrt(variance);

    TimingResult result;
    result.avg_time_ms = avg;
    result.avg_time_us = avg * 1000.0;
    result.std_dev_ms = std_dev;
    result.num_runs = times_ms.size();

    return result;
}

// ================================================================
// 拓扑创建函数
// ================================================================

// 1. 网格图
uint32_t CreateGridTopology(uint32_t nRows, uint32_t nCols)
{
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    PointToPointGridHelper grid(nRows, nCols, p2p);

    InternetStackHelper stack;
    grid.InstallStack(stack);

    grid.AssignIpv4Addresses(Ipv4AddressHelper("10.1.0.0", "255.255.255.0"),
                              Ipv4AddressHelper("10.2.0.0", "255.255.255.0"));

    return (nRows * (nCols - 1)) + ((nRows - 1) * nCols);
}

// 2. 随机图
uint32_t CreateRandomTopology(uint32_t nNodes, double probability)
{
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NodeContainer nodes;
    nodes.Create(nNodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    uint32_t linkIndex = 0;
    uint32_t edgeCount = 0;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.0, 1.0);

    for (uint32_t i = 0; i < nNodes; i++)
    {
        for (uint32_t j = i + 1; j < nNodes; j++)
        {
            if (dis(gen) < probability)
            {
                NetDeviceContainer devices = p2p.Install(nodes.Get(i), nodes.Get(j));

                Ipv4AddressHelper address;
                std::stringstream subnet;
                subnet << "10." << (linkIndex / 256) << "." << (linkIndex % 256) << ".0";
                address.SetBase(subnet.str().c_str(), "255.255.255.0");

                Ipv4InterfaceContainer interfaces = address.Assign(devices);
                linkIndex++;
                edgeCount++;
            }
        }
    }

    // 确保连通性（添加链式连接）
    for (uint32_t i = 0; i < nNodes - 1; i++)
    {
        NetDeviceContainer devices = p2p.Install(nodes.Get(i), nodes.Get(i + 1));

        Ipv4AddressHelper address;
        std::stringstream subnet;
        subnet << "10." << (linkIndex / 256) << "." << (linkIndex % 256) << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");

        Ipv4InterfaceContainer interfaces = address.Assign(devices);
        linkIndex++;
        edgeCount++;
    }

    return edgeCount;
}

// 3. 星形图
uint32_t CreateStarTopology(uint32_t nNodes)
{
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NodeContainer nodes;
    nodes.Create(nNodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    // 中心节点连接所有其他节点
    for (uint32_t i = 1; i < nNodes; i++)
    {
        NetDeviceContainer devices = p2p.Install(nodes.Get(0), nodes.Get(i));

        Ipv4AddressHelper address;
        std::stringstream subnet;
        subnet << "10.0." << i << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");

        Ipv4InterfaceContainer interfaces = address.Assign(devices);
    }

    return nNodes - 1;
}

// 4. 树状图 (二叉树结构)
uint32_t CreateTreeTopology(uint32_t nNodes)
{
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NodeContainer nodes;
    nodes.Create(nNodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    uint32_t linkIndex = 0;
    uint32_t edgeCount = 0;

    // 创建完全二叉树：节点 i 的父节点是 (i-1)/2
    for (uint32_t i = 1; i < nNodes; i++)
    {
        uint32_t parent = (i - 1) / 2;

        NetDeviceContainer devices = p2p.Install(nodes.Get(parent), nodes.Get(i));

        Ipv4AddressHelper address;
        std::stringstream subnet;
        subnet << "10." << (linkIndex / 256) << "." << (linkIndex % 256) << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");

        Ipv4InterfaceContainer interfaces = address.Assign(devices);
        linkIndex++;
        edgeCount++;
    }

    return edgeCount;
}

// ================================================================
// 执行拓扑测试
// ================================================================
TopologyResult RunTopologyTest(const std::string &testName,
                                const std::string &topologyType,
                                const std::string &topologyDesc,
                                uint32_t nodes, uint32_t edges,
                                const std::string &algorithm)
{
    NS_LOG_UNCOND("------------------------------------------------");
    NS_LOG_UNCOND("Running: " << testName << " (" << topologyType << ")");
    NS_LOG_UNCOND("Description: " << topologyDesc);
    NS_LOG_UNCOND("Nodes: " << nodes << ", Edges: " << edges);
    NS_LOG_UNCOND("Algorithm: " << algorithm);
    NS_LOG_UNCOND("------------------------------------------------");

    // 使用计时函数运行，使用 RecomputeRoutingTables 进行多次测试
    TimingResult timing = TimeFunction([&]() {
        Ipv4GlobalRoutingHelper::RecomputeRoutingTables();
    });

    TopologyResult result;
    result.testName = testName;
    result.topologyType = topologyType;
    result.topologyDesc = topologyDesc;
    result.nodes = nodes;
    result.edges = edges;
    result.avgPathLength = 0; // 需要额外计算，暂设为 0
    result.algorithm = "Breaking";  // Switched via make breaking/dijkstra
    result.time_ms = timing.avg_time_ms;
    result.time_us = timing.avg_time_us;
    result.num_runs = timing.num_runs;
    result.std_dev_ms = timing.std_dev_ms;

    NS_LOG_UNCOND("Time: " << result.time_ms << " ms (" << result.time_us << " us)");
    NS_LOG_UNCOND("Runs: " << result.num_runs << ", StdDev: " << result.std_dev_ms << " ms");

    return result;
}

// ================================================================
// 主测试函数
// ================================================================
int main(int argc, char *argv[])
{
    // 禁用详细日志
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_ERROR);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_ERROR);
    LogComponentEnable("TopologyTest", LOG_LEVEL_INFO);

    // 命令行参数
    uint32_t nNodes = 100; // 默认节点数
    CommandLine cmd;
    cmd.AddValue("nNodes", "Number of nodes (for applicable topologies)", nNodes);
    cmd.Parse(argc, argv);

    // 打开 CSV 输出文件
    std::ofstream csvFile("test-topology-results.csv");
    csvFile << CSV_HEADER << std::endl;

    NS_LOG_UNCOND("================================================");
    NS_LOG_UNCOND("Experiment 4: Topology Type Test");
    NS_LOG_UNCOND("Testing algorithm performance across different topologies");
    NS_LOG_UNCOND("================================================");
    NS_LOG_UNCOND("");

    int testNum = 1;

    // ================================================================
    // 测试场景 4.1: 网格图
    // ================================================================
    {
        NS_LOG_UNCOND("================================================");
        NS_LOG_UNCOND("Test 4." << testNum << ": Grid Topology");
        NS_LOG_UNCOND("================================================");

        // 重置模拟环境以清除之前的 IP 地址分配
        Simulator::Destroy();
        Ipv4AddressGenerator::Reset();

        uint32_t n = 10; // 10x10 = 100 nodes
        uint32_t edges = CreateGridTopology(n, n);

        std::string testName = "Exp4_" + std::to_string(testNum);

        TopologyResult result = RunTopologyTest(
            testName, "Grid", "2D Regular Grid", n * n, edges, "Breaking");
        csvFile << result.ToCsv() << std::endl;

        NS_LOG_UNCOND("");
        testNum++;
    }

    // ================================================================
    // 测试场景 4.2: 随机图
    // ================================================================
    {
        NS_LOG_UNCOND("================================================");
        NS_LOG_UNCOND("Test 4." << testNum << ": Random Topology");
        NS_LOG_UNCOND("================================================");

        // 重置模拟环境以清除之前的 IP 地址分配
        Simulator::Destroy();
        Ipv4AddressGenerator::Reset();

        uint32_t edges = CreateRandomTopology(nNodes, 0.06); // 约 3n 边

        std::string testName = "Exp4_" + std::to_string(testNum);

        TopologyResult result = RunTopologyTest(
            testName, "Random", "Erdős-Rényi Random Graph", nNodes, edges, "Breaking");
        csvFile << result.ToCsv() << std::endl;

        NS_LOG_UNCOND("");
        testNum++;
    }

    // ================================================================
    // 测试场景 4.3: 星形图
    // ================================================================
    {
        NS_LOG_UNCOND("================================================");
        NS_LOG_UNCOND("Test 4." << testNum << ": Star Topology");
        NS_LOG_UNCOND("================================================");

        // 重置模拟环境以清除之前的 IP 地址分配
        Simulator::Destroy();
        Ipv4AddressGenerator::Reset();

        uint32_t edges = CreateStarTopology(nNodes);

        std::string testName = "Exp4_" + std::to_string(testNum);

        TopologyResult result = RunTopologyTest(
            testName, "Star", "Central Hub Topology", nNodes, edges, "Breaking");
        csvFile << result.ToCsv() << std::endl;

        NS_LOG_UNCOND("");
        testNum++;
    }

    // ================================================================
    // 测试场景 4.4: 树状图
    // ================================================================
    {
        NS_LOG_UNCOND("================================================");
        NS_LOG_UNCOND("Test 4." << testNum << ": Tree Topology");
        NS_LOG_UNCOND("================================================");

        // 重置模拟环境以清除之前的 IP 地址分配
        Simulator::Destroy();
        Ipv4AddressGenerator::Reset();

        uint32_t edges = CreateTreeTopology(nNodes);

        std::string testName = "Exp4_" + std::to_string(testNum);

        TopologyResult result = RunTopologyTest(
            testName, "Tree", "Binary Tree Structure", nNodes, edges, "Breaking");
        csvFile << result.ToCsv() << std::endl;

        NS_LOG_UNCOND("");
        testNum++;
    }

    // ================================================================
    // 关闭 CSV 文件并输出总结
    // ================================================================
    csvFile.close();

    NS_LOG_UNCOND("");
    NS_LOG_UNCOND("================================================");
    NS_LOG_UNCOND("Topology Type Test Completed!");
    NS_LOG_UNCOND("Results saved to: test-topology-results.csv");
    NS_LOG_UNCOND("");
    NS_LOG_UNCOND("To complete the comparison:");
    NS_LOG_UNCOND("  1. Switch algorithm: m_useBmssp = false (Dijkstra)");
    NS_LOG_UNCOND("  2. Recompile: make build");
    NS_LOG_UNCOND("  3. Run again: make run PROGRAM=test-topology");
    NS_LOG_UNCOND("  4. Compare performance across topologies");
    NS_LOG_UNCOND("");
    NS_LOG_UNCOND("Expected Results:");
    NS_LOG_UNCOND("  - Grid: Regular structure, moderate difference");
    NS_LOG_UNCOND("  - Random: Breaking should be more stable");
    NS_LOG_UNCOND("  - Star: Both algorithms fast (simple structure)");
    NS_LOG_UNCOND("  - Tree: Dijkstra may be competitive (acyclic)");
    NS_LOG_UNCOND("================================================");

    return 0;
}

/* scratch/test-realistic.cc - Breaking vs Dijkstra Real-World Scenario Test
 *
 * 实验 5：实际网络场景模拟
 * 目标：验证算法在实际网络场景中的表现
 *
 * 测试场景：
 * - 数据中心 (Fat-Tree 拓扑)
 * - 校园网 (层级结构)
 * - ISP 骨干网 (高连通)
 *
 * 预期结果：
 * - 数据中心等密集场景中 Breaking 优势明显
 *
 * 输出：CSV 格式结果到 test-realistic-results.csv
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
#include <cmath>
#include <vector>
#include <functional>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RealisticTest");

// ================================================================
// 结果记录结构
// ================================================================
struct RealisticResult
{
    std::string testName;
    std::string scenario;
    std::string topologyType;
    uint32_t nodes;
    uint32_t edges;
    std::string algorithm;
    double time_ms;
    double time_us;
    double time_per_node_us;
    std::string notes;
    int num_runs;        // 运行次数
    double std_dev_ms;   // 标准差（毫秒）

    std::string ToCsv() const
    {
        std::stringstream ss;
        ss << testName << "," << scenario << "," << topologyType << ","
           << nodes << "," << edges << "," << algorithm << ","
           << std::fixed << std::setprecision(3) << time_ms << ","
           << std::fixed << std::setprecision(1) << time_us << ","
           << std::fixed << std::setprecision(2) << time_per_node_us << ","
           << num_runs << "," << std::fixed << std::setprecision(3) << std_dev_ms << ","
           << "\"" << notes << "\"";
        return ss.str();
    }
};

// ================================================================
// CSV 头部
// ================================================================
const std::string CSV_HEADER =
    "TestName,Scenario,TopologyType,Nodes,Edges,Algorithm,Time_ms,Time_us,TimePerNode_us,NumRuns,StdDev_ms,Notes";

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
// 实际场景拓扑创建函数
// ================================================================

// 1. 数据中心 Fat-Tree 拓扑 (简化版 k=4)
// Fat-Tree 参数: k 个端口，k=4 时有 k^3/4 = 16 个主机，k^2/2 = 8 个交换机
// 简化实现：创建一个三层结构 (Core-Aggregation-Edge)
uint32_t CreateFatTreeTopology(uint32_t k)
{
    // k-ary Fat-Tree:
    // - Core 层: (k/2)^2 个交换机
    // - Aggregation 层: k * (k/2) 个交换机
    // - Edge 层: k * (k/2) 个交换机
    // - 主机: k * k * (k/2) / 2 个主机（这里简化，只包含交换机）

    uint32_t nCore = (k / 2) * (k / 2);
    uint32_t nAgg = k * (k / 2);
    uint32_t nEdge = k * (k / 2);
    uint32_t nSwitches = nCore + nAgg + nEdge;

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Gbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));

    NodeContainer switches;
    switches.Create(nSwitches);

    InternetStackHelper stack;
    stack.Install(switches);

    uint32_t linkIndex = 0;
    uint32_t edgeCount = 0;

    // Core 连接 Aggregation
    // 每个 Core 交换机连接到所有 pod 的 Aggregation 交换机
    for (uint32_t i = 0; i < nCore; i++)
    {
        for (uint32_t pod = 0; pod < k; pod++)
        {
            for (uint32_t agg = 0; agg < (k / 2); agg++)
            {
                uint32_t aggIndex = nCore + pod * (k / 2) + agg;

                NetDeviceContainer devices = p2p.Install(
                    switches.Get(i), switches.Get(aggIndex));

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

    // Aggregation 连接 Edge (同一 pod 内)
    for (uint32_t pod = 0; pod < k; pod++)
    {
        for (uint32_t agg = 0; agg < (k / 2); agg++)
        {
            uint32_t aggIndex = nCore + pod * (k / 2) + agg;

            for (uint32_t edge = 0; edge < (k / 2); edge++)
            {
                uint32_t edgeIndex = nCore + k * (k / 2) + pod * (k / 2) + edge;

                NetDeviceContainer devices = p2p.Install(
                    switches.Get(aggIndex), switches.Get(edgeIndex));

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

    return edgeCount;
}

// 2. 校园网层级拓扑
// 三层结构：核心 -> 楼宇 -> 楼层
uint32_t CreateCampusTopology(uint32_t nBuildings, uint32_t nFloorsPerBuilding)
{
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
    p2p.SetChannelAttribute("Delay", StringValue("5ms"));

    // 核心 + 楼宇 + 楼层交换机
    uint32_t nCore = 2; // 两个核心交换机（冗余）
    uint32_t nBuilding = nBuildings;
    uint32_t nFloor = nBuildings * nFloorsPerBuilding;
    uint32_t nSwitches = nCore + nBuilding + nFloor;

    NodeContainer switches;
    switches.Create(nSwitches);

    InternetStackHelper stack;
    stack.Install(switches);

    uint32_t linkIndex = 0;
    uint32_t edgeCount = 0;

    // 核心交换机互联
    NetDeviceContainer coreDevices = p2p.Install(switches.Get(0), switches.Get(1));

    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(coreDevices);
    linkIndex++;
    edgeCount++;

    // 核心到楼宇
    for (uint32_t i = 0; i < nBuilding; i++)
    {
        for (uint32_t c = 0; c < nCore; c++)
        {
            NetDeviceContainer devices = p2p.Install(
                switches.Get(c), switches.Get(nCore + i));

            std::stringstream subnet;
            subnet << "10." << (linkIndex / 256) << "." << (linkIndex % 256) << ".0";
            address.SetBase(subnet.str().c_str(), "255.255.255.0");

            interfaces = address.Assign(devices);
            linkIndex++;
            edgeCount++;
        }
    }

    // 楼宇到楼层
    for (uint32_t b = 0; b < nBuilding; b++)
    {
        for (uint32_t f = 0; f < nFloorsPerBuilding; f++)
        {
            uint32_t buildingIndex = nCore + b;
            uint32_t floorIndex = nCore + nBuilding + b * nFloorsPerBuilding + f;

            NetDeviceContainer devices = p2p.Install(
                switches.Get(buildingIndex), switches.Get(floorIndex));

            std::stringstream subnet;
            subnet << "10." << (linkIndex / 256) << "." << (linkIndex % 256) << ".0";
            address.SetBase(subnet.str().c_str(), "255.255.255.0");

            interfaces = address.Assign(devices);
            linkIndex++;
            edgeCount++;
        }
    }

    return edgeCount;
}

// 3. ISP 骨干网拓扑 (简化 Mesh 结构)
// 高连通性的网状结构
uint32_t CreateISPTopology(uint32_t nRouters, uint32_t degree)
{
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Gbps"));
    p2p.SetChannelAttribute("Delay", StringValue("10ms"));

    NodeContainer routers;
    routers.Create(nRouters);

    InternetStackHelper stack;
    stack.Install(routers);

    uint32_t linkIndex = 0;
    uint32_t edgeCount = 0;

    // 创建高连通性的网状结构
    // 每个路由器连接到最近的 'degree' 个其他路由器
    for (uint32_t i = 0; i < nRouters; i++)
    {
        for (uint32_t j = 1; j <= degree; j++)
        {
            uint32_t target = (i + j) % nRouters;

            // 避免重复边 (只创建 i < target 的边)
            if (i < target)
            {
                NetDeviceContainer devices = p2p.Install(
                    routers.Get(i), routers.Get(target));

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

    // 确保图是连通的（添加一个环）
    for (uint32_t i = 0; i < nRouters; i++)
    {
        uint32_t target = (i + 1) % nRouters;

        // 简化处理：直接创建边（可能有少量重复）
        NetDeviceContainer devices = p2p.Install(
            routers.Get(i), routers.Get(target));

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
// 执行实际场景测试
// ================================================================
RealisticResult RunRealisticTest(const std::string &testName,
                                  const std::string &scenario,
                                  const std::string &topologyType,
                                  uint32_t nodes, uint32_t edges,
                                  const std::string &algorithm,
                                  const std::string &notes)
{
    NS_LOG_UNCOND("  Topology: " << topologyType);
    NS_LOG_UNCOND("  Nodes: " << nodes << ", Edges: " << edges);
    NS_LOG_UNCOND("  Notes: " << notes);
    NS_LOG_UNCOND("  Running routing calculation...");

    // 使用计时函数运行，使用 RecomputeRoutingTables 进行多次测试
    TimingResult timing = TimeFunction([&]() {
        Ipv4GlobalRoutingHelper::RecomputeRoutingTables();
    });

    RealisticResult result;
    result.testName = testName;
    result.scenario = scenario;
    result.topologyType = topologyType;
    result.nodes = nodes;
    result.edges = edges;
    result.algorithm = "Dijkstra";  // 会被 Makefile 替换
    result.time_ms = timing.avg_time_ms;
    result.time_us = timing.avg_time_us;
    result.time_per_node_us = timing.avg_time_us / nodes;
    result.num_runs = timing.num_runs;
    result.std_dev_ms = timing.std_dev_ms;
    result.notes = notes;

    NS_LOG_UNCOND("  Time: " << result.time_ms << " ms (" << result.time_us << " us)");
    NS_LOG_UNCOND("  Time per node: " << result.time_per_node_us << " us/node");
    NS_LOG_UNCOND("  Runs: " << result.num_runs << ", StdDev: " << result.std_dev_ms << " ms");

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
    LogComponentEnable("RealisticTest", LOG_LEVEL_INFO);

    // 命令行参数
    bool runLargeTests = false;
    CommandLine cmd;
    cmd.AddValue("runLargeTests", "Run large-scale tests (more nodes)", runLargeTests);
    cmd.Parse(argc, argv);

    // 打开 CSV 输出文件
    std::ofstream csvFile("test-realistic-results.csv");
    csvFile << CSV_HEADER << std::endl;

    // 定义总测试数（不包括可选的大规模测试）
    const int totalTests = 3;
    int testNum = 1;

    NS_LOG_UNCOND("================================================");
    NS_LOG_UNCOND("Experiment 5: Real-World Scenario Test");
    NS_LOG_UNCOND("Algorithm: Algorithm");  // 会被 Makefile 替换为 Breaking 或 Dijkstra
    NS_LOG_UNCOND("Testing algorithm performance in realistic networks");
    NS_LOG_UNCOND("================================================");
    NS_LOG_UNCOND("");

    // ================================================================
    // 测试场景 5.1: 数据中心 Fat-Tree (k=4)
    // ================================================================
    {
        NS_LOG_UNCOND("================================================");
        NS_LOG_UNCOND("Test 5." << testNum << "/" << totalTests << ": Data Center (Fat-Tree)");
        NS_LOG_UNCOND("================================================");

        // 重置模拟环境以清除之前的 IP 地址分配
        Simulator::Destroy();
        Ipv4AddressGenerator::Reset();

        uint32_t k = 4;
        uint32_t edges = CreateFatTreeTopology(k);

        // k=4 Fat-Tree 交换机数量
        uint32_t nSwitches = (k / 2) * (k / 2) + k * (k / 2) + k * (k / 2);

        std::string testName = "Exp5_" + std::to_string(testNum);

        RealisticResult result = RunRealisticTest(
            testName, "DataCenter", "Fat-Tree_k4",
            nSwitches, edges, "Dijkstra",
            "k=4 Fat-Tree, typical small DC topology");
        csvFile << result.ToCsv() << std::endl;

        NS_LOG_UNCOND("");
        testNum++;
    }

    // ================================================================
    // 测试场景 5.2: 校园网
    // ================================================================
    {
        NS_LOG_UNCOND("================================================");
        NS_LOG_UNCOND("Test 5." << testNum << "/" << totalTests << ": Campus Network");
        NS_LOG_UNCOND("================================================");

        // 重置模拟环境以清除之前的 IP 地址分配
        Simulator::Destroy();
        Ipv4AddressGenerator::Reset();

        uint32_t nBuildings = 5;
        uint32_t nFloorsPerBuilding = 4;
        uint32_t edges = CreateCampusTopology(nBuildings, nFloorsPerBuilding);

        uint32_t nSwitches = 2 + nBuildings + nBuildings * nFloorsPerBuilding;

        std::string testName = "Exp5_" + std::to_string(testNum);

        RealisticResult result = RunRealisticTest(
            testName, "Campus", "Hierarchical_3-Tier",
            nSwitches, edges, "Dijkstra",
            "3-tier hierarchy: Core-Building-Floor");
        csvFile << result.ToCsv() << std::endl;

        NS_LOG_UNCOND("");
        testNum++;
    }

    // ================================================================
    // 测试场景 5.3: ISP 骨干网
    // ================================================================
    {
        NS_LOG_UNCOND("================================================");
        NS_LOG_UNCOND("Test 5." << testNum << "/" << totalTests << ": ISP Backbone");
        NS_LOG_UNCOND("================================================");

        // 重置模拟环境以清除之前的 IP 地址分配
        Simulator::Destroy();
        Ipv4AddressGenerator::Reset();

        uint32_t nRouters = 20;
        uint32_t degree = 4;
        uint32_t edges = CreateISPTopology(nRouters, degree);

        std::string testName = "Exp5_" + std::to_string(testNum);

        RealisticResult result = RunRealisticTest(
            testName, "ISP", "High-Degree_Mesh",
            nRouters, edges, "Dijkstra",
            "Mesh topology with degree=4");
        csvFile << result.ToCsv() << std::endl;

        NS_LOG_UNCOND("");
        testNum++;
    }

    // ================================================================
    // 可选：大规模测试
    // ================================================================
    if (runLargeTests)
    {
        // 更大的 Fat-Tree
        {
            NS_LOG_UNCOND("================================================");
            NS_LOG_UNCOND("Test 5." << testNum << ": Large Data Center (Fat-Tree k=8)");
            NS_LOG_UNCOND("================================================");

            uint32_t k = 8;
            uint32_t edges = CreateFatTreeTopology(k);

            uint32_t nSwitches = (k / 2) * (k / 2) + k * (k / 2) + k * (k / 2);

            std::string testName = "Exp5_" + std::to_string(testNum);

            RealisticResult result = RunRealisticTest(
                testName, "DataCenter_Large", "Fat-Tree_k8",
                nSwitches, edges, "Dijkstra",
                "k=8 Fat-Tree, larger DC topology");
            csvFile << result.ToCsv() << std::endl;

            NS_LOG_UNCOND("");
            testNum++;
        }
    }

    // ================================================================
    // 关闭 CSV 文件并输出总结
    // ================================================================
    csvFile.close();

    NS_LOG_UNCOND("");
    NS_LOG_UNCOND("================================================");
    NS_LOG_UNCOND("Real-World Scenario Test Completed!");
    NS_LOG_UNCOND("Results saved to: test-realistic-results.csv");
    NS_LOG_UNCOND("");
    NS_LOG_UNCOND("To complete the comparison:");
    NS_LOG_UNCOND("  1. Switch algorithm: m_useBmssp = false (Dijkstra)");
    NS_LOG_UNCOND("  2. Recompile: make build");
    NS_LOG_UNCOND("  3. Run again: make run PROGRAM=test-realistic");
    NS_LOG_UNCOND("  4. Analyze results for each scenario");
    NS_LOG_UNCOND("");
    NS_LOG_UNCOND("Expected Results:");
    NS_LOG_UNCOND("  - Data Center: Breaking should excel (dense topology)");
    NS_LOG_UNCOND("  - Campus: Moderate advantage (hierarchical structure)");
    NS_LOG_UNCOND("  - ISP: Breaking advantage in high-connectivity scenarios");
    NS_LOG_UNCOND("================================================");

    return 0;
}

/* scratch/test-density.cc - Breaking vs Dijkstra Graph Density Test
 *
 * 实验 3：图稠密度影响测试
 * 目标：分析边密度对算法性能的影响
 *
 * 测试场景（固定节点数 n=100）：
 * - 稀疏图 (m ≈ 1.5n)
 * - 中等图 (m ≈ 3n)
 * - 较密图 (m ≈ 5n)
 * - 密集图 (m ≈ 10n)
 * - 很密图 (m ≈ 20n)
 *
 * 预期结果：
 * - 边越多，Breaking 的优势越明显（因为减少了排序开销）
 *
 * 输出：CSV 格式结果到 test-density-results.csv
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv4-address-generator.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <random>
#include <set>
#include <vector>
#include <cmath>
#include <functional>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DensityTest");

// ================================================================
// 结果记录结构
// ================================================================
struct DensityResult
{
    std::string testName;
    std::string topology;
    uint32_t nodes;
    uint32_t edges;
    double density; // m/n ratio
    std::string densityLevel;
    std::string algorithm;
    double time_ms;
    double time_us;
    double speedup; // vs baseline
    int num_runs;        // 运行次数
    double std_dev_ms;   // 标准差（毫秒）

    std::string ToCsv() const
    {
        std::stringstream ss;
        ss << testName << "," << topology << "," << nodes << "," << edges << ","
           << std::fixed << std::setprecision(2) << density << ","
           << densityLevel << "," << algorithm << ","
           << std::fixed << std::setprecision(3) << time_ms << ","
           << std::fixed << std::setprecision(1) << time_us << ","
           << std::fixed << std::setprecision(2) << speedup << ","
           << num_runs << "," << std::fixed << std::setprecision(3) << std_dev_ms;
        return ss.str();
    }
};

// ================================================================
// CSV 头部
// ================================================================
const std::string CSV_HEADER =
    "TestName,Topology,Nodes,Edges,Density,DensityLevel,Algorithm,Time_ms,Time_us,Speedup,NumRuns,StdDev_ms";

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
// 创建随机拓扑
// ================================================================
uint32_t CreateRandomTopology(uint32_t nNodes, double probability,
                               NodeContainer &nodes)
{
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    nodes.Create(nNodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    uint32_t linkIndex = 0;
    uint32_t edgeCount = 0;

    // 使用随机数生成器
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.0, 1.0);

    // 创建边（无向图，只创建 i < j 的边）
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

    // 确保图是连通的（添加最小生成树）
    // 这是一个简单的连通性保证：将所有节点连成一条链
    for (uint32_t i = 0; i < nNodes - 1; i++)
    {
        // 这里简化处理，直接创建新边（可能有重复，但不影响路由）
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

// ================================================================
// 执行密度测试
// ================================================================
DensityResult RunDensityTest(const std::string &testName,
                              const std::string &densityLevel,
                              uint32_t nodes, uint32_t edges,
                              double density,
                              const std::string &algorithm,
                              double probability)
{
    NS_LOG_UNCOND("------------------------------------------------");
    NS_LOG_UNCOND("Running: " << testName << " (" << densityLevel << ")");
    NS_LOG_UNCOND("Nodes: " << nodes << ", Edges: " << edges);
    NS_LOG_UNCOND("Density (m/n): " << std::fixed << std::setprecision(2) << density);
    NS_LOG_UNCOND("Algorithm: " << algorithm);
    NS_LOG_UNCOND("------------------------------------------------");

    // 使用计时函数运行，需要重新创建拓扑因为 PopulateRoutingTables 只能执行一次
    TimingResult timing = TimeFunction([&]() {
        // 每次运行需要重新创建网络拓扑
        Simulator::Destroy();
        Ipv4AddressGenerator::Reset();

        PointToPointHelper p2p;
        p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        p2p.SetChannelAttribute("Delay", StringValue("2ms"));

        NodeContainer tempNodes;
        tempNodes.Create(nodes);

        InternetStackHelper stack;
        stack.Install(tempNodes);

        uint32_t linkIndex = 0;

        // 使用随机数生成器（固定种子以保持一致性）
        std::mt19937 gen(42); // 固定种子
        std::uniform_real_distribution<> dis(0.0, 1.0);

        // 创建边（无向图，只创建 i < j 的边）
        for (uint32_t i = 0; i < nodes; i++)
        {
            for (uint32_t j = i + 1; j < nodes; j++)
            {
                if (dis(gen) < probability)
                {
                    NetDeviceContainer devices = p2p.Install(tempNodes.Get(i), tempNodes.Get(j));

                    Ipv4AddressHelper address;
                    std::stringstream subnet;
                    subnet << "10." << (linkIndex / 256) << "." << (linkIndex % 256) << ".0";
                    address.SetBase(subnet.str().c_str(), "255.255.255.0");

                    Ipv4InterfaceContainer interfaces = address.Assign(devices);
                    linkIndex++;
                }
            }
        }

        // 确保图是连通的（添加最小生成树）
        for (uint32_t i = 0; i < nodes - 1; i++)
        {
            NetDeviceContainer devices = p2p.Install(tempNodes.Get(i), tempNodes.Get(i + 1));

            Ipv4AddressHelper address;
            std::stringstream subnet;
            subnet << "10." << (linkIndex / 256) << "." << (linkIndex % 256) << ".0";
            address.SetBase(subnet.str().c_str(), "255.255.255.0");

            Ipv4InterfaceContainer interfaces = address.Assign(devices);
            linkIndex++;
        }

        // 计算路由表
        Ipv4GlobalRoutingHelper::PopulateRoutingTables();

        // 清理
        Simulator::Destroy();
    });

    DensityResult result;
    result.testName = testName;
    result.topology = "Random";
    result.nodes = nodes;
    result.edges = edges;
    result.density = density;
    result.densityLevel = densityLevel;
    result.algorithm = "Breaking";  // Switched via make breaking/dijkstra
    result.time_ms = timing.avg_time_ms;
    result.time_us = timing.avg_time_us;
    result.num_runs = timing.num_runs;
    result.std_dev_ms = timing.std_dev_ms;
    result.speedup = 1.0; // 需要对比数据计算

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
    LogComponentEnable("DensityTest", LOG_LEVEL_INFO);

    // 命令行参数
    uint32_t nNodes = 100; // 默认节点数
    CommandLine cmd;
    cmd.AddValue("nNodes", "Number of nodes in random graph", nNodes);
    cmd.Parse(argc, argv);

    // 打开 CSV 输出文件
    std::ofstream csvFile("test-density-results.csv");
    csvFile << CSV_HEADER << std::endl;

    NS_LOG_UNCOND("================================================");
    NS_LOG_UNCOND("Experiment 3: Graph Density Test");
    NS_LOG_UNCOND("Testing algorithm performance vs edge density");
    NS_LOG_UNCOND("================================================");
    NS_LOG_UNCOND("");

    // ================================================================
    // 测试场景 3.1-3.5: 不同密度的随机图
    // ================================================================

    // 定义测试密度级别 (连接概率)
    // 对于 n=100 的完全图，最大边数 = 100*99/2 = 4950
    struct DensityConfig
    {
        std::string level;
        double probability; // Erdős-Rényi 连接概率
        double targetDensity; // 目标 m/n 比率
    };

    std::vector<DensityConfig> densityConfigs = {
        {"Sparse", 0.03, 1.5},   // m ≈ 1.5n
        {"Medium", 0.06, 3.0},   // m ≈ 3n
        {"Dense", 0.10, 5.0},    // m ≈ 5n
        {"VeryDense", 0.20, 10.0}, // m ≈ 10n
        {"UltraDense", 0.40, 20.0}  // m ≈ 20n
    };

    int testNum = 1;
    for (const auto &config : densityConfigs)
    {
        NS_LOG_UNCOND("================================================");
        NS_LOG_UNCOND("Test 3." << testNum << ": " << config.level << " Graph");
        NS_LOG_UNCOND("Target density: m/n ≈ " << config.targetDensity);
        NS_LOG_UNCOND("Connection probability: " << config.probability);
        NS_LOG_UNCOND("================================================");

        // 重置模拟环境以清除之前的 IP 地址分配
        Simulator::Destroy();
        Ipv4AddressGenerator::Reset();

        // 计算预期边数（用于记录）
        uint32_t expectedEdges = (uint32_t)(config.targetDensity * nNodes);
        double expectedDensity = config.targetDensity;

        std::string testName = "Exp3_" + std::to_string(testNum);

        // 运行 Breaking 算法测试（内部会创建拓扑并计时）
        DensityResult result = RunDensityTest(
            testName, config.level, nNodes, expectedEdges, expectedDensity, "Breaking", config.probability);
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
    NS_LOG_UNCOND("Density Test Completed!");
    NS_LOG_UNCOND("Results saved to: test-density-results.csv");
    NS_LOG_UNCOND("");
    NS_LOG_UNCOND("To complete the comparison:");
    NS_LOG_UNCOND("  1. Switch algorithm: m_useBmssp = false (Dijkstra)");
    NS_LOG_UNCOND("  2. Recompile: make build");
    NS_LOG_UNCOND("  3. Run again: make run PROGRAM=test-density");
    NS_LOG_UNCOND("  4. Calculate speedup: Time_Dijkstra / Time_Breaking");
    NS_LOG_UNCOND("");
    NS_LOG_UNCOND("Expected Results:");
    NS_LOG_UNCOND("  - Breaking advantage increases with density");
    NS_LOG_UNCOND("  - At high density, Breaking should be significantly faster");
    NS_LOG_UNCOND("================================================");

    return 0;
}

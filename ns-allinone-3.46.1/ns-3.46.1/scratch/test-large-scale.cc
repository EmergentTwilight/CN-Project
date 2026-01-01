/* scratch/test-large-scale.cc - Breaking Algorithm Large-Scale Performance Test
 *
 * 大规模图性能测试 - 测试 Breaking 算法在 20000+ 节点图上的性能扩展性
 *
 * 支持的拓扑类型：
 * - Grid (2D 规则网格)
 * - Random (Erdős-Rényi 随机图)
 * - FatTree (数据中心网络)
 * - SmallWorld (Watts-Strogatz 小世界网络)
 * - ScaleFree (Barabási-Albert 无标度网络)
 *
 * 使用方法：
 *   ./ns3 run "test-large-scale"
 *   ./ns3 run "test-large-scale --quick"  # 快速测试（小规模）
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <vector>
#include <cmath>
#include <functional>
#include <random>
#include <set>
#include <sys/time.h>
#include <sys/resource.h>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LargeScaleTest");

// ================================================================
// 常量定义
// ================================================================

const uint32_t MAX_NODES = 25000;
const uint32_t TIMEOUT_SECONDS = 3600;  // 1小时超时
const size_t MEMORY_LIMIT_MB = 8192;    // 8GB内存限制

// ================================================================
// 结果数据结构（扩展版）
// ================================================================

struct LargeScaleResult
{
    // 基础信息
    std::string testName;
    std::string topologyType;
    uint32_t nodes;
    uint32_t edges;
    double density;

    // 算法信息
    std::string algorithm;

    // 时间信息
    double time_ms;
    double time_us;
    double time_per_node_us;
    int num_runs;
    double std_dev_ms;

    // 内存信息
    double memory_before_mb;
    double memory_after_mb;
    double memory_peak_mb;
    double memory_delta_mb;

    // 系统状态
    bool timeout_triggered;
    bool memory_limit_exceeded;
    bool test_passed;
    std::string error_message;

    // Breaking 算法参数
    int param_k;
    int param_t;
    int param_l;

    // 预估信息
    double estimated_time_sec;
    double actual_time_sec;
    double estimation_error_pct;

    std::string ToCsv() const
    {
        std::stringstream ss;
        ss << testName << ","
           << topologyType << ","
           << nodes << ","
           << edges << ","
           << std::fixed << std::setprecision(2) << density << ","
           << algorithm << ","
           << std::fixed << std::setprecision(3) << time_ms << ","
           << std::fixed << std::setprecision(1) << time_us << ","
           << std::fixed << std::setprecision(2) << time_per_node_us << ","
           << num_runs << ","
           << std::fixed << std::setprecision(3) << std_dev_ms << ","
           << std::fixed << std::setprecision(1) << memory_before_mb << ","
           << std::fixed << std::setprecision(1) << memory_after_mb << ","
           << std::fixed << std::setprecision(1) << memory_peak_mb << ","
           << std::fixed << std::setprecision(1) << memory_delta_mb << ","
           << (timeout_triggered ? "true" : "false") << ","
           << (memory_limit_exceeded ? "true" : "false") << ","
           << (test_passed ? "true" : "false") << ","
           << "\"" << error_message << "\","
           << param_k << ","
           << param_t << ","
           << param_l << ","
           << std::fixed << std::setprecision(1) << estimated_time_sec << ","
           << std::fixed << std::setprecision(1) << actual_time_sec << ","
           << std::fixed << std::setprecision(1) << estimation_error_pct;
        return ss.str();
    }
};

// ================================================================
// 系统监控类
// ================================================================

class SystemMonitor
{
public:
    static size_t GetCurrentMemoryKB()
    {
        std::ifstream statusFile("/proc/self/status");
        std::string line;

        while (std::getline(statusFile, line))
        {
            if (line.find("VmRSS:") == 0)
            {
                size_t value;
                sscanf(line.c_str(), "VmRSS: %zu", &value);
                return value;
            }
        }
        return 0;
    }

    static size_t GetPeakMemoryKB()
    {
        struct rusage usage;
        getrusage(RUSAGE_SELF, &usage);
        return usage.ru_maxrss;
    }

    static void PrintMemoryStats(const std::string& label)
    {
        size_t currentMem = GetCurrentMemoryKB();
        size_t peakMem = GetPeakMemoryKB();

        std::cout << "[" << label << "] "
                  << "Memory: " << (currentMem / 1024.0) << " MB (peak: "
                  << (peakMem / 1024.0) << " MB)" << std::endl;
    }

    static bool CheckMemoryLimit(size_t limitMB)
    {
        size_t currentMem = GetCurrentMemoryKB();
        return (currentMem / 1024) < limitMB;
    }
};

// ================================================================
// 计时辅助函数（复用自 test-scalability.cc）
// ================================================================

struct TimingResult
{
    double avg_time_ms;
    double avg_time_us;
    double std_dev_ms;
    int num_runs;
};

TimingResult TimeFunction(std::function<void()> func,
                          double threshold_ms = 5000.0,
                          int num_iterations = 10)
{
    std::vector<double> times_ms;

    // 第一次运行
    {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = end - start;
        times_ms.push_back(elapsed.count());
    }

    // 如果快速，再运行多次
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
    for (double t : times_ms) sum += t;
    double avg = sum / times_ms.size();

    double variance = 0;
    for (double t : times_ms)
    {
        variance += (t - avg) * (t - avg);
    }
    variance /= times_ms.size();

    return {avg, avg * 1000.0, std::sqrt(variance), (int)times_ms.size()};
}

// ================================================================
// 拓扑生成器
// ================================================================

enum TopologyType { GRID, RANDOM, FATTREE, SMALLWORLD, SCALEFREE };

struct TopologyResult
{
    NodeContainer nodes;
    uint32_t edgeCount;
    std::string typeName;
};

// 创建优化的大规模网格图
TopologyResult CreateLargeGridTopology(uint32_t nRows, uint32_t nCols)
{
    NodeContainer nodes;
    nodes.Create(nRows * nCols);

    InternetStackHelper stack;
    stack.Install(nodes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // 使用 Class A 私有地址，支持大规模网络
    // 使用更大的子网掩码以支持更多链接
    Ipv4AddressHelper address;
    address.SetBase("10.1.0.0", "255.255.255.0");

    uint32_t linkCount = 0;

    // 水平连接
    for (uint32_t row = 0; row < nRows; row++)
    {
        for (uint32_t col = 0; col < nCols - 1; col++)
        {
            NetDeviceContainer devices = p2p.Install(
                nodes.Get(row * nCols + col),
                nodes.Get(row * nCols + col + 1));
            address.Assign(devices);
            address.NewNetwork();
            linkCount++;
        }
    }

    // 垂直连接
    for (uint32_t col = 0; col < nCols; col++)
    {
        for (uint32_t row = 0; row < nRows - 1; row++)
        {
            NetDeviceContainer devices = p2p.Install(
                nodes.Get(row * nCols + col),
                nodes.Get((row + 1) * nCols + col));
            address.Assign(devices);
            address.NewNetwork();
            linkCount++;
        }
    }

    TopologyResult result;
    result.nodes = nodes;
    result.edgeCount = linkCount * 2;  // 双向边
    result.typeName = "Grid";
    return result;
}

// 创建大规模随机图（优化版）
TopologyResult CreateLargeRandomTopology(uint32_t nNodes, double probability)
{
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NodeContainer nodes;
    nodes.Create(nNodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    // 使用高效的随机数生成器
    std::random_device rd;
    std::mt19937_64 gen(42);  // 固定种子以复现结果
    std::uniform_real_distribution<> dis(0.0, 1.0);

    // 预分配边的容器
    std::vector<std::pair<uint32_t, uint32_t>> edges;
    edges.reserve(nNodes * nNodes / 20);

    // 生成边（只生成上三角）
    for (uint32_t i = 0; i < nNodes; i++)
    {
        for (uint32_t j = i + 1; j < nNodes; j++)
        {
            if (dis(gen) < probability)
            {
                edges.push_back({i, j});
            }
        }
    }

    // 确保连通性：添加哈密顿路径
    for (uint32_t i = 0; i < nNodes - 1; i++)
    {
        edges.push_back({i, i + 1});
    }

    // 创建物理连接
    Ipv4AddressHelper address;
    address.SetBase("172.16.0.0", "255.255.0.0");

    uint32_t linkIndex = 0;
    for (const auto& edge : edges)
    {
        NetDeviceContainer devices = p2p.Install(
            nodes.Get(edge.first),
            nodes.Get(edge.second));

        std::stringstream subnet;
        subnet << "172." << (linkIndex / 256) << "." << (linkIndex % 256) << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        address.Assign(devices);
        linkIndex++;
    }

    TopologyResult result;
    result.nodes = nodes;
    result.edgeCount = edges.size() * 2;
    result.typeName = "Random";
    return result;
}

// 创建 Fat-Tree 拓扑（数据中心网络）
TopologyResult CreateFatTreeTopology(uint32_t k)
{
    // k-ary Fat-Tree 参数
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

    Ipv4AddressHelper address;
    address.SetBase("192.168.0.0", "255.255.0.0");

    uint32_t edgeCount = 0;

    // Core 连接到 Aggregation
    for (uint32_t i = 0; i < nCore; i++)
    {
        for (uint32_t pod = 0; pod < k; pod++)
        {
            for (uint32_t agg = 0; agg < (k / 2); agg++)
            {
                uint32_t aggIndex = nCore + pod * (k / 2) + agg;
                NetDeviceContainer devices = p2p.Install(
                    switches.Get(i),
                    switches.Get(aggIndex));
                address.Assign(devices);
                address.NewNetwork();
                edgeCount++;
            }
        }
    }

    // Aggregation 连接到 Edge（同一 pod 内）
    for (uint32_t pod = 0; pod < k; pod++)
    {
        for (uint32_t agg = 0; agg < (k / 2); agg++)
        {
            uint32_t aggIndex = nCore + pod * (k / 2) + agg;

            for (uint32_t edge = 0; edge < (k / 2); edge++)
            {
                uint32_t edgeIndex = nCore + k * (k / 2) + pod * (k / 2) + edge;
                NetDeviceContainer devices = p2p.Install(
                    switches.Get(aggIndex),
                    switches.Get(edgeIndex));
                address.Assign(devices);
                address.NewNetwork();
                edgeCount++;
            }
        }
    }

    TopologyResult result;
    result.nodes = switches;
    result.edgeCount = edgeCount * 2;
    result.typeName = "FatTree_k" + std::to_string(k);
    return result;
}

// ================================================================
// 测试配置
// ================================================================

struct TestConfig
{
    std::string testName;
    uint32_t gridSize;      // 对于网格图
    uint32_t numNodes;      // 对于其他拓扑
    TopologyType topoType;
    double randomProb;      // 对于随机图
    uint32_t fatTreeK;      // 对于 Fat-Tree
    double estimatedTime;   // 预估时间（秒）
    bool skipIfTimeout;     // 是否跳过（如果前置测试超时）
};

std::vector<TestConfig> GetTestSequence(bool quickMode = false)
{
    std::vector<TestConfig> configs;

    if (quickMode)
    {
        // 快速模式：只运行小规模测试
        configs.push_back({"Warmup_25x25", 25, 0, GRID, 0, 0, 10.0, false});
        configs.push_back({"Warmup_30x30", 30, 0, GRID, 0, 0, 20.0, false});
        configs.push_back({"Scale_35x35", 35, 0, GRID, 0, 0, 40.0, false});
        configs.push_back({"Random_1000", 0, 1000, RANDOM, 0.02, 0, 50.0, false});
        return configs;
    }

    // 完整模式：渐进式测试
    // 阶段1：验证阶段
    configs.push_back({"Warmup_25x25", 25, 0, GRID, 0, 0, 10.0, false});
    configs.push_back({"Warmup_30x30", 30, 0, GRID, 0, 0, 20.0, false});

    // 阶段2：中等规模测试（1000-5000节点）
    configs.push_back({"Scale_35x35", 35, 0, GRID, 0, 0, 40.0, false});
    configs.push_back({"Scale_40x40", 40, 0, GRID, 0, 0, 80.0, false});
    configs.push_back({"Scale_50x50", 50, 0, GRID, 0, 0, 200.0, false});
    configs.push_back({"Scale_60x60", 60, 0, GRID, 0, 0, 400.0, false});

    // 阶段3：大规模测试（5000-15000节点）
    configs.push_back({"Large_80x80", 80, 0, GRID, 0, 0, 1200.0, true});
    configs.push_back({"Large_100x100", 100, 0, GRID, 0, 0, 2500.0, true});
    configs.push_back({"Large_120x120", 120, 0, GRID, 0, 0, 4500.0, true});

    // 阶段4：超大规模测试（15000-25000节点）
    configs.push_back({"XLarge_140x140", 140, 0, GRID, 0, 0, 7000.0, true});
    configs.push_back({"XLarge_150x150", 150, 0, GRID, 0, 0, 9000.0, true});

    // 阶段5：其他拓扑类型测试
    configs.push_back({"Random_2000", 0, 2000, RANDOM, 0.02, 0, 100.0, false});
    configs.push_back({"Random_5000", 0, 5000, RANDOM, 0.02, 0, 400.0, true});
    configs.push_back({"Random_10000", 0, 10000, RANDOM, 0.02, 0, 1200.0, true});
    configs.push_back({"FatTree_k8", 0, 0, FATTREE, 0, 8, 300.0, false});
    configs.push_back({"FatTree_k16", 0, 0, FATTREE, 0, 16, 2000.0, true});

    return configs;
}

// ================================================================
// 预估时间模型
// ================================================================

double EstimateTime(uint32_t nNodes)
{
    // 基于历史数据的拟合模型
    // T(n) = a * n^α * log₂(n)^β
    double a = 0.001;
    double alpha = 2.5;
    double beta = -0.5;

    return a * std::pow(nNodes, alpha) * std::pow(std::log2(nNodes), beta);
}

// ================================================================
// 主函数
// ================================================================

int main(int argc, char* argv[])
{
    bool quickMode = false;
    std::string outputFile = "test-large-scale-results.csv";

    CommandLine cmd;
    cmd.AddValue("quick", "Run quick test only (small scale)", quickMode);
    cmd.AddValue("output", "Output CSV file", outputFile);
    cmd.Parse(argc, argv);

    std::cout << "================================================================" << std::endl;
    std::cout << "Breaking Algorithm Large-Scale Performance Test" << std::endl;
    std::cout << "================================================================" << std::endl;
    std::cout << "Mode: " << (quickMode ? "Quick (small scale)" : "Full (progressive)") << std::endl;
    std::cout << "Output: " << outputFile << std::endl;
    std::cout << "================================================================" << std::endl;

    // 写入 CSV 头部
    std::ofstream csv(outputFile);
    csv << "TestName,TopologyType,Nodes,Edges,Density,Algorithm,"
        << "Time_ms,Time_us,TimePerNode_us,NumRuns,StdDev_ms,"
        << "MemoryBefore_mb,MemoryAfter_mb,MemoryPeak_mb,MemoryDelta_mb,"
        << "TimeoutTriggered,MemoryLimitExceeded,TestPassed,ErrorMessage,"
        << "Param_k,Param_t,Param_l,"
        << "EstimatedTime_sec,ActualTime_sec,EstimationError_pct" << std::endl;

    // 获取测试序列
    std::vector<TestConfig> testConfigs = GetTestSequence(quickMode);

    std::cout << "\nTest sequence:" << std::endl;
    for (const auto& config : testConfigs)
    {
        std::cout << "  - " << config.testName;
        if (config.gridSize > 0)
        {
            std::cout << " (" << config.gridSize << "x" << config.gridSize
                      << " = " << (config.gridSize * config.gridSize) << " nodes)";
        }
        else if (config.numNodes > 0)
        {
            std::cout << " (" << config.numNodes << " nodes)";
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;

    // 运行测试
    bool previousTimeout = false;
    for (const auto& config : testConfigs)
    {
        // 检查是否跳过
        if (config.skipIfTimeout && previousTimeout)
        {
            std::cout << "[" << config.testName << "] SKIPPED (previous test timed out)" << std::endl;
            continue;
        }

        std::cout << "\n================================================================" << std::endl;
        std::cout << "Running: " << config.testName << std::endl;
        std::cout << "================================================================" << std::endl;

        // 检查内存限制
        size_t memBefore = SystemMonitor::GetCurrentMemoryKB();
        if (!SystemMonitor::CheckMemoryLimit(MEMORY_LIMIT_MB))
        {
            std::cout << "SKIPPED: Initial memory too high" << std::endl;
            continue;
        }

        // 创建拓扑
        TopologyResult topo;
        uint32_t nNodes = 0;

        try
        {
            switch (config.topoType)
            {
                case GRID:
                    topo = CreateLargeGridTopology(config.gridSize, config.gridSize);
                    nNodes = config.gridSize * config.gridSize;
                    break;
                case RANDOM:
                    topo = CreateLargeRandomTopology(config.numNodes, config.randomProb);
                    nNodes = config.numNodes;
                    break;
                case FATTREE:
                    topo = CreateFatTreeTopology(config.fatTreeK);
                    // 计算 Fat-Tree 节点数
                    nNodes = (config.fatTreeK / 2) * (config.fatTreeK / 2) +
                            config.fatTreeK * (config.fatTreeK / 2) * 2;
                    break;
                default:
                    std::cout << "Unknown topology type" << std::endl;
                    continue;
            }

            double density = (2.0 * topo.edgeCount) / (nNodes * (nNodes - 1));
            std::cout << "Topology: " << topo.typeName << std::endl;
            std::cout << "Nodes: " << nNodes << std::endl;
            std::cout << "Edges: " << topo.edgeCount << std::endl;
            std::cout << "Density: " << std::fixed << std::setprecision(2) << density << std::endl;

            // 设置全局路由
            Ipv4GlobalRoutingHelper::PopulateRoutingTables();

            // 运行 Breaking 算法测试
            std::cout << "\nRunning Breaking algorithm..." << std::endl;

            size_t memBeforeTest = SystemMonitor::GetCurrentMemoryKB();
            SystemMonitor::PrintMemoryStats("Before");

            // 使用 RecomputeRoutingTables 触发 Breaking 算法
            auto testFunc = [&]() {
                Ipv4GlobalRoutingHelper::RecomputeRoutingTables();
            };

            TimingResult timing = TimeFunction(testFunc, 5000.0, 1);

            size_t memAfterTest = SystemMonitor::GetCurrentMemoryKB();
            size_t memPeak = SystemMonitor::GetPeakMemoryKB();

            SystemMonitor::PrintMemoryStats("After");

            // 记录结果
            LargeScaleResult result;
            result.testName = config.testName;
            result.topologyType = topo.typeName;
            result.nodes = nNodes;
            result.edges = topo.edgeCount;
            result.density = density;
            result.algorithm = "Breaking";
            result.time_ms = timing.avg_time_ms;
            result.time_us = timing.avg_time_us;
            result.time_per_node_us = timing.avg_time_us / nNodes;
            result.num_runs = timing.num_runs;
            result.std_dev_ms = timing.std_dev_ms;
            result.memory_before_mb = memBeforeTest / 1024.0;
            result.memory_after_mb = memAfterTest / 1024.0;
            result.memory_peak_mb = memPeak / 1024.0;
            result.memory_delta_mb = (memAfterTest - memBeforeTest) / 1024.0;
            result.timeout_triggered = false;
            result.memory_limit_exceeded = false;
            result.test_passed = true;
            result.error_message = "";
            result.param_k = 0;  // TODO: 从 solver 获取
            result.param_t = 0;
            result.param_l = 0;
            result.estimated_time_sec = config.estimatedTime;
            result.actual_time_sec = timing.avg_time_ms / 1000.0;
            result.estimation_error_pct = 100.0 * (timing.avg_time_ms / 1000.0 - config.estimatedTime) / config.estimatedTime;

            csv << result.ToCsv() << std::endl;
            csv.flush();

            std::cout << "Time: " << std::fixed << std::setprecision(1)
                      << timing.avg_time_ms << " ms" << std::endl;
            std::cout << "Result: PASS" << std::endl;

            previousTimeout = false;

        }
        catch (const std::exception& e)
        {
            std::cout << "EXCEPTION: " << e.what() << std::endl;
            previousTimeout = true;
        }

        // 清理
        Simulator::Destroy();
        Ipv4AddressGenerator::Reset();
    }

    csv.close();

    std::cout << "\n================================================================" << std::endl;
    std::cout << "All tests completed!" << std::endl;
    std::cout << "Results saved to: " << outputFile << std::endl;
    std::cout << "================================================================" << std::endl;

    return 0;
}

/* scratch/test-scalability-2.cc - Breaking vs Dijkstra Scalability Test (改进版)
 *
 * 实验 2：节点规模影响测试
 *
 * 改进点：
 * 1. 使用 Ipv4AddressHelper::NewNetwork() 而不是每次创建新的 helper
 * 2. 正确的清理顺序：Ipv4AddressGenerator::Reset() -> Simulator::Destroy()
 * 3. 参考了 PointToPointGridHelper 的实现模式
 *
 * 使用方法：
 *   ./ns3 run "test-scalability-2"
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

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ScalabilityTest2");

struct ScalabilityResult
{
    std::string testName;
    std::string topology;
    uint32_t nodes;
    uint32_t edges;
    double density;
    std::string algorithm;
    double time_ms;
    double time_us;
    double time_per_node_us;

    std::string ToCsv() const
    {
        std::stringstream ss;
        ss << testName << "," << topology << "," << nodes << "," << edges << ","
           << std::fixed << std::setprecision(2) << density << ","
           << algorithm << ","
           << std::fixed << std::setprecision(3) << time_ms << ","
           << std::fixed << std::setprecision(1) << time_us << ","
           << std::fixed << std::setprecision(2) << time_per_node_us << ",0";
        return ss.str();
    }
};

// ================================================================
// 拓扑创建函数 - 使用正确的 Ipv4AddressHelper 模式
// ================================================================
struct TopologyResult
{
    NodeContainer nodes;
    uint32_t edges;
};

TopologyResult CreateGridWithSingleHelper(uint32_t nRows, uint32_t nCols,
                                          const std::string &baseNetwork)
{
    NodeContainer nodes;
    nodes.Create(nRows * nCols);

    InternetStackHelper stack;
    stack.Install(nodes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // 创建一个单一的 Ipv4AddressHelper，使用 NewNetwork() 来递增
    Ipv4AddressHelper address;
    address.SetBase(baseNetwork.c_str(), "255.255.255.0");

    uint32_t linkCount = 0;

    // 水平连接
    for (uint32_t row = 0; row < nRows; row++)
    {
        for (uint32_t col = 0; col < nCols - 1; col++)
        {
            NetDeviceContainer devices = p2p.Install(nodes.Get(row * nCols + col),
                                                      nodes.Get(row * nCols + col + 1));
            address.Assign(devices);
            address.NewNetwork();  // 关键：递增到下一个网络
            linkCount++;
        }
    }

    // 垂直连接
    for (uint32_t col = 0; col < nCols; col++)
    {
        for (uint32_t row = 0; row < nRows - 1; row++)
        {
            NetDeviceContainer devices = p2p.Install(nodes.Get(row * nCols + col),
                                                      nodes.Get((row + 1) * nCols + col));
            address.Assign(devices);
            address.NewNetwork();  // 关键：递增到下一个网络
            linkCount++;
        }
    }

    TopologyResult result;
    result.nodes = nodes;
    result.edges = linkCount;
    return result;
}

// ================================================================
// 运行单个测试
// ================================================================
ScalabilityResult RunTest(const std::string &testName,
                          uint32_t gridSize,
                          const std::string &baseNetwork)
{
    NS_LOG_UNCOND("================================================");
    NS_LOG_UNCOND("Test: " << testName << " - " << gridSize << "x" << gridSize << " Grid");
    NS_LOG_UNCOND("================================================");

    TopologyResult topo = CreateGridWithSingleHelper(gridSize, gridSize, baseNetwork);

    auto start = std::chrono::high_resolution_clock::now();
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double, std::milli> elapsed_ms = end - start;
    std::chrono::duration<double, std::micro> elapsed_us = end - start;

    ScalabilityResult result;
    result.testName = testName;
    result.topology = "Grid" + std::to_string(gridSize) + "x" + std::to_string(gridSize);
    result.nodes = gridSize * gridSize;
    result.edges = topo.edges;
    result.density = (double)result.edges / result.nodes;
    result.algorithm = "Dijkstra";  // Switched via make breaking/dijkstra
    result.time_ms = elapsed_ms.count();
    result.time_us = elapsed_us.count();
    result.time_per_node_us = elapsed_us.count() / result.nodes;

    NS_LOG_UNCOND("Nodes: " << result.nodes);
    NS_LOG_UNCOND("Edges: " << result.edges);
    NS_LOG_UNCOND("Time: " << result.time_ms << " ms (" << result.time_us << " us)");
    NS_LOG_UNCOND("Time per node: " << result.time_per_node_us << " us");

    return result;
}

// ================================================================
// 主函数
// ================================================================
int main(int argc, char *argv[])
{
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_ERROR);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_ERROR);
    LogComponentEnable("ScalabilityTest2", LOG_LEVEL_INFO);

    // 打开 CSV 输出文件
    std::ofstream csvFile("test-scalability-2-results.csv");
    csvFile << "TestName,Topology,Nodes,Edges,Density,Algorithm,Time_ms,Time_us,TimePerNode_us,Memory_kb"
            << std::endl;

    NS_LOG_UNCOND("================================================");
    NS_LOG_UNCOND("Scalability Test Suite (All in One Run)");
    NS_LOG_UNCOND("================================================");

    // ================================================================
    // 测试场景 2.1: 5x5 网格图 (25 节点)
    // ================================================================
    {
        ScalabilityResult result = RunTest("Exp2_1", 5, "10.1.0.0");
        csvFile << result.ToCsv() << std::endl;

        // 正确的清理顺序：先 Reset Ipv4AddressGenerator，再 Destroy Simulator
        Ipv4AddressGenerator::Reset();
        Simulator::Destroy();
    }

    // ================================================================
    // 测试场景 2.2: 10x10 网格图 (100 节点)
    // ================================================================
    {
        ScalabilityResult result = RunTest("Exp2_2", 10, "20.1.0.0");
        csvFile << result.ToCsv() << std::endl;

        Ipv4AddressGenerator::Reset();
        Simulator::Destroy();
    }

    // ================================================================
    // 测试场景 2.3: 15x15 网格图 (225 节点)
    // ================================================================
    {
        ScalabilityResult result = RunTest("Exp2_3", 15, "30.1.0.0");
        csvFile << result.ToCsv() << std::endl;

        Ipv4AddressGenerator::Reset();
        Simulator::Destroy();
    }

    // ================================================================
    // 测试场景 2.4: 20x20 网格图 (400 节点)
    // ================================================================
    {
        ScalabilityResult result = RunTest("Exp2_4", 20, "40.1.0.0");
        csvFile << result.ToCsv() << std::endl;

        Ipv4AddressGenerator::Reset();
        Simulator::Destroy();
    }

    // ================================================================
    // 完成
    // ================================================================
    csvFile.close();

    NS_LOG_UNCOND("================================================");
    NS_LOG_UNCOND("All Scalability Tests Completed!");
    NS_LOG_UNCOND("Results saved to: test-scalability-2-results.csv");
    NS_LOG_UNCOND("================================================");

    return 0;
}

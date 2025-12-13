/*
 * Breaking the Sorting Barrier - NS-3 Network Simulation Version
 * 
 * 这是真正的NS-3仿真版本，实现了：
 * 1. 创建真实的NS-3网络拓扑
 * 2. 从NS-3拓扑提取Graph用于算法计算
 * 3. 将算法结果注入NS-3路由表
 * 4. 运行仿真验证路由正确性
 */

#include "algorithm-lib/shortest-path-algorithms.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-static-routing.h"
#include <iostream>
#include <iomanip>
#include <map>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("BreakingSortingBarrierNS3");

/**
 * \brief 从NS-3拓扑提取Graph对象
 * 
 * @param nodes NS-3节点容器
 * @param interfaces 所有接口容器（用于获取链路权重）
 * @return Graph对象
 */
Graph ExtractGraphFromNS3(NodeContainer& nodes, 
                          const std::vector<Ipv4InterfaceContainer>& allInterfaces,
                          const std::vector<std::pair<uint32_t, uint32_t>>& edgeMapping) {
    Graph graph;
    
    // 遍历所有链路，提取权重（使用延迟作为权重）
    for (size_t i = 0; i < edgeMapping.size(); ++i) {
        uint32_t from = edgeMapping[i].first;
        uint32_t to = edgeMapping[i].second;
        
        // 获取链路的延迟作为权重（简化：使用固定值或从channel获取）
        // 这里使用延迟的毫秒数作为权重
        double weight = 1.0; // 默认权重，可以从PointToPointChannel获取实际延迟
        
        graph.AddEdge(from, to, weight);
    }
    
    return graph;
}

/**
 * \brief 将算法计算出的路径注入到NS-3路由表
 * 
 * 这是关键函数：把算法结果（predecessors数组）转换成NS-3能理解的路由表项
 * 
 * @param sourceId 源节点ID
 * @param predecessors 算法算出的前驱数组
 * @param nodes NS-3节点容器
 * @param nodeIps 节点ID到IP地址的映射
 */
void InstallRoutesToNS3(uint32_t sourceId,
                        const std::vector<uint32_t>& predecessors,
                        NodeContainer& nodes,
                        const std::vector<Ipv4Address>& nodeIps) {
    
    Ptr<Node> sourceNode = nodes.Get(sourceId);
    Ptr<Ipv4> ipv4 = sourceNode->GetObject<Ipv4>();
    
    if (!ipv4) {
        std::cerr << "Error: Node " << sourceId << " has no IPv4 stack" << std::endl;
        return;
    }
    
    Ipv4StaticRoutingHelper staticRoutingHelper;
    Ptr<Ipv4StaticRouting> staticRouting = staticRoutingHelper.GetStaticRouting(ipv4);
    
    // 遍历所有目标节点
    for (uint32_t destId = 0; destId < nodes.GetN(); ++destId) {
        if (destId == sourceId) continue;
        
        // 检查是否可达（算法使用nodeCount作为"未定义"标记）
        if (predecessors[destId] >= nodes.GetN()) {
            continue; // 不可达
        }
        
        // 从predecessors数组反向推导出从source到dest的下一跳
        uint32_t current = destId;
        uint32_t nextHopId = destId;
        bool pathFound = false;
        
        // 回溯路径找到源节点的直连下一跳
        while (current != sourceId && predecessors[current] < nodes.GetN()) {
            uint32_t prev = predecessors[current];
            if (prev == sourceId) {
                nextHopId = current; // 找到了源节点的直连邻居
                pathFound = true;
                break;
            }
            current = prev;
        }
        
        if (pathFound && nextHopId < nodeIps.size()) {
            // 获取下一跳和目标的IP地址
            Ipv4Address nextHopAddress = nodeIps[nextHopId];
            Ipv4Address destAddress = nodeIps[destId];
            
            // 添加路由表项：去往destAddress，下一跳是nextHopAddress
            // 参数：目标地址，下一跳地址，接口索引（通常p2p链路是1）
            staticRouting->AddHostRouteTo(destAddress, nextHopAddress, 1);
            
            std::cout << "  Route: Node " << sourceId << " -> Node " << destId 
                      << " via Node " << nextHopId << std::endl;
        }
    }
}

/**
 * \brief 创建NS-3网络拓扑并运行仿真
 */
void RunNS3SimulationWithAlgorithm() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "NS-3 Network Simulation with Algorithm" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    // ============================================================
    // 步骤1: 创建NS-3拓扑
    // ============================================================
    std::cout << "Step 1: Creating NS-3 topology..." << std::endl;
    
    // 创建5个节点：0 -> 1 -> 2 -> 3 -> 4 (链式拓扑)
    NodeContainer nodes;
    nodes.Create(5);
    
    // 配置点对点链路
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));
    
    // 创建链路：0-1, 1-2, 2-3, 3-4
    NetDeviceContainer devices01 = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer devices12 = pointToPoint.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer devices23 = pointToPoint.Install(nodes.Get(2), nodes.Get(3));
    NetDeviceContainer devices34 = pointToPoint.Install(nodes.Get(3), nodes.Get(4));
    
    // 安装网络协议栈
    InternetStackHelper stack;
    stack.Install(nodes);
    
    // 分配IP地址
    Ipv4AddressHelper address;
    std::vector<Ipv4InterfaceContainer> allInterfaces;
    std::vector<std::pair<uint32_t, uint32_t>> edgeMapping;
    std::vector<Ipv4Address> nodeIps(nodes.GetN());
    
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer if01 = address.Assign(devices01);
    allInterfaces.push_back(if01);
    edgeMapping.push_back({0, 1});
    edgeMapping.push_back({1, 0});
    nodeIps[0] = if01.GetAddress(0);
    nodeIps[1] = if01.GetAddress(1);
    
    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer if12 = address.Assign(devices12);
    allInterfaces.push_back(if12);
    edgeMapping.push_back({1, 2});
    edgeMapping.push_back({2, 1});
    nodeIps[2] = if12.GetAddress(1);
    
    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer if23 = address.Assign(devices23);
    allInterfaces.push_back(if23);
    edgeMapping.push_back({2, 3});
    edgeMapping.push_back({3, 2});
    nodeIps[3] = if23.GetAddress(1);
    
    address.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer if34 = address.Assign(devices34);
    allInterfaces.push_back(if34);
    edgeMapping.push_back({3, 4});
    edgeMapping.push_back({4, 3});
    nodeIps[4] = if34.GetAddress(1);
    
    std::cout << "  Created " << nodes.GetN() << " nodes with " 
              << edgeMapping.size()/2 << " links" << std::endl;
    
    // ============================================================
    // 步骤2: 从NS-3拓扑提取Graph
    // ============================================================
    std::cout << "\nStep 2: Extracting Graph from NS-3 topology..." << std::endl;
    Graph graph = ExtractGraphFromNS3(nodes, allInterfaces, edgeMapping);
    std::cout << "  Graph extracted: " << graph.GetNodeCount() << " nodes, " 
              << graph.GetEdges().size() << " edges" << std::endl;
    
    // ============================================================
    // 步骤3 & 4: 为网络中的【每一个节点】运行算法并注入路由
    // 模拟真实的OSPF协议：每个路由器都以自己为源点运行Dijkstra
    // ============================================================
    std::cout << "\nStep 3 & 4: Calculating and installing routes for ALL nodes..." << std::endl;
    std::cout << "  Simulating OSPF behavior: each router calculates its own routing table" << std::endl;
    
    // 统计总时间和算法对比（仅对第一个节点进行对比验证）
    double totalDijkstraTime = 0.0;
    double totalBSBTime = 0.0;
    bool algorithmResultsMatch = true;
    
    // 遍历每一个节点，模拟每个路由器都在计算自己的路由表
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        std::cout << "\n  Processing Node " << i << " as source..." << std::endl;
        
        // 1. 以节点 i 为源点运行Dijkstra算法（用于对比验证）
        DijkstraAlgorithm dijkstra;
        auto dijkstraMetrics = dijkstra.RunShortestPath(graph, i);
        auto dijkstraDist = dijkstra.GetDistances();
        auto dijkstraPred = dijkstra.GetPredecessors();
        totalDijkstraTime += dijkstraMetrics.executionTimeMs;
        
        // 2. 以节点 i 为源点运行Breaking Sorting Barrier算法
        BreakingSortingBarrierAlgorithm bsb;
        auto bsbMetrics = bsb.RunShortestPath(graph, i);
        auto bsbDist = bsb.GetDistances();
        auto bsbPred = bsb.GetPredecessors();
        totalBSBTime += bsbMetrics.executionTimeMs;
        
        // 3. 验证算法结果一致性（仅对第一个节点详细输出）
        if (i == 0) {
            bool resultsMatch = true;
            for (size_t j = 0; j < dijkstraDist.size(); ++j) {
                if (std::abs(dijkstraDist[j] - bsbDist[j]) > 1e-6) {
                    resultsMatch = false;
                    algorithmResultsMatch = false;
                    break;
                }
            }
            std::cout << "    Algorithm results match: " << (resultsMatch ? "✓ YES" : "✗ NO") << std::endl;
        } else {
            // 对其他节点也进行验证，但不输出详细信息
            for (size_t j = 0; j < dijkstraDist.size(); ++j) {
                if (std::abs(dijkstraDist[j] - bsbDist[j]) > 1e-6) {
                    algorithmResultsMatch = false;
                    break;
                }
            }
        }
        
        // 4. 将BSB算法计算结果注入到节点 i 的路由表中
        // 注意：这里运行算法的开销会被计入总时间，符合仿真逻辑
        std::cout << "    Installing routes for Node " << i << "..." << std::endl;
        InstallRoutesToNS3(i, bsbPred, nodes, nodeIps);
    }
    
    // 输出统计信息
    std::cout << "\n  Summary:" << std::endl;
    std::cout << "    Total Dijkstra time: " << std::fixed << std::setprecision(3) 
              << totalDijkstraTime << " ms" << std::endl;
    std::cout << "    Total BSB time: " << totalBSBTime << " ms" << std::endl;
    if (totalBSBTime > 0) {
        double speedup = totalDijkstraTime / totalBSBTime;
        std::cout << "    Speedup: " << speedup << "x" << std::endl;
    }
    std::cout << "    All algorithm results match: " 
              << (algorithmResultsMatch ? "✓ YES" : "✗ NO") << std::endl;
    
    // ============================================================
    // 步骤5: 运行NS-3仿真验证路由
    // ============================================================
    std::cout << "\nStep 5: Running NS-3 simulation to verify routes..." << std::endl;
    
    // 在节点4上安装UDP回显服务器
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(4));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));
    
    // 在节点0上安装UDP回显客户端，向节点4发送数据包
    UdpEchoClientHelper echoClient(nodeIps[4], 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(1));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    
    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));
    
    // 启用日志（可选）
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    
    std::cout << "  Starting simulation..." << std::endl;
    
    // 运行仿真
    Simulator::Run();
    Simulator::Destroy();
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "Simulation completed!" << std::endl;
    std::cout << "If you see echo replies, the routes are working correctly." << std::endl;
    std::cout << "========================================" << std::endl;
}

int main(int argc, char* argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);
    
    // 运行NS-3仿真
    RunNS3SimulationWithAlgorithm();
    
    return 0;
}


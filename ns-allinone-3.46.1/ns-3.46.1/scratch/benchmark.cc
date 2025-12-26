/* scratch/benchmark.cc - 高强度网格测试 */
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/point-to-point-layout-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include <iostream>
#include <sstream>
#include <chrono>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("BenchmarkRouting");

int 
main (int argc, char *argv[])
{
  // 启用日志输出以查看 UDP Echo 通信详情
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("BenchmarkRouting", LOG_LEVEL_INFO);
  
  // ======================================================
  // 1. 设置参数：10x10 = 100个节点（高强度测试）
  // ======================================================
  uint32_t nRows = 10;
  uint32_t nCols = 10;
  
  CommandLine cmd;
  cmd.AddValue ("nRows", "Number of rows in grid", nRows);
  cmd.AddValue ("nCols", "Number of columns in grid", nCols);
  cmd.Parse (argc, argv);
  
  uint32_t totalNodes = nRows * nCols;
  NS_LOG_UNCOND ("================================================");
  NS_LOG_UNCOND ("High-Intensity Grid Test: " << nRows << "x" << nCols << " = " << totalNodes << " nodes");
  NS_LOG_UNCOND ("Source: Node 0 (top-left corner)");
  NS_LOG_UNCOND ("Destination: Node " << (totalNodes - 1) << " (bottom-right corner)");
  NS_LOG_UNCOND ("Expected shortest path: " << (nRows - 1 + nCols - 1) << " hops");
  NS_LOG_UNCOND ("================================================");

  // ======================================================
  // 2. 创建网格拓扑
  // ======================================================
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  // 使用网格助手快速构建 10x10 网格
  PointToPointGridHelper grid (nRows, nCols, p2p);
  
  // ======================================================
  // 3. 安装网络协议栈 (这里会调用 BMSSP 算法)
  // ======================================================
  InternetStackHelper stack;
  grid.InstallStack (stack);

  // ======================================================
  // 4. 分配 IP 地址
  // ======================================================
  // 网格拓扑需要两个地址空间：一个用于水平链接，一个用于垂直链接
  grid.AssignIpv4Addresses (Ipv4AddressHelper ("10.1.0.0", "255.255.255.0"),
                            Ipv4AddressHelper ("10.2.0.0", "255.255.255.0"));

  // ======================================================
  // 5. 全局路由计算（BMSSP算法）并计时
  // ======================================================
  NS_LOG_UNCOND ("Starting Routing Table Calculation (BMSSP)...");
  
  auto start = std::chrono::high_resolution_clock::now();
  
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
  
  auto end = std::chrono::high_resolution_clock::now();
  
  // 计算耗时
  std::chrono::duration<double, std::milli> elapsed_ms = end - start;
  std::chrono::duration<double, std::micro> elapsed_us = end - start;
  
  NS_LOG_UNCOND ("Routing Calculation Finished.");
  NS_LOG_UNCOND ("Time Cost: " << elapsed_ms.count() << " ms (" 
                << elapsed_us.count() << " us)");
  NS_LOG_UNCOND ("------------------------------------------------");

  // ======================================================
  // 6. 导出路由表到文件（用于验证）
  // ======================================================
  Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper> ("benchmark-grid.routes", std::ios::out);
  Ipv4GlobalRoutingHelper::PrintRoutingTableAllAt (Seconds (0.1), routingStream);
  NS_LOG_UNCOND ("Routing tables exported to: benchmark-grid.routes");

  // ======================================================
  // 7. UDP Echo 通信测试 - 验证路由是否正确
  // ======================================================
  
  // 选择源节点和目标节点：
  // - 左上角 (0, 0): 使用 GetNode(0, 0)
  // - 右下角 (9, 9): 使用 GetNode(9, 9)
  Ptr<Node> srcNode = grid.GetNode (0, 0);
  Ptr<Node> dstNode = grid.GetNode (nRows - 1, nCols - 1);
  
  // 获取目标节点的 IP 地址
  // 使用 GetIpv4Address 方法直接获取节点的 IP 地址
  Ipv4Address dstAddr = grid.GetIpv4Address (nRows - 1, nCols - 1);
  
  NS_LOG_UNCOND ("------------------------------------------------");
  NS_LOG_UNCOND ("UDP Echo Test Configuration:");
  NS_LOG_UNCOND ("  Source Node: 0 (top-left: 0,0)");
  NS_LOG_UNCOND ("  Destination Node: " << (totalNodes - 1) << " (bottom-right: " 
                << (nRows - 1) << "," << (nCols - 1) << ")");
  NS_LOG_UNCOND ("  Destination IP: " << dstAddr);
  NS_LOG_UNCOND ("  Expected Hops: " << (nRows - 1 + nCols - 1));
  NS_LOG_UNCOND ("------------------------------------------------");

  // 7.1 在目标节点安装 UDP Echo Server (端口 9)
  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (dstNode);
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (20.0));

  // 7.2 在源节点安装 UDP Echo Client
  // 发送 10 个数据包，每个 1024 字节，间隔 1 秒
  UdpEchoClientHelper echoClient (dstAddr, 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (10));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (srcNode);
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (20.0));

  // ======================================================
  // 8. 运行仿真
  // ======================================================
  NS_LOG_UNCOND ("Starting Simulation...");
  NS_LOG_UNCOND ("Watch for 'UdpEchoClient: Received X bytes' messages below.");
  NS_LOG_UNCOND ("If packets are received successfully, BMSSP routing is CORRECT!");
  NS_LOG_UNCOND ("------------------------------------------------");
  
  Simulator::Stop (Seconds (25.0)); // 足够的时间完成所有测试
  Simulator::Run ();
  Simulator::Destroy ();
  
  NS_LOG_UNCOND ("------------------------------------------------");
  NS_LOG_UNCOND ("Simulation Finished.");
  NS_LOG_UNCOND ("================================================");
  NS_LOG_UNCOND ("VERIFICATION CHECKLIST:");
  NS_LOG_UNCOND ("  [ ] Routing calculation completed without errors");
  NS_LOG_UNCOND ("  [ ] Routing tables exported to benchmark-grid.routes");
  NS_LOG_UNCOND ("  [ ] UDP Echo packets sent from Node 0");
  NS_LOG_UNCOND ("  [ ] UDP Echo packets received by Node " << (totalNodes - 1));
  NS_LOG_UNCOND ("  [ ] Echo replies received by Node 0");
  NS_LOG_UNCOND ("");
  NS_LOG_UNCOND ("If you see 'UdpEchoClient: Received 1024 bytes' messages,");
  NS_LOG_UNCOND ("your BMSSP algorithm is working CORRECTLY in complex topology!");
  NS_LOG_UNCOND ("================================================");
  
  return 0;
}

// scratch/large-grid-test.cc
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/point-to-point-layout-module.h"
#include <chrono>
#include <iostream> 

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("LargeGridTest");

int 
main (int argc, char *argv[])
{
  // 1. 设置参数：10x10 = 100个节点
  uint32_t nRows = 10;
  uint32_t nCols = 10;
  
  CommandLine cmd;
  cmd.AddValue ("nRows", "Number of rows", nRows);
  cmd.AddValue ("nCols", "Number of columns", nCols);
  cmd.Parse (argc, argv);

  // 2. 创建网格拓扑
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  // 使用网格助手快速构建
  PointToPointGridHelper grid (nRows, nCols, p2p);
  
  // 3. 安装网络协议栈 (这里会调用我们的 BMSSP 算法)
  InternetStackHelper stack;
  grid.InstallStack (stack);

  // 4. 分配 IP 地址
  grid.AssignIpv4Addresses (Ipv4AddressHelper ("10.1.0.0", "255.255.255.0"),
                            Ipv4AddressHelper ("10.2.0.0", "255.255.255.0"));

  // 5. 开启全局路由并计时
  std::cout << "------------------------------------------------" << std::endl;
  std::cout << "Starting Routing Table Calculation..." << std::endl;

  auto start = std::chrono::high_resolution_clock::now(); // 开始计时

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  auto end = std::chrono::high_resolution_clock::now(); // 结束计时

  // 计算耗时 (微秒和毫秒)
  std::chrono::duration<double, std::milli> elapsed_ms = end - start;
  std::chrono::duration<double, std::micro> elapsed_us = end - start;

  std::cout << "Routing Calculation Finished." << std::endl;
  std::cout << "Time Cost: " << elapsed_ms.count() << " ms (" 
            << elapsed_us.count() << " us)" << std::endl;
  std::cout << "------------------------------------------------" << std::endl;

  NS_LOG_UNCOND("Grid Topology Created: " << nRows << "x" << nCols << " (" << nRows*nCols << " nodes)");
  NS_LOG_UNCOND("Routing Tables Calculated using BMSSP.");

  // 6. 导出路由表以供检查 (可选)
  Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper> ("large-grid.routes", std::ios::out);
  Ipv4GlobalRoutingHelper::PrintRoutingTableAllAt (Seconds (0.1), routingStream);

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}
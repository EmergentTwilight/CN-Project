#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("DijkstraExample");

int main (int argc, char *argv[])
{
  // 1. 启用日志，方便观察
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  // 2. 创建 3 个节点 (Node 0, Node 1, Node 2)
  // 拓扑结构: Node 0 <---> Node 1 <---> Node 2
  NodeContainer nodes;
  nodes.Create (3);

  // 3. 配置链路属性 (相当于边的权重)
  // 这里设置传输速率为 5Mbps，延迟为 2ms
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  // 4. 安装网卡并连接线路
  // 连接 Node 0 和 Node 1
  NetDeviceContainer devices01 = pointToPoint.Install (nodes.Get (0), nodes.Get (1));
  // 连接 Node 1 和 Node 2
  NetDeviceContainer devices12 = pointToPoint.Install (nodes.Get (1), nodes.Get (2));

  // 5. 安装网络协议栈 (TCP/IP)
  InternetStackHelper stack;
  stack.Install (nodes);

  // 6. 分配 IP 地址
  Ipv4AddressHelper address;
  
  // 给 0-1 链路分配 10.1.1.0 网段
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces01 = address.Assign (devices01);

  // 给 1-2 链路分配 10.1.2.0 网段
  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces12 = address.Assign (devices12);

  // =========================================================
  // 【关键步骤】 使用全局路由 (Global Routing)
  // 这行代码会触发 NS-3 内部运行 Dijkstra 算法
  // 计算出 Node 0 到 Node 2 需要经过 Node 1
  // =========================================================
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // 7. 设置应用层流量来验证路由是否通了
  // 在 Node 2 上运行一个 UDP 回显服务器 (端口 9)
  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (2));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  // 在 Node 0 上运行客户端，向 Node 2 发包
  // 注意：Node 2 的 IP 是 interfaces12.GetAddress(1)
  UdpEchoClientHelper echoClient (interfaces12.GetAddress (1), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  // 8. 运行模拟
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}
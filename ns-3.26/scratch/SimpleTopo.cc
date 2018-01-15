/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014 University of Campinas (Unicamp)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Luciano Chaves <luciano@lrc.ic.unicamp.br>
 *         Vitor M. Eichemberger <vitor.marge@gmail.com>
 *
 * Two hosts connected to different OpenFlow switches.
 * Both switches are managed by the default learning controller application.
 *
 *                         SRC Controller
 *                                |
 *                         +-------------+           
 *      Host 1--           |             |           --Host 3
 *              |   +----------+     +----------+   |
 *      Host 0--=== | Switch 0 | === | Switch 1 | ===--Host 4
 *              |   +----------+     +----------+   |
 *      Host 2--                                     --Host 5
 */

#include <ns3/core-module.h>
#include <ns3/network-module.h>
#include <ns3/csma-module.h>
#include <ns3/internet-module.h>
#include <ns3/ofswitch13-module.h>
#include <ns3/internet-apps-module.h>
#include <ns3/flow-monitor-helper.h>
#include <ns3/tap-bridge-module.h>
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/point-to-point-module.h"


using namespace ns3;


void
CheckQueueSize (Ptr<QueueDisc> queue)
{
  uint32_t qSize = StaticCast<RedQueueDisc> (queue)->GetQueueSize ();

  // check queue size every 1/100 of a second
  Simulator::Schedule (Seconds (0.01), &CheckQueueSize, queue);

  std::cout << Simulator::Now ().GetSeconds () << " " << qSize << std::endl;

}


int
main (int argc, char *argv[])
{
  uint16_t simTime = 10;
  bool verbose = false;
  int SubnetNum = 3;
  std::string aredLinkDataRate = "0.5Mbps";
  std::string aredLinkDelay = "20ms";
  uint32_t meanPktSize = 1000;
  bool trace = true;

  // Configure command line parameters
  CommandLine cmd;
  cmd.AddValue ("simTime", "Simulation time (seconds)", simTime);
  cmd.AddValue ("verbose", "Enable verbose output", verbose);
  cmd.AddValue ("trace", "Enable datapath stats and pcap traces", trace);
  cmd.Parse (argc, argv);

  if (verbose)
    {
      OFSwitch13Helper::EnableDatapathLogs ();
      LogComponentEnable ("OFSwitch13Interface", LOG_LEVEL_ALL);
      LogComponentEnable ("OFSwitch13Device", LOG_LEVEL_ALL);
      LogComponentEnable ("OFSwitch13Port", LOG_LEVEL_ALL);
      LogComponentEnable ("OFSwitch13Queue", LOG_LEVEL_ALL);
      LogComponentEnable ("OFSwitch13Controller", LOG_LEVEL_ALL);
      LogComponentEnable ("OFSwitch13LearningController", LOG_LEVEL_ALL);
      LogComponentEnable ("OFSwitch13Helper", LOG_LEVEL_ALL);
      LogComponentEnable ("OFSwitch13InternalHelper", LOG_LEVEL_ALL);
    }

  // Enable checksum computations (required by OFSwitch13 module)
  GlobalValue::Bind ("ChecksumEnabled", BooleanValue (true));


  // Create two host nodes
  NodeContainer Subnet1, Subnet2;
  Subnet1.Create (SubnetNum);
  Subnet2.Create (SubnetNum);

  // Create two switch nodes
  NodeContainer LeftSwitch, RightSwitch;
  LeftSwitch.Create(1);
  RightSwitch.Create(1);


  // redECN params
  Config::SetDefault ("ns3::RedQueueDisc::Mode", StringValue ("QUEUE_MODE_PACKETS"));
  Config::SetDefault ("ns3::RedQueueDisc::MeanPktSize", UintegerValue (meanPktSize));
  Config::SetDefault ("ns3::RedQueueDisc::Wait", BooleanValue (true));
  Config::SetDefault ("ns3::RedQueueDisc::Gentle", BooleanValue (true));
  Config::SetDefault ("ns3::RedQueueDisc::QW", DoubleValue (0.002));
  Config::SetDefault ("ns3::RedQueueDisc::MinTh", DoubleValue (5));
  Config::SetDefault ("ns3::RedQueueDisc::MaxTh", DoubleValue (15));
  Config::SetDefault ("ns3::RedQueueDisc::QueueLimit", UintegerValue (25));
  Config::SetDefault ("ns3::TcpSocketBase::UseEcn", BooleanValue (true));
  Config::SetDefault ("ns3::RedQueueDisc::UseEcn", BooleanValue (true));

  // Install the TCP/IP stack in two Subnet Nodes
  InternetStackHelper internet;
  internet.Install (Subnet1);
  internet.Install (Subnet2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("100Mbps")));
  pointToPoint.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));


  NetDeviceContainer Sub1PairDevices[SubnetNum], Sub2PairDevices[SubnetNum];
  NetDeviceContainer SwitchPairDevice;
  NetDeviceContainer Subnet1Devices, Subnet2Devices;
  NetDeviceContainer LeftSwitchPorts, RightSwitchPorts;


  // Connect Subnet1 to LeftSwitch
  for(int i = 0; i < SubnetNum; i++)
  { 
    Sub1PairDevices[i] = pointToPoint.Install (NodeContainer (Subnet1.Get(i), LeftSwitch.Get(0)));
    Subnet1Devices.Add(Sub1PairDevices[i].Get(0));
    LeftSwitchPorts.Add(Sub1PairDevices[i].Get(1));
  }

  // Connect Subnet2 to RightSwitch
    for(int i = 0; i < SubnetNum; i++)
  {
    Sub2PairDevices[i] = pointToPoint.Install (NodeContainer (Subnet2.Get(i), RightSwitch.Get(0)));
    Subnet2Devices.Add(Sub2PairDevices[i].Get(0));
    RightSwitchPorts.Add(Sub2PairDevices[i].Get(1));
  }

  //Connect the Left and the Right Switches
  SwitchPairDevice = pointToPoint.Install (NodeContainer(LeftSwitch.Get(0), RightSwitch.Get(0)));
  LeftSwitchPorts.Add (SwitchPairDevice.Get (0));
  RightSwitchPorts.Add (SwitchPairDevice.Get (1));


  // Create the controller node
  Ptr<Node> controllerNode = CreateObject<Node> ();

  // Configure the OpenFlow network domain using an external controller
  Ptr<OFSwitch13InternalHelper> of13Helper = CreateObject<OFSwitch13InternalHelper> ();
  //Ptr<NetDevice> ctrlDev = of13Helper->InstallExternalController (controllerNode);
  of13Helper->SetChannelType(OFSwitch13Helper::DEDICATEDP2P);
  of13Helper->InstallController (controllerNode);
  of13Helper->InstallSwitch (LeftSwitch.Get (0), LeftSwitchPorts);
  of13Helper->InstallSwitch (RightSwitch.Get (0), RightSwitchPorts);
  of13Helper->CreateOpenFlowChannels ();


/*
  // TapBridge the controller device to local machine
  // The default configuration expects a controller on local port 6653
  TapBridgeHelper tapBridge;
  tapBridge.SetAttribute ("Mode", StringValue ("ConfigureLocal"));
  tapBridge.SetAttribute ("DeviceName", StringValue ("ctrl"));
  tapBridge.Install (controllerNode, ctrlDev);
*/

  TrafficControlHelper tchRed;
  tchRed.SetRootQueueDisc ("ns3::RedQueueDisc", "LinkBandwidth", StringValue (aredLinkDataRate),
                           "LinkDelay", StringValue (aredLinkDelay));

    
  //Add Red-ECN Flow Monitor on the Switch - Client Links
  for(int i = 0; i < SubnetNum; i++)
  {
    tchRed.Install (Sub1PairDevices[i]);
    tchRed.Install (Sub2PairDevices[i]);
  }
  //Add Red-ECN Flow Monitor on the Switch - Switch Link
  QueueDiscContainer queueDiscs = tchRed.Install(SwitchPairDevice);


  // Set IPv4 host addresses
  //std::ostringstream SubnetIP;
  //SubnetIP<<"0.0.0."<<SubnetNum+1;
  Ipv4AddressHelper ipv4helpr;
  Ipv4InterfaceContainer Subnet1IpIfaces, Subnet2IpIfaces;
  ipv4helpr.SetBase ("10.1.1.0", "255.255.255.0", "0.0.0.1");
  Subnet1IpIfaces = ipv4helpr.Assign (Subnet1Devices);
  ipv4helpr.SetBase ("10.1.1.0", "255.255.255.0", "0.0.0.5");
  Subnet2IpIfaces = ipv4helpr.Assign (Subnet2Devices);
  ipv4helpr.Assign (LeftSwitchPorts);
  ipv4helpr.Assign (RightSwitchPorts);


  // SINK is at the Subnet2
  uint16_t port = 50000;
  Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApp1 = sinkHelper.Install (Subnet2);
  ApplicationContainer sinkApp2 = sinkHelper.Install (Subnet1);
  sinkApp1.Start (Seconds (0.0));
  sinkApp1.Stop (Seconds (40.0));
  sinkApp2.Start (Seconds (0.0));
  sinkApp2.Stop (Seconds (40.0));

  // Connection one
  // Clients are in left side
  /*
   * Create the OnOff applications to send TCP to the server
   * onoffhelper is a client that send data to TCP destination
  */
  OnOffHelper SenderHelper("ns3::TcpSocketFactory", Address(InetSocketAddress(Subnet2IpIfaces.GetAddress(2), port)));
  SenderHelper.SetConstantRate(DataRate("50kbps"));
  ApplicationContainer SenderApp;
  for(int i = 0; i< SubnetNum; i++){
    SenderApp.Add (SenderHelper.Install (Subnet1.Get(i)));
  }
  SenderApp.Start (Seconds (1.0));
  SenderApp.Stop (Seconds (18.0));



  // Enable datapath stats and pcap traces at hosts, switch(es), and controller(s)
  if (trace)
    {
      of13Helper->EnableOpenFlowPcap ("openflow");
      of13Helper->EnableDatapathStats ("switch-stats");
      pointToPoint.EnablePcap ("LeftSwitch", LeftSwitchPorts, true);
      pointToPoint.EnablePcap ("RightSwitch", RightSwitchPorts, true);
      pointToPoint.EnablePcap ("Subnet1", Subnet1Devices);
      pointToPoint.EnablePcap ("Subnet2", Subnet2Devices);
    }

  Simulator::ScheduleNow (&CheckQueueSize, queueDiscs.Get (0));

  // Run the simulation
  Simulator::Run ();
  Simulator::Destroy ();
}


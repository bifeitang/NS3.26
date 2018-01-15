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
#include "ns3/simulator.h"
#include "ns3/nstime.h"
#include "ns3/command-line.h"
#include "ns3/double.h"
#include "ns3/random-variable-stream.h"


using namespace ns3;

/*
void
CheckQueueSize (Ptr<QueueDisc> queue)
{
  uint32_t qSize = StaticCast<RedQueueDisc> (queue)->GetQueueSize ();

  // check queue size every 1/100 of a second
  Simulator::Schedule (Seconds (0.01), &CheckQueueSize, queue);

  std::cout << Simulator::Now ().GetSeconds () << " " << qSize << std::endl;

}
*/

int
main (int argc, char *argv[])
{
  bool verbose = false;
  int BackgroundNum = 6;
  int QueryNum = 80;
  int SubnetNum = BackgroundNum + QueryNum;
  std::string aredLinkDataRate = "500Mbps";
  std::string aredLinkDelay = "10ms";
  //uint32_t meanPktSize = 1000;
  bool trace = true;
  //bool InternalController = true;
  uint32_t ShortBytes = 1024 * 2;
  uint32_t LongBytes = 1024 * 1024 * 1;

  // Configure command line parameters
  CommandLine cmd;
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
  Subnet2.Create (1);

  // Create two switch nodes
  NodeContainer LeftSwitch, RightSwitch;
  LeftSwitch.Create(1);
  RightSwitch.Create(1);

  // Set TCP Protocol
  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpNewReno"));
  Config::SetDefault ("ns3::TcpSocketBase::MinRto", TimeValue (Seconds (10)));



  // Install the TCP/IP stack in two Subnet Nodes
  InternetStackHelper internet;
  internet.Install (Subnet1);
  internet.Install (Subnet2);

  // Use the CsmaHelper to connect hosts and switches
  CsmaHelper csmaHelper;
  csmaHelper.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("1Gbps")));
  csmaHelper.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));

  NetDeviceContainer Sub1PairDevices[SubnetNum], Sub2PairDevices[1];
  NetDeviceContainer SwitchPairDevice;
  NetDeviceContainer Subnet1Devices, Subnet2Devices;
  NetDeviceContainer LeftSwitchPorts, RightSwitchPorts;


  // Connect Subnet1 to LeftSwitch
  for(int i = 0; i < SubnetNum; i++)
  {
    Sub1PairDevices[i] = csmaHelper.Install (NodeContainer (Subnet1.Get(i), LeftSwitch.Get(0)));
    Subnet1Devices.Add(Sub1PairDevices[i].Get(0));
    LeftSwitchPorts.Add(Sub1PairDevices[i].Get(1));
  }

  //Connect the Left and the Right Switches
  SwitchPairDevice = csmaHelper.Install (NodeContainer(LeftSwitch.Get(0), RightSwitch.Get(0)));
  LeftSwitchPorts.Add (SwitchPairDevice.Get (0));
  RightSwitchPorts.Add (SwitchPairDevice.Get (1));


  csmaHelper.SetChannelAttribute ("DataRate", StringValue (aredLinkDataRate));
  csmaHelper.SetChannelAttribute ("Delay", StringValue (aredLinkDelay));

  // Connect Subnet2 to RightSwitch
    for(int i = 0; i < 1; i++)
  {
    Sub2PairDevices[i] = csmaHelper.Install (NodeContainer (Subnet2.Get(i), RightSwitch.Get(0)));
    Subnet2Devices.Add(Sub2PairDevices[i].Get(0));
    RightSwitchPorts.Add(Sub2PairDevices[i].Get(1));
  }


  // Create the controller node
  Ptr<Node> controllerNode = CreateObject<Node> ();

  //if(InternalController){
    Ptr<OFSwitch13InternalHelper> of13Helper = CreateObject<OFSwitch13InternalHelper> ();
    of13Helper->InstallController (controllerNode);
    of13Helper->InstallSwitch (LeftSwitch.Get (0), LeftSwitchPorts);
    of13Helper->InstallSwitch (RightSwitch.Get (0), RightSwitchPorts);
    of13Helper->CreateOpenFlowChannels ();
  //}
/*
  else{
    // Configure the OpenFlow network domain using an external controller
    Ptr<OFSwitch13ExternalHelper> of13Helper = CreateObject<OFSwitch13ExternalHelper> ();
    Ptr<NetDevice> ctrlDev = of13Helper->InstallExternalController (controllerNode);
    of13Helper->InstallSwitch (LeftSwitch.Get (0), LeftSwitchPorts);
    of13Helper->InstallSwitch (RightSwitch.Get (0), RightSwitchPorts);
    of13Helper->CreateOpenFlowChannels ();

    // TapBridge the controller device to local machine
    // The default configuration expects a controller on local port 6653
    TapBridgeHelper tapBridge;
    tapBridge.SetAttribute ("Mode", StringValue ("ConfigureLocal"));
    tapBridge.SetAttribute ("DeviceName", StringValue ("ctrl"));
    tapBridge.Install (controllerNode, ctrlDev);
  }
*/

  TrafficControlHelper tchPfifoFast;
  tchPfifoFast.SetRootQueueDisc ("ns3::PfifoFastQueueDisc", "Limit", UintegerValue (100));



  //Add Red-ECN Flow Monitor on the Switch - Client Links
  for(int i = 0; i < SubnetNum; i++)
  {
    tchPfifoFast.Install (Sub1PairDevices[i]);
  }
  //Add Red-ECN Flow Monitor on the Switch - Switch Link
  tchPfifoFast.Install(SwitchPairDevice);
  tchPfifoFast.Install(Sub2PairDevices[0]);


  // Set IPv4 host addresses
  //std::ostringstream SubnetIP;
  //SubnetIP<<"0.0.0."<<SubnetNum+1;
  Ipv4AddressHelper ipv4helpr;
  Ipv4InterfaceContainer Subnet1IpIfaces, Subnet2IpIfaces;
  ipv4helpr.SetBase ("10.1.1.0", "255.255.255.0", "0.0.0.1");
  Subnet1IpIfaces = ipv4helpr.Assign (Subnet1Devices);
  Subnet2IpIfaces = ipv4helpr.Assign (Subnet2Devices);


  // SINK is at the Subnet2
  uint16_t port = 50000;
  Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
  ApplicationContainer aggregator = sinkHelper.Install (Subnet2.Get(0));
  aggregator.Start (Seconds (0.0));
  aggregator.Stop (Seconds (600.0));

  // Connection one
  // Clients are in left side
  /*
   * Create the OnOff applications to send TCP to the server
   * onoffhelper is a client that send data to TCP destination
  */

  Ptr<WeibullRandomVariable> StartTime = CreateObject<WeibullRandomVariable> ();
  StartTime->SetAttribute ("Scale", DoubleValue (1.0));
  StartTime->SetAttribute ("Shape", DoubleValue (1.0));
  StartTime->SetAttribute ("Bound", DoubleValue (10.0));

  BulkSendHelper ShortSource ("ns3::TcpSocketFactory",
                         InetSocketAddress (Subnet2IpIfaces.GetAddress(0), port));
  // Set the amount of data to send in bytes.  Zero is unlimited.

  ApplicationContainer ShortApps[QueryNum];
  ShortSource.SetAttribute ("MaxBytes", UintegerValue(ShortBytes));
  for (int i = 0; i < QueryNum; ++i)
  {
    ShortApps[i] = ShortSource.Install (Subnet1.Get (i));
    ShortApps[i].Start(Seconds(20.0));
    ShortApps[i].Stop(Seconds(80.0));
  }


  BulkSendHelper LongSource ("ns3::TcpSocketFactory",
                         InetSocketAddress (Subnet2IpIfaces.GetAddress(0), port));
  ApplicationContainer LongApps[BackgroundNum];
  LongSource.SetAttribute ("MaxBytes", UintegerValue(LongBytes));
  for (int i = QueryNum; i < SubnetNum; ++i)
  {
    LongApps[i-QueryNum] = LongSource.Install (Subnet1.Get(i));
    LongApps[i-QueryNum].Start(Seconds(StartTime->GetValue()));
    LongApps[i-QueryNum].Stop(Seconds(100.0));
  }



  // Enable datapath stats and pcap traces at hosts, switch(es), and controller(s)
  if (trace)
    {
      of13Helper->EnableOpenFlowPcap ("openflow");
      of13Helper->EnableDatapathStats ("switch-stats");
      csmaHelper.EnablePcap ("LeftSwitch", LeftSwitchPorts, true);
      csmaHelper.EnablePcap ("RightSwitch", RightSwitchPorts, true);
      csmaHelper.EnablePcap ("Subnet1", Subnet1Devices);
      csmaHelper.EnablePcap ("Aggregator", Subnet2Devices);
    }

  //Simulator::ScheduleNow (&CheckQueueSize, queueDiscs.Get (0));

  // Run the simulation
  Simulator::Run ();
  Simulator::Destroy ();
}


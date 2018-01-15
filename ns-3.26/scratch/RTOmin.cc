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


using namespace ns3;

#define KARY      4

int
main (int argc, char *argv[])
{
  uint16_t simTime = 10;
  bool verbose = false;
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

  Config::SetDefault ("ns3::RttEstimator::InitialEstimation", TimeValue (MilliSeconds (1)));

  // Enable checksum computations (required by OFSwitch13 module)
  GlobalValue::Bind ("ChecksumEnabled", BooleanValue (true));


  // Create Clients
  NodeContainer Clients;
  Clients.Create(KARY*KARY*KARY/4);

  // Create Switches Nodes
  NodeContainer CoreSwitches, AggSwitches, EdgeSwitches;
  CoreSwitches.Create(KARY*KARY/4);
  AggSwitches.Create(KARY*KARY/2);
  EdgeSwitches.Create(KARY*KARY/2);

  // Set TCP Protocol
  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpNewReno"));


  // Install the TCP/IP stack in two Subnet Nodes
  InternetStackHelper internet;
  internet.Install (Clients);

  // Use the CsmaHelper to connect hosts and switches
  CsmaHelper csmaHelper;
  csmaHelper.SetQueue ("ns3::DropTailQueue", "Mode", StringValue ("QUEUE_MODE_PACKETS"), "MaxPackets", UintegerValue (100));

  NetDeviceContainer CoreSwitchesDevices[KARY*KARY/4];
  NetDeviceContainer AggSwitchesDevices[KARY*KARY/2];
  NetDeviceContainer EdgeSwitchesDevices[KARY*KARY/2];
  NetDeviceContainer ClientDevices[KARY*KARY*KARY/4];
  QueueDiscContainer queueDiscs;

  //Add Links between CoreSwitches and AggSwitches
  std::vector<NetDeviceContainer> CADevice (KARY*KARY*KARY/4);
  for(int pod = 0; pod < KARY; pod++)
  {
      for(int i= 0; i < KARY/2; i++)
      {
          for(int j = 0; j< KARY/2; j++)
          {
        	  csmaHelper.SetQueue("ns3::DropTailQueue");
        	  csmaHelper.SetChannelAttribute ("DataRate", StringValue ("1Mbps"));
        	  csmaHelper.SetChannelAttribute ("Delay", StringValue ("5ms"));
            CADevice[pod*KARY*KARY/4+i*KARY/2+j] = csmaHelper.Install(NodeContainer(CoreSwitches.Get(i*KARY/2+j), AggSwitches.Get(pod*KARY/2+i)));
            CoreSwitchesDevices[i*KARY/2+j].Add(CADevice[pod*KARY*KARY/4+i*KARY/2+j].Get(0));
            AggSwitchesDevices[pod*KARY/2+i].Add(CADevice[pod*KARY*KARY/4+i*KARY/2+j].Get(1));
          }
      }
  }

  //Add Links between AggSwitches and EdgeSwitches
  std::vector<NetDeviceContainer> AEDevice (KARY*KARY*KARY/4);
    for(int pod = 0; pod < KARY; pod++)
    {
        for(int i= 0; i < KARY/2; i++)
        {
            for(int j = 0; j< KARY/2; j++)
            {
            	csmaHelper.SetQueue("ns3::DropTailQueue");
            	csmaHelper.SetChannelAttribute ("DataRate", StringValue ("1Mbps"));
            	csmaHelper.SetChannelAttribute ("Delay", StringValue ("4ms"));
              AEDevice[pod*KARY*KARY/4+i*KARY/2+j] = csmaHelper.Install(NodeContainer(AggSwitches.Get(pod*KARY/2+i), EdgeSwitches.Get(pod*KARY/2+j)));
              AggSwitchesDevices[pod*KARY/2+i].Add(AEDevice[pod*KARY*KARY/4+i*KARY/2+j].Get(0));
              EdgeSwitchesDevices[pod*KARY/2+j].Add(AEDevice[pod*KARY*KARY/4+i*KARY/2+j].Get(1));
            }
        }
    }


  //Add Links between EdgeSwitches and Clients
  std::vector<NetDeviceContainer> ECDevice (KARY*KARY*KARY/4);
  for(int pod = 0; pod < KARY; pod++)
  {
      for(int i= 0; i < KARY/2; i++)
      {
          for(int j = 0; j< KARY/2; j++)
          {
        	  csmaHelper.SetQueue("ns3::DropTailQueue");
        	  csmaHelper.SetChannelAttribute ("DataRate", StringValue ("0.5Mbps"));
        	  csmaHelper.SetChannelAttribute ("Delay", StringValue ("2ms"));
            ECDevice[pod*KARY*KARY/4+i*KARY/2+j] = csmaHelper.Install(NodeContainer(EdgeSwitches.Get(pod*KARY/2+i), Clients.Get(pod*KARY*KARY/4+i*KARY/2+j)));
            EdgeSwitchesDevices[pod*KARY/2+i].Add(ECDevice[pod*KARY*KARY/4+i*KARY/2+j].Get(0));
            ClientDevices[pod*KARY*KARY/4+i*KARY/2+j].Add(ECDevice[pod*KARY*KARY/4+i*KARY/2+j].Get(1));
          }
      }
  }


  // Create the controller node
  Ptr<Node> controllerNode = CreateObject<Node> ();

  // Configure the OpenFlow network domain using an external controller
  Ptr<OFSwitch13ExternalHelper> of13Helper = CreateObject<OFSwitch13ExternalHelper> ();
  Ptr<NetDevice> ctrlDev = of13Helper->InstallExternalController (controllerNode);
  for (int i = 0; i < KARY*KARY/4; ++i)
  {
    of13Helper->InstallSwitch (CoreSwitches.Get(i), CoreSwitchesDevices[i]);
  }
  for (int i = 0; i < KARY*KARY/2; ++i)
  {
    of13Helper->InstallSwitch (AggSwitches.Get(i), AggSwitchesDevices[i]);
  }
  for (int i = 0; i < KARY*KARY/2; ++i)
  {
    of13Helper->InstallSwitch (EdgeSwitches.Get(i), EdgeSwitchesDevices[i]);
  }
  of13Helper->CreateOpenFlowChannels ();

  // TapBridge the controller device to local machine
  // The default configuration expects a controller on local port 6653
  TapBridgeHelper tapBridge;
  tapBridge.SetAttribute ("Mode", StringValue ("ConfigureLocal"));
  tapBridge.SetAttribute ("DeviceName", StringValue ("ctrl"));
  tapBridge.Install (controllerNode, ctrlDev);

/*
  TrafficControlHelper tchPfifo;
  uint16_t handle = tchPfifo.SetRootQueueDisc ("ns3::PfifoFastQueueDisc");
  //tchPfifo.AddInternalQueues (handle, 3, "ns3::DropTailQueue", "MaxPackets", UintegerValue (100));
  tchPfifo.AddInternalQueues (handle, 3, "ns3::DropTailQueue", "MaxPackets", UintegerValue (500));

  //Add Red-ECN Flow Monitor on the CoreSwitches - AggSwitches Links
  for(int i = 0; i < KARY*KARY*KARY/4; i++)
  {
    tchPfifo.Install (CADevice[i]);
  }
  //Add Red-ECN Flow Monitor on the AggSwitches - EdgeSwitches Links
  for(int i = 0; i < KARY*KARY*KARY/4; i++)
  {
    tchPfifo.Install (AEDevice[i]);
  }
  //Add Red-ECN Flow Monitor on the EdgeSwitches - Clients Links
  for(int i = 0; i < KARY*KARY*KARY/4; i++)
  {
    tchPfifo.Install (ECDevice[i]);
  }
*/

  // Set IPv4 host addresses
  //std::ostringstream SubnetIP;
  //SubnetIP<<"0.0.0."<<SubnetNum+1;
  Ipv4AddressHelper ipv4helpr;
  Ipv4InterfaceContainer ClientInterfaces;
  ipv4helpr.SetBase ("10.1.1.0", "255.255.255.0");
  for(int i = 0; i < KARY*KARY*KARY/4; i++)
  {
	  ClientInterfaces = ipv4helpr.Assign (ClientDevices[i]);
  }
  for(int i = 0; i < KARY*KARY/4; i++)
  {
	  ipv4helpr.Assign(CoreSwitchesDevices[i]);
  }
  for(int i = 0; i < KARY*KARY/2; i++)
  {
	  ipv4helpr.Assign(AggSwitchesDevices[i]);
  }
  for(int i = 0; i < KARY*KARY/2; i++)
  {
	  ipv4helpr.Assign(EdgeSwitchesDevices[i]);
  }

  // SINK is at the Subnet2
  uint16_t port = 50000;
  Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApp = sinkHelper.Install (Clients);
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (40.0));

  // Connection one
  // Clients are in left side
  /*
   * Create the OnOff applications to send TCP to the server
   * onoffhelper is a client that send data to TCP destination
  */
  OnOffHelper SenderHelper("ns3::TcpSocketFactory", Address(InetSocketAddress(ClientInterfaces.GetAddress(0), port)));
  SenderHelper.SetAttribute ("DataRate", DataRateValue (DataRate ("10Mb/s")));
  ApplicationContainer SenderApp;
  for(int i = 0; i< KARY*KARY*KARY/4 -1; i++){
    SenderApp.Add (SenderHelper.Install (Clients.Get(i)));
  }
  SenderApp.Start (Seconds (1.0));
  SenderApp.Stop (Seconds (18.0));



  // Enable datapath stats and pcap traces at hosts, switch(es), and controller(s)
  if (trace)
    {
      of13Helper->EnableOpenFlowPcap ("openflow");
      of13Helper->EnableDatapathStats ("switch-stats");
      for(int i = 0; i < KARY*KARY/4; ++i){
    	 csmaHelper.EnablePcap ("CoreSwitches", CoreSwitchesDevices[i], true);
      }
      for(int i = 0; i < KARY*KARY/2; ++i){
    	  csmaHelper.EnablePcap ("AggSwitches", AggSwitchesDevices[i], true);
      }
      for(int i = 0; i < KARY*KARY/2; ++i){
    	  csmaHelper.EnablePcap ("EdgeSwitches", EdgeSwitchesDevices[i], true);
      }
      for(int i = 0; i < KARY*KARY*KARY/4; ++i){
    	  csmaHelper.EnablePcap ("Clients", ClientDevices[i], true);
      }
    }

  // Run the simulation
  Simulator::Run ();
  Simulator::Destroy ();
}


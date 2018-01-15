#include <ns3/core-module.h>
#include <ns3/network-module.h>
#include <ns3/csma-module.h>
#include <ns3/internet-module.h>
#include "ns3/applications-module.h"

using namespace ns3;

int
main (int argc, char *argv[])
{
  // Configure command line parameters
  CommandLine cmd;
  cmd.Parse (argc, argv);

  NodeContainer Clients;
  Clients.Create(2);

  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpNewReno"));

  CsmaHelper csmaHelper;
  NetDeviceContainer csmaDevice;
  csmaHelper.SetQueue("ns3::DropTailQueue");
  csmaHelper.SetChannelAttribute ("DataRate", StringValue ("10Mbps"));
  csmaHelper.SetChannelAttribute ("Delay", StringValue ("20ms"));
  csmaDevice = csmaHelper.Install(Clients);
  
  InternetStackHelper internet;
  internet.Install (Clients);

  Ipv4AddressHelper ipv4helpr;
  Ipv4InterfaceContainer ClientInterfaces;
  ipv4helpr.SetBase ("10.1.1.0", "255.255.255.0");
  ClientInterfaces = ipv4helpr.Assign (csmaDevice);

  uint16_t port = 50000;
  Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApp = sinkHelper.Install (Clients);
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (40.0));

  OnOffHelper SenderHelper("ns3::TcpSocketFactory", Address(InetSocketAddress(ClientInterfaces.GetAddress(0), port)));
  SenderHelper.SetAttribute ("DataRate", DataRateValue (DataRate ("10Mb/s")));
  ApplicationContainer SenderApp;
  SenderApp.Add(SenderHelper.Install(Clients.Get(1)));
  SenderApp.Start(Seconds(1.0));
  SenderApp.Stop (Seconds (18.0));

  csmaHelper.EnablePcap ("ThroughputTest", csmaDevice, true);

  // Run the simulation
  Simulator::Run ();
  Simulator::Destroy ();
}

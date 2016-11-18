/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 */

#include "ns3/vector.h"
#include "ns3/string.h"
#include "ns3/socket.h"
#include "ns3/double.h"
#include "ns3/config.h"
#include "ns3/log.h"
#include "ns3/command-line.h"
#include "ns3/mobility-model.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/position-allocator.h"
#include "ns3/mobility-helper.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-interface-container.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"

#include <iostream>

#include "ns3/ocb-wifi-mac.h"
#include "ns3/wifi-80211p-helper.h"
#include "ns3/wave-mac-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("trabalho_tio_arthur");

/*
 * In WAVE module, there is no net device class named like "Wifi80211pNetDevice",
 * instead, we need to use Wifi80211pHelper to create an object of
 * WifiNetDevice class.
 *
 * usage:
 *  NodeContainer nodes;
 *  NetDeviceContainer devices;
 *  nodes.Create (2);
 *  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
 *  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
 *  wifiPhy.SetChannel (wifiChannel.Create ());
 *  NqosWaveMacHelper wifi80211pMac = NqosWave80211pMacHelper::Default();
 *  Wifi80211pHelper wifi80211p = Wifi80211pHelper::Default ();
 *  devices = wifi80211p.Install (wifiPhy, wifi80211pMac, nodes);
 *
 * The reason of not providing a 802.11p class is that most of modeling
 * 802.11p standard has been done in wifi module, so we only need a high
 * MAC class that enables OCB mode.
 */


int main (int argc, char *argv[])
{
  std::string phyMode ("OfdmRate6MbpsBW10MHz");
  uint32_t packetSize = 1000; // bytes
  uint32_t numPackets = 3;
  uint32_t N = 9;
  double interval = 1.0; // seconds
  bool verbose = false;

  CommandLine cmd;

  cmd.AddValue ("phyMode", "Wifi Phy mode", phyMode);
  cmd.AddValue ("packetSize", "size of application packet sent", packetSize);
  cmd.AddValue ("numPackets", "number of packets generated", numPackets);
  cmd.AddValue ("interval", "interval (seconds) between packets", interval);
  cmd.AddValue ("verbose", "turn on all WifiNetDevice log components", verbose);
  cmd.Parse (argc, argv);
  Time interPacketInterval = Seconds (interval);

  NodeContainer serverNode;
  NodeContainer clientNodes;
  NodeContainer c;
  c.Create(21);

  // The below set of helpers will help us to put together the wifi NICs we want
  YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();

  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss ("ns3::NakagamiPropagationLossModel");
  Ptr<YansWifiChannel> channel = wifiChannel.Create ();
  wifiPhy.SetChannel (channel);

  // ns-3 supports generate a pcap trace
  wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11);
  NqosWaveMacHelper wifi80211pMac = NqosWaveMacHelper::Default ();
  Wifi80211pHelper wifi80211p = Wifi80211pHelper::Default ();

  if (verbose)
    {
      wifi80211p.EnableLogComponents ();
    }

  wifi80211p.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                      "DataMode",StringValue (phyMode),
                                      "ControlMode",StringValue (phyMode));
  NetDeviceContainer devices = wifi80211p.Install (wifiPhy, wifi80211pMac, c);

  // Tracing
  wifiPhy.EnablePcap ("wave-simple-80211p", devices);

  MobilityHelper mobility;
  // Modelo de mobilidade
  int m_streamIndex = 0;
  ObjectFactory pos;
  pos.SetTypeId ("ns3::RandomBoxPositionAllocator");
  pos.Set ("X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=1457.0]"));
  pos.Set ("Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=300.0]"));
  // we need antenna height uniform [1.0 .. 2.0] for loss model
  pos.Set ("Z", StringValue ("ns3::UniformRandomVariable[Min=1.0|Max=2.0]"));

  Ptr<PositionAllocator> taPositionAlloc = pos.Create ()->GetObject<PositionAllocator> ();
  m_streamIndex += taPositionAlloc->AssignStreams (m_streamIndex);

  // Aqui voce pode mudar a velocidade e verificar o PDR
  // Testar com velocidades 10, 50 e 100km/h
  std::stringstream ssSpeed;
  ssSpeed << "ns3::UniformRandomVariable[Min=0.0|Max=" << 3 << "]";
  std::stringstream ssPause;
  ssPause << "ns3::ConstantRandomVariable[Constant=" << 5 << "]";
  mobility.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
                                  "Speed", StringValue (ssSpeed.str ()),
                                  "Pause", StringValue (ssPause.str ()),
                                  "PositionAllocator", PointerValue (taPositionAlloc));

  mobility.SetPositionAllocator (taPositionAlloc);
  mobility.Install (c);

  // Instalando a pilha de protocolos
  InternetStackHelper internet;
  internet.Install (c);

  std::vector<NetDeviceContainer> deviceAdjacencyList (N-1);
  std::vector<NodeContainer> nodeAdjacencyList (N-1);

  // servico DHCP
  Ipv4AddressHelper ipv4;
  NS_LOG_INFO ("Assign IP Addresses.");
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign (devices);

  //Criando aplicacao servidor
  // Lembrar de mudar o tempo e tamanho do pacote
  uint16_t port = 50000;
  Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApp = sinkHelper.Install (c.Get(0));
  sinkApp.Start (Seconds (1.0));
  sinkApp.Stop (Seconds (300.0));

  // Criando a aplicacao cliente
  Config::SetDefault ("ns3::OnOffApplication::PacketSize", UintegerValue (1025));

  OnOffHelper clientHelper ("ns3::TcpSocketFactory", Address ());
  clientHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  clientHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  clientHelper.SetAttribute ("MaxBytes", UintegerValue (1024));

  // Instalando as applicacoes
  ApplicationContainer clientApps;
  for (size_t x = 1; x < 21; x++) {
    AddressValue remoteAddress
      (InetSocketAddress (i.GetAddress (0), port));
    clientHelper.SetAttribute ("Remote", remoteAddress);
    clientApps.Add (clientHelper.Install (c.Get (x)));
  }

  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (300.0));

  // Habilitando trace
  AsciiTraceHelper ascii;
  wifiPhy.EnableAsciiAll (ascii.CreateFileStream ("resultados.tr"));
  //wifiPhy.EnablePcapAll ("resultados-vanets-tcp");
  MobilityHelper::EnableAsciiAll (ascii.CreateFileStream ("mobilidade_vanets_tcp.mob"));

  Simulator::Stop (Seconds (500.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}

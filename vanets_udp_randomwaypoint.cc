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

#include <iostream>
#include "ns3/ocb-wifi-mac.h"
#include "ns3/wifi-80211p-helper.h"

#include "ns3/wave-mac-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("trabalho_tio_arthur");

int main (int argc, char *argv[])
{
  std::ofstream m_os;
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  std::string phyMode ("OfdmRate6MbpsBW10MHz");
  uint32_t packetSize = 1000; // bytes
  uint32_t numPackets = 3;
  double interval = 1.0; // seconds
  bool verbose = false;

  CommandLine cmd;
  cmd.AddValue ("phyMode", "Wifi Phy mode", phyMode);
  cmd.AddValue ("packetSize", "size of application packet sent", packetSize);
  cmd.AddValue ("numPackets", "number of packets generated", numPackets);
  cmd.AddValue ("interval", "interval (seconds) between packets", interval);
  cmd.AddValue ("verbose", "turn on all WifiNetDevice log components", verbose);
  cmd.Parse (argc, argv);
  // Convert to time object
  Time interPacketInterval = Seconds (interval);

  NodeContainer c;
  c.Create (21);

  // Ajustando Wifi NICs, Modelo de perda de propagacao
  YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss ("ns3::NakagamiPropagationLossModel");

  Ptr<YansWifiChannel> channel = wifiChannel.Create ();
  wifiPhy.SetChannel (channel);

  // Habilitando trace
  wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11);
  NqosWaveMacHelper wifi80211pMac = NqosWaveMacHelper::Default ();
  Wifi80211pHelper wifi80211p = Wifi80211pHelper::Default ();

  if (verbose)
    {
      wifi80211p.EnableLogComponents ();      // Turn on all Wifi 802.11p logging
    }

  wifi80211p.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                      "DataMode",StringValue (phyMode),
                                      "ControlMode",StringValue (phyMode));
  NetDeviceContainer devices = wifi80211p.Install (wifiPhy, wifi80211pMac, c);

  // Gera trace
  wifiPhy.EnablePcap ("wave-simple-80211p", devices);

  MobilityHelper mobility;
  /*Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));
  positionAlloc->Add (Vector (5.0, 0.0, 0.0));
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  */

  // Modelo de mobilidade
  int m_streamIndex = 0;
  ObjectFactory pos;
  pos.SetTypeId ("ns3::RandomBoxPositionAllocator");
  pos.Set ("X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=3101.0]"));
  pos.Set ("Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=300.0]"));
// Mudei aqui
  pos.Set ("Z", StringValue ("ns3::UniformRandomVariable[Min=1.0|Max=500.0]"));

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

  // Instalando pilha de protocolos
  InternetStackHelper internet;
  internet.Install (c);

  // Habilitando trace ascii
  AsciiTraceHelper ascii;
  Ptr<OutputStreamWrapper> osw = ascii.CreateFileStream (("udp_trace.tr"));
  wifiPhy.EnableAsciiAll (osw);

  // Criando trace ascii para mobilidade
  MobilityHelper::EnableAsciiAll (ascii.CreateFileStream ("mobilidade.mob"));

  // Habilitando servico DHCP
  Ipv4AddressHelper ipv4;
  NS_LOG_INFO ("Assign IP Addresses.");
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign (devices);

  // Criando aplicacao servico udp echoServer
  UdpEchoServerHelper echoServer (9);

  ApplicationContainer serverApps = echoServer.Install (c.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (300.0));


  // Criando aplicacao servico udp echoClient
  UdpEchoClientHelper echoClient (i.GetAddress (0), 9);

  echoClient.SetAttribute ("MaxPackets", UintegerValue (30));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
  //Setando limites e atributos para a simulacao
  ApplicationContainer clientApps;

  // Instalando aplicacao nos clientes
  for (size_t x = 1; x < 21; x++) {
        clientApps.Add (echoClient.Install (c.Get (x)));

  }
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (300.0));

  Simulator::Stop (Seconds (500.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}

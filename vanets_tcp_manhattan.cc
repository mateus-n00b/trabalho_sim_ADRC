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
#include "ns3/ns2-mobility-helper.h"
#include "ns3/netanim-module.h"

#include <iostream>

#include "ns3/ocb-wifi-mac.h"
#include "ns3/wifi-80211p-helper.h"
#include "ns3/wave-mac-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("trabalho_tio_arthur");

static void
CourseChange (std::ostream *os, std::string foo, Ptr<const MobilityModel> mobility)
{
  Vector pos = mobility->GetPosition (); // Get position
  Vector vel = mobility->GetVelocity (); // Get velocity

  // Prints position and velocities
  *os << Simulator::Now () << " POS: x=" << pos.x << ", y=" << pos.y
      << ", z=" << pos.z << "; VEL:" << vel.x << ", y=" << vel.y
      << ", z=" << vel.z << std::endl;
}


int main (int argc, char *argv[])
{
  std::string phyMode ("OfdmRate6MbpsBW10MHz");
  // uint32_t packetSize = 1000; // bytes
  // uint32_t numPackets = 3;
   uint32_t N = 23;
  double interval = 1.0; // seconds

  std::string traceFile;
  std::string logFile;

  // Enable logging from the ns2 helper
  LogComponentEnable ("Ns2MobilityHelper",LOG_LEVEL_DEBUG);

  // Parse command line attribute
  CommandLine cmd;
  cmd.AddValue ("traceFile", "Ns2 movement trace file", traceFile);
  cmd.AddValue ("logFile", "Log file", logFile);
  cmd.Parse (argc,argv);

  // Check command line arguments
  if (traceFile.empty () || logFile.empty ())
    {
      std::cout << "Usage of " << argv[0] << " :\n\n"
      "./waf --run \"vanets_tcp"
      " --traceFile=src/mobility/examples/default.ns_movements"
      " --logFile=ns2-mob.log\" \n\n"
      "NOTE: ns2-traces-file could be an absolute or relative path. You could use the file default.ns_movements\n"
      "      included in the same directory of this example file.\n\n"
      "NOTE 2: Number of nodes present in the trace file must match with the command line argument and must\n"
      "        be a positive number. Note that you must know it before to be able to load it.\n\n"
      "NOTE 3: Duration must be a positive number. Note that you must know it before to be able to load it.\n\n";

      return 0;
    }

  MobilityHelper mobility;
  Ns2MobilityHelper ns2 = Ns2MobilityHelper (traceFile);

  std::ofstream os;
  os.open (logFile.c_str ());

  Time interPacketInterval = Seconds (interval);

  NodeContainer c;
  c.Create(23);

  ns2.Install ();
  Config::Connect ("/NodeList/*/$ns3::MobilityModel/CourseChange",
                   MakeBoundCallback (&CourseChange, &os));

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

  wifi80211p.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                      "DataMode",StringValue (phyMode),
                                      "ControlMode",StringValue (phyMode));
  NetDeviceContainer devices = wifi80211p.Install (wifiPhy, wifi80211pMac, c);

  // Tracing
  wifiPhy.EnablePcap ("wave-simple-80211p", devices);


  // Aqui voce pode mudar a velocidade e verificar o PDR
  // Testar com velocidades 10, 50 e 100km/h
  std::stringstream ssSpeed;
  ssSpeed << "ns3::UniformRandomVariable[Min=0.0|Max=" << 30 << "]";
  std::stringstream ssPause;
  ssPause << "ns3::ConstantRandomVariable[Constant=" << 5 << "]";

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
  Config::SetDefault ("ns3::OnOffApplication::PacketSize", UintegerValue (1800));

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

  AnimationInterface anim ("vanets-animation.xml");


  // Habilitando trace
  AsciiTraceHelper ascii;
  wifiPhy.EnableAsciiAll (ascii.CreateFileStream ("resultados.tr"));

  Simulator::Stop (Seconds (500.0));
  Simulator::Run ();
  Simulator::Destroy ();

  os.close (); // close log file
  return 0;
}

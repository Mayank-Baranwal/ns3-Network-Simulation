#include <string>
#include <fstream>
#include <cstdlib>
#include <map>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/tcp-westwood.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/packet-sink.h"
#include "ns3/tcp-hybla.h"
#include "ns3/tcp-congestion-ops.h"
#include "ns3/traced-value.h"
#include "ns3/tcp-yeah.h"
#include "ns3/log.h"
#include "ns3/tcp-scalable.h"
#include "ns3/sequence-number.h"
#include "ns3/traced-value.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/enum.h"

typedef uint32_t uint;

using namespace ns3;
using namespace std;

#define ERROR 0.0000001

NS_LOG_COMPONENT_DEFINE ("App_Part_A");

class App_Part_A: public Application
{
	private:
		virtual void StartApplication(void);
		virtual void StopApplication(void);
		void ScheduleTx(void);
		void SendPacket(void);
		Ptr<Socket> m_Socket;
		Address m_Peer;
		uint32_t m_PacketSize;
		uint32_t m_NPackets;
		DataRate m_DataRate;
		EventId m_SendEvent;
		bool m_Running;
		uint32_t m_PacketsSent;
	public:
		App_Part_A();
		virtual ~App_Part_A();
		void Setup(Ptr<Socket> socket, Address address, uint packetSize, uint nPackets, DataRate dataRate);
		void recv(int numBytesRcvd);
};

App_Part_A::App_Part_A(): m_Socket(0), m_Peer(), m_PacketSize(0), m_NPackets(0), m_DataRate(0), m_SendEvent(), m_Running(false), m_PacketsSent(0)
{}

App_Part_A::~App_Part_A()
{
	m_Socket = 0;
}

void App_Part_A::Setup(Ptr<Socket> socket, Address address, uint packetSize, uint nPackets, DataRate dataRate)
{
	m_Socket = socket;
	m_Peer = address;
	m_PacketSize = packetSize;
	m_NPackets = nPackets;
	m_DataRate = dataRate;
}

void App_Part_A::StartApplication()
{
	m_Running = true;
	m_PacketsSent = 0;
	m_Socket->Bind();
	m_Socket->Connect(m_Peer);
	SendPacket();
}

void App_Part_A::StopApplication()
{
	m_Running = false;
	if(m_SendEvent.IsRunning())	Simulator::Cancel(m_SendEvent);
	if(m_Socket)	m_Socket->Close();
}

void App_Part_A::SendPacket()
{
	Ptr<Packet> packet = Create<Packet>(m_PacketSize);
	m_Socket->Send(packet);
	if(++m_PacketsSent < m_NPackets)	ScheduleTx();
}

void App_Part_A::ScheduleTx()
{
	if (m_Running)
	{
		Time tNext(Seconds(m_PacketSize*8/static_cast<double>(m_DataRate.GetBitRate())));
		m_SendEvent = Simulator::Schedule(tNext, &App_Part_A::SendPacket, this);

	}
}

map<uint, uint> packetsDropped;
map<Address, double> TotalBytesAtSink;
map<string, double> mapBytesReceivedIPV4, Throughput;

static void CapturePacketDrop(FILE* stream, double startTime, uint myId)
{
	if(packetsDropped.find(myId) == packetsDropped.end())	packetsDropped[myId] = 0;
	packetsDropped[myId]++;
}

static void UpdateCwnd(FILE *stream, double startTime, uint oldCwnd, uint newCwnd)
{
	// Resizing cwnd (Congestion Window Size)
    fprintf(stream, "%s,%s\n",(to_string(Simulator::Now ().GetSeconds () - startTime)).c_str(), (to_string(newCwnd )).c_str());
}

void GetThroughputStats(FILE *stream, double startTime, string context, Ptr<const Packet> p, Ptr<Ipv4> ipv4, uint interface)
{
	// Calculating Throughput
	double timeNow = Simulator::Now().GetSeconds();
    if(Throughput.find(context) == Throughput.end())	Throughput[context] = 0;
	if(mapBytesReceivedIPV4.find(context) == mapBytesReceivedIPV4.end())	mapBytesReceivedIPV4[context] = 0;
	mapBytesReceivedIPV4[context] += p->GetSize();
	double curSpeed = (((mapBytesReceivedIPV4[context] * 8.0) / 1024)/(timeNow-startTime));
    fprintf(stream, "%s,%s\n",(to_string( timeNow-startTime).c_str()), (to_string(curSpeed )).c_str() );
    if(Throughput[context] < curSpeed)	Throughput[context] = curSpeed;
}

void GetGoodputStats(FILE *stream, double startTime, string context, Ptr<const Packet> p, const Address& addr)
{
	// Calculating Goodout
	double timeNow = Simulator::Now().GetSeconds();
	if(TotalBytesAtSink.find(addr) == TotalBytesAtSink.end())	TotalBytesAtSink[addr] = 0;
	TotalBytesAtSink[addr] += p->GetSize();
    double speed = (((TotalBytesAtSink[addr] * 8.0) / 1024)/(timeNow-startTime));
    fprintf(stream, "%s,%s\n",(to_string( timeNow-startTime).c_str() ) , (to_string(speed )).c_str() );
}


Ptr<Socket> WestwoodFlow(Address sinkAddress, uint sinkPort, Ptr<Node> hostNode, Ptr<Node> sinkNode, double startTime, double stopTime, uint packetSize, uint totalPackets, string dataRate, double appStartTime, double appStopTime)
{
    Config::SetDefault ("ns3::TcpWestwood::ProtocolType", EnumValue (TcpWestwood::WESTWOODPLUS));
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpWestwood::GetTypeId()));
	PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
	ApplicationContainer sinkApps = packetSinkHelper.Install(sinkNode);
	sinkApps.Start(Seconds(startTime));
	sinkApps.Stop(Seconds(stopTime));
	Ptr<Socket> TcpSocket = Socket::CreateSocket(hostNode, TcpSocketFactory::GetTypeId());
	// Running app for data collection
	Ptr<App_Part_A> app = CreateObject<App_Part_A>();
	app->Setup(TcpSocket, sinkAddress, packetSize, totalPackets, DataRate(dataRate));
	hostNode->AddApplication(app);
	app->SetStartTime(Seconds(appStartTime));
	app->SetStopTime(Seconds(appStopTime));
	return TcpSocket;
}

Ptr<Socket> YeahFlow(Address sinkAddress, uint sinkPort, Ptr<Node> hostNode, Ptr<Node> sinkNode, double startTime, double stopTime, uint packetSize, uint totalPackets, string dataRate, double appStartTime, double appStopTime)
{
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpYeah::GetTypeId()));
	PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
	ApplicationContainer sinkApps = packetSinkHelper.Install(sinkNode);
	sinkApps.Start(Seconds(startTime));
	sinkApps.Stop(Seconds(stopTime));
	Ptr<Socket> TcpSocket = Socket::CreateSocket(hostNode, TcpSocketFactory::GetTypeId());
	// Running app for data collection
	Ptr<App_Part_A> app = CreateObject<App_Part_A>();
	app->Setup(TcpSocket, sinkAddress, packetSize, totalPackets, DataRate(dataRate));
	hostNode->AddApplication(app);
	app->SetStartTime(Seconds(appStartTime));
	app->SetStopTime(Seconds(appStopTime));
	return TcpSocket;
}


Ptr<Socket> HyblaFlow(Address sinkAddress, uint sinkPort, Ptr<Node> hostNode, Ptr<Node> sinkNode, double startTime, double stopTime, uint packetSize, uint totalPackets, string dataRate, double appStartTime, double appStopTime)
{
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpYeah::GetTypeId()));
	PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
	ApplicationContainer sinkApps = packetSinkHelper.Install(sinkNode);
	sinkApps.Start(Seconds(startTime));
	sinkApps.Stop(Seconds(stopTime));
	Ptr<Socket> TcpSocket = Socket::CreateSocket(hostNode, TcpSocketFactory::GetTypeId());
    // Running app for data collection
	Ptr<App_Part_A> app = CreateObject<App_Part_A>();
	app->Setup(TcpSocket, sinkAddress, packetSize, totalPackets, DataRate(dataRate));
	hostNode->AddApplication(app);
	app->SetStartTime(Seconds(appStartTime));
	app->SetStopTime(Seconds(appStopTime));
	return TcpSocket;
}

int main()
{
	string rateHR = "100Mbps";
	string latencyHR = "20ms";
	string rateRR = "10Mbps";
	string latencyRR = "50ms";
    uint totalPackets = 1000000;
	uint packetSize = 1.5*1024;
	double runningTime = 100;
	double startTime = 0;
	uint port = 9000;
	string transferSpeed = "400Mbps";
	double errorP = ERROR;
    Ptr<RateErrorModel> em = CreateObjectWithAttributes<RateErrorModel> ("ErrorRate", DoubleValue (errorP));

	cout << "Stage 1: Defining Node Containers" << endl;
	PointToPointHelper p2pHTR, p2pRTR;
    p2pHTR.SetDeviceAttribute("DataRate", StringValue(rateHR));
	p2pHTR.SetChannelAttribute("Delay", StringValue(latencyHR));
	p2pHTR.SetQueue("ns3::DropTailQueue");
	p2pRTR.SetDeviceAttribute("DataRate", StringValue(rateRR));
	p2pRTR.SetChannelAttribute("Delay", StringValue(latencyRR));
	p2pRTR.SetQueue("ns3::DropTailQueue");
    NodeContainer nodes;
    nodes.Create(8);
    NodeContainer h1r1 = NodeContainer (nodes.Get (0), nodes.Get (3)); //h1r1
    NodeContainer h2r1 = NodeContainer (nodes.Get (1), nodes.Get (3));
    NodeContainer h3r1 = NodeContainer (nodes.Get (2), nodes.Get (3));
    NodeContainer h4r2 = NodeContainer (nodes.Get (5), nodes.Get (4));
    NodeContainer h5r2 = NodeContainer (nodes.Get (6), nodes.Get (4));
    NodeContainer h6r2 = NodeContainer (nodes.Get (7), nodes.Get (4));
    NodeContainer r1r2 = NodeContainer (nodes.Get (3), nodes.Get (4));
    InternetStackHelper internet;
    internet.Install (nodes);

    cout << "Stage 2: Installing p2p links in the dumbell" << endl;
    NetDeviceContainer n_h1r1 = p2pHTR.Install (h1r1);
    NetDeviceContainer n_h2r1 = p2pHTR.Install (h2r1);
    NetDeviceContainer n_h3r1 = p2pHTR.Install (h3r1);
    NetDeviceContainer n_h4r2 = p2pHTR.Install (h4r2);
    NetDeviceContainer n_h5r2 = p2pHTR.Install (h5r2);
    NetDeviceContainer n_h6r2 = p2pHTR.Install (h6r2);
    NetDeviceContainer n_r1r2 = p2pRTR.Install (r1r2);
    n_h1r1.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(em));
    n_h2r1.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(em));
    n_h3r1.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(em));
    n_h4r2.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(em));
    n_h5r2.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(em));
    n_h6r2.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(em));

    cout << "Stage 3: Adding IP addresses" << endl;
    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("100.101.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i_h1r1 = ipv4.Assign (n_h1r1);
    ipv4.SetBase ("100.101.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i_h2r1 = ipv4.Assign (n_h2r1);
    ipv4.SetBase ("100.101.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i_h3r1 = ipv4.Assign (n_h3r1);
    ipv4.SetBase ("100.101.5.0", "255.255.255.0");
    Ipv4InterfaceContainer i_h4r2 = ipv4.Assign (n_h4r2);
    ipv4.SetBase ("100.101.6.0", "255.255.255.0");
    Ipv4InterfaceContainer i_h5r2 = ipv4.Assign (n_h5r2);
    ipv4.SetBase ("100.101.7.0", "255.255.255.0");
    Ipv4InterfaceContainer i_h6r2 = ipv4.Assign (n_h6r2);
    ipv4.SetBase ("100.101.4.0", "255.255.255.0");
    Ipv4InterfaceContainer i_r1r2 = ipv4.Assign (n_r1r2);
    AsciiTraceHelper asciiTraceHelper;
    
	FILE * HyblaPacketLossStats,*HyblaCwndStats,*HyblaThroughputStats,*HyblaGoodputStats;
    HyblaPacketLossStats = fopen ("PartA_Hybla_congestion_loss.txt","w");
    HyblaCwndStats = fopen ("PartA_Hybla_cwnd.csv","w");
    HyblaThroughputStats = fopen ("PartA_Hybla_tp.csv","w");
    HyblaGoodputStats = fopen ("PartA_Hybla_gp.csv","w");

    // TCP Hybla Simulation

	cout << "Stage 4a: Initiating TCP Hybla Simulation" << endl;
	Ptr<Socket> HyblaSocket = HyblaFlow(InetSocketAddress(i_h5r2.GetAddress(0), port), port, nodes.Get(1), nodes.Get(6), startTime, startTime+runningTime, packetSize, totalPackets, transferSpeed, startTime, startTime+runningTime);
	// Measuring PacketSinks
	string sinkHybla1 = "/NodeList/6/ApplicationList/0/$ns3::PacketSink/Rx";
	Config::Connect(sinkHybla1, MakeBoundCallback(&GetGoodputStats, HyblaGoodputStats, startTime));
	string sinkHybla2 = "/NodeList/6/$ns3::Ipv4L3Protocol/Rx";
	Config::Connect(sinkHybla2, MakeBoundCallback(&GetThroughputStats, HyblaThroughputStats, startTime));
	HyblaSocket->TraceConnectWithoutContext("Drop", MakeBoundCallback (&CapturePacketDrop, HyblaPacketLossStats, startTime, 2));
    HyblaSocket->TraceConnectWithoutContext("CongestionWindow", MakeBoundCallback (&UpdateCwnd, HyblaCwndStats, startTime));
	startTime += runningTime;

    FILE * YeahPacketLossStats,*YeahCwndStats,*YeahThroughputStats,*YeahGoodPutStats;
    YeahPacketLossStats = fopen ("PartA_Yeah_congestion_loss.txt","w");
    YeahCwndStats = fopen ("PartA_Yeah_cwnd.csv","w");
    YeahThroughputStats = fopen ("PartA_Yeah_tp.csv","w");
    YeahGoodPutStats = fopen ("PartA_Yeah_gp.csv","w");
	
    // TCP Yeah Stimulation
	cout << "Stage 4b: Initiating TCP Yeah Simulation" << endl;

	Ptr<Socket> YeahSocket = YeahFlow(InetSocketAddress( i_h4r2.GetAddress(0), port), port, nodes.Get(0), nodes.Get(5), startTime, startTime+runningTime, packetSize, totalPackets, transferSpeed, startTime, startTime+runningTime);
	// Measuring PacketSinks
	string sinkYeah1 = "/NodeList/5/ApplicationList/0/$ns3::PacketSink/Rx";
	Config::Connect(sinkYeah1, MakeBoundCallback(&GetGoodputStats, YeahGoodPutStats, startTime));
	string sinkYeah2 = "/NodeList/5/$ns3::Ipv4L3Protocol/Rx";
	Config::Connect(sinkYeah2, MakeBoundCallback(&GetThroughputStats, YeahThroughputStats, startTime));
    YeahSocket->TraceConnectWithoutContext("Drop", MakeBoundCallback (&CapturePacketDrop, YeahPacketLossStats, startTime, 1));
    YeahSocket->TraceConnectWithoutContext("CongestionWindow", MakeBoundCallback (&UpdateCwnd, YeahCwndStats, startTime));
	startTime += runningTime;

    // Westwood+ Stimulation

	cout << "Stage 4c: Initialting TCP Westwood+ Simulation" << endl;
    FILE * WestwoodPacketLossStats,*WestwoodCwndStats,*WestwoodThroughputStats,*WestwoodGoodputStats;
    WestwoodPacketLossStats = fopen ("PartA_Westwood_congestion_loss.txt","w");
    WestwoodCwndStats = fopen ("PartA_Westwood_cwnd.csv","w");
    WestwoodThroughputStats = fopen ("PartA_Westwood_tp.csv","w");
    WestwoodGoodputStats = fopen ("PartA_Westwood_gp.csv","w");
	Ptr<Socket> westwoodPSocket = WestwoodFlow(InetSocketAddress(i_h6r2.GetAddress(0), port), port, nodes.Get(2), nodes.Get(7), startTime, startTime+runningTime, packetSize, totalPackets, transferSpeed, startTime, startTime+runningTime);
	// Measuring PacketSinks
	string sinkWestwood1 = "/NodeList/7/ApplicationList/0/$ns3::PacketSink/Rx";
	Config::Connect(sinkWestwood1, MakeBoundCallback(&GetGoodputStats, WestwoodGoodputStats, startTime));
	string sinkWestwood2 = "/NodeList/7/$ns3::Ipv4L3Protocol/Rx";
	Config::Connect(sinkWestwood2, MakeBoundCallback(&GetThroughputStats, WestwoodThroughputStats, startTime));
    westwoodPSocket->TraceConnectWithoutContext("Drop", MakeBoundCallback (&CapturePacketDrop, WestwoodPacketLossStats, startTime, 3));
    westwoodPSocket->TraceConnectWithoutContext("CongestionWindow", MakeBoundCallback (&UpdateCwnd, WestwoodCwndStats, startTime));
	startTime += runningTime;
	cout << "Stage 5: Running Flow Monitor. Please Wait" << endl;
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
	Ptr<FlowMonitor> flowmon;
	FlowMonitorHelper flowmonHelper;
	flowmon = flowmonHelper.InstallAll();
	Simulator::Stop(Seconds(startTime));
	Simulator::Run();
	flowmon->CheckForLostPackets();
	Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
	map<FlowId, FlowMonitor::FlowStats> stats = flowmon->GetFlowStats();
	for (map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
	{
		Ipv4FlowClassifier::FiveTuple tempClassifier = classifier->FindFlow (i->first);
        if (tempClassifier.sourceAddress=="100.101.1.1")
		{
		    fprintf(YeahPacketLossStats, "TCP Yeah flow %s (100.101.1.1 -> 100.101.5.1)\n",(to_string( i->first)).c_str());
            fprintf(YeahPacketLossStats, "Total Packet Lost: %s\n",(to_string( i->second.lostPackets)).c_str() );
            fprintf(YeahPacketLossStats, "Packet Lost due to buffer overflow: %s\n",(to_string( packetsDropped[1] )).c_str() );
            fprintf(YeahPacketLossStats, "Packet Lost due to Congestion: %s\n",(to_string( i->second.lostPackets - packetsDropped[1] )).c_str() );
            fprintf(YeahPacketLossStats, "Maximum throughput(in kbps): %s\n",(to_string( Throughput["/NodeList/5/$ns3::Ipv4L3Protocol/Rx"] )).c_str() );
            fprintf(YeahPacketLossStats, "Total Packets transmitted: %s\n",(to_string( totalPackets )).c_str() );
            fprintf(YeahPacketLossStats, "Packets Successfully Transferred: %s\n",(to_string(  totalPackets- i->second.lostPackets )).c_str() );
            fprintf(YeahPacketLossStats, "Percentage of packet loss (total): %s\n",(to_string( double(i->second.lostPackets*100)/double(totalPackets) )).c_str() );
		    fprintf(YeahPacketLossStats, "Percentage of packet loss (due to buffer overflow): %s\n",(to_string(  double(packetsDropped[1]*100)/double(totalPackets))).c_str() );
		    fprintf(YeahPacketLossStats, "Percentage of packet loss (due to congestion): %s\n",(to_string( double((i->second.lostPackets - packetsDropped[1])*100)/double(totalPackets))).c_str() );
		}
        else if(tempClassifier.sourceAddress=="100.101.2.1")
		{
            fprintf(HyblaPacketLossStats, "TCP Hybla flow %s (100.101.2.1 -> 100.101.6.1)\n",(to_string( i->first)).c_str());
            fprintf(HyblaPacketLossStats, "Total Packet Lost: %s\n",(to_string( i->second.lostPackets)).c_str() );
            fprintf(HyblaPacketLossStats, "Packet Lost due to buffer overflow: %s\n",(to_string( packetsDropped[2] )).c_str() );
            fprintf(HyblaPacketLossStats, "Packet Lost due to Congestion: %s\n",(to_string( i->second.lostPackets - packetsDropped[2] )).c_str() );
            fprintf(HyblaPacketLossStats, "Maximum throughput(in kbps): %s\n",(to_string( Throughput["/NodeList/6/$ns3::Ipv4L3Protocol/Rx"] )).c_str() );
            fprintf(HyblaPacketLossStats, "Total Packets transmitted: %s\n",(to_string( totalPackets )).c_str() );
            fprintf(HyblaPacketLossStats, "Packets Successfully Transferred: %s\n",(to_string(  totalPackets- i->second.lostPackets )).c_str() );
            fprintf(HyblaPacketLossStats, "Percentage of packet loss (total): %s\n",(to_string( double(i->second.lostPackets*100)/double(totalPackets) )).c_str() );
		    fprintf(HyblaPacketLossStats, "Percentage of packet loss (due to buffer overflow): %s\n",(to_string(  double(packetsDropped[2]*100)/double(totalPackets))).c_str() );
		    fprintf(HyblaPacketLossStats, "Percentage of packet loss (due to congestion): %s\n",(to_string( double((i->second.lostPackets - packetsDropped[2])*100)/double(totalPackets))).c_str() );
        }
        else if(tempClassifier.sourceAddress=="100.101.3.1")
		{
            fprintf(WestwoodPacketLossStats, "TCP Westwood+ flow %s (100.101.3.1 -> 100.101.7.1)\n",(to_string( i->first)).c_str());
            fprintf(WestwoodPacketLossStats, "Total Packet Lost: %s\n",(to_string( i->second.lostPackets)).c_str() );
            fprintf(WestwoodPacketLossStats, "Packet Lost due to buffer overflow: %s\n",(to_string( packetsDropped[3] )).c_str() );
            fprintf(WestwoodPacketLossStats, "Packet Lost due to Congestion: %s\n",(to_string( i->second.lostPackets - packetsDropped[3] )).c_str() );
            fprintf(WestwoodPacketLossStats, "Maximum throughput(in kbps): %s\n",(to_string( Throughput["/NodeList/7/$ns3::Ipv4L3Protocol/Rx"] )).c_str() );
            fprintf(WestwoodPacketLossStats, "Total Packets transmitted: %s\n",(to_string( totalPackets )).c_str() );
            fprintf(WestwoodPacketLossStats, "Packets Successfully Transferred: %s\n",(to_string(  totalPackets- i->second.lostPackets )).c_str() );
            fprintf(WestwoodPacketLossStats, "Percentage of packet loss (total): %s\n",(to_string( double(i->second.lostPackets*100)/double(totalPackets) )).c_str() );
		    fprintf(WestwoodPacketLossStats, "Percentage of packet loss (due to buffer overflow): %s\n",(to_string(  double(packetsDropped[3]*100)/double(totalPackets))).c_str() );
		    fprintf(WestwoodPacketLossStats, "Percentage of packet loss (due to congestion): %s\n",(to_string( double((i->second.lostPackets - packetsDropped[3])*100)/double(totalPackets))).c_str() );
        }
	}

 //    fclose(YeahPacketLossStats);
	// fclose(YeahCwndStats);
	// fclose(YeahThroughputStats);
	// fclose(YeahGoodputStats);

	// fclose(HyblaPacketLossStats);
	// fclose(HyblaCwndStats);
	// fclose(HyblaThroughputStats);
	// fclose(HyblaGoodputStats);

	// fclose(WestwoodPacketLossStats);
	// fclose(WestwoodCwndStats);
	// fclose(WestwoodThroughputStats);
	// fclose(WestwoodGoodputStats);

	cout << "Finished running app. Please check files created for output data" << endl;

	Simulator::Destroy();
    return 0;
}

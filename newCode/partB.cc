#include <string>
#include <fstream>
#include <cstdlib>
#include <map>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/gnuplot.h"
#include <sys/stat.h>
#include "ns3/point-to-point-dumbbell.h"
typedef uint32_t uint;

using namespace ns3;
using namespace std;


NS_LOG_COMPONENT_DEFINE ("App6");

//Structure to define application and device
class App_Part_B: public Application {
	private:
		virtual void StartApplication(void);
		virtual void StopApplication(void);

		void ScheduleTx(void);
		void SendPacket(void);

		Ptr<Socket>     mSocket;
		Address         mPeer;
		uint32_t        mPacketSize;
		uint32_t        mNPackets;
		DataRate        mDataRate;
		EventId         mSendEvent;
		bool            mRunning;
		uint32_t        mPacketsSent;

	public:
		App_Part_B();
		virtual ~App_Part_B();

		void Setup(Ptr<Socket> socket, Address address, uint packetSize, uint nPackets, DataRate dataRate);
		void ChangeRate(DataRate newRate);
		void recv(int numBytesRcvd);

};

//Application Constructor
App_Part_B::App_Part_B(): mSocket(0),
		    mPeer(),
		    mPacketSize(0),
		    mNPackets(0),
		    mDataRate(0),
		    mSendEvent(),
		    mRunning(false),
		    mPacketsSent(0) {
}

App_Part_B::~App_Part_B() {
	mSocket = 0;
}

//Application Specifications setup
void App_Part_B::Setup(Ptr<Socket> socket, Address address, uint packetSize, uint nPackets, DataRate dataRate) {
	mSocket = socket;
	mPeer = address;
	mPacketSize = packetSize;
	mNPackets = nPackets;
	mDataRate = dataRate;
}

//Function to start application and sockets
void App_Part_B::StartApplication() {
	mRunning = true;
	mPacketsSent = 0;
	mSocket->Bind();
	mSocket->Connect(mPeer);
	SendPacket();
}

//Function to stop application and sockets
void App_Part_B::StopApplication() {
	mRunning = false;
	if(mSendEvent.IsRunning()) {
		Simulator::Cancel(mSendEvent);
	}
	if(mSocket) {
		mSocket->Close();
	}
}

//Function to send packets
void App_Part_B::SendPacket() {
	Ptr<Packet> packet = Create<Packet>(mPacketSize);
	mSocket->Send(packet);

	if(++mPacketsSent < mNPackets) {
		ScheduleTx();
	}
}

void App_Part_B::ScheduleTx() {
	if (mRunning) {
		Time tNext(Seconds(mPacketSize*8/static_cast<double>(mDataRate.GetBitRate())));
		mSendEvent = Simulator::Schedule(tNext, &App_Part_B::SendPacket, this);
		//double tVal = Simulator::Now().GetSeconds();
		//if(tVal-int(tVal) >= 0.99)
		//	cout << Simulator::Now ().GetSeconds () << "\t" << mPacketsSent << endl;
	}
}

void App_Part_B::ChangeRate(DataRate newrate) {
	mDataRate = newrate;
	return;
}

map<uint, uint> dropPacketBufferFull;
map<Address, double> mapBytesReceived;
map<string, double> mapBytesReceivedIPV4, MaxFlowThroughput;
static double lastTimePrint = 0, lastTimePrintIPV4 = 0;
double printGap = 0;
PointToPointHelper configureP2PHelper(string rate, string latency, string s);
static void CaptureCwndChange(Ptr<OutputStreamWrapper> stream, double startTime, uint oldCwnd, uint newCwnd);
static void CapturePacketDrop(double startTime, uint myId);

void ReceivedPacket(Ptr<OutputStreamWrapper> stream, double startTime, string context, 
					Ptr<const Packet> p, const Address& addr);

void ReceivedPacketIPV4(Ptr<OutputStreamWrapper> stream, double startTime, string context, 
						Ptr<const Packet> p, Ptr<Ipv4> ipv4, uint interface);

Ptr<Socket> uniFlow(Address sinkAddress, uint sinkPort, string tcpVariant, 
					Ptr<Node> hostNode, Ptr<Node> sinkNode, double startTime, 
					double stopTime, uint packetSize, uint totalPackets,
					string dataRate, double appStartTime, double appStopTime); 

void InitiateSimulation(Ptr<Socket> TcpSocket, string tcpVariant, int nodenum, int index, double startTime);
void printIteration(string tcpVariant, int nodenum, int index, Ptr<OutputStreamWrapper> summary, 
					Ipv4FlowClassifier::FiveTuple flow_elem, map<FlowId, FlowMonitor::FlowStats>::const_iterator flow_stat);
void createFlowSummary(Ptr<Ipv4FlowClassifier> flowClassifier, map<FlowId, FlowMonitor::FlowStats> flowStats);

int main(){
	//Dumbell topolgy specifications
	string rateHR = "100Mbps";
	string latencyHR = "20ms";
	string rateRR = "10Mbps";
	string latencyRR = "50ms";
	uint packetSize = 1.5*1024;		//1.5KB

	string queueSizeHR = to_string((100000*20)/packetSize)+"p";
	string queueSizeRR = to_string((10000*50)/packetSize)+"p";

	uint totalSenders = 3;

	double netRunningTime = 100;
	double FirstFlowStartTime = 0;
	double MultiFlowStartTime = 20;
	uint port = 9000;
	uint totalPackets = 10000000;
	string transferSpeed = "400Mbps";

	mkdir("PartB", S_IRWXU);
	//Defining dumbell topology
	cout << "Stage 1: Defining dumbell topology" << endl;
	PointToPointHelper HostRouterLink = configureP2PHelper(rateHR, latencyHR, queueSizeHR);
	PointToPointHelper RouterRouterLink = configureP2PHelper(rateRR, latencyRR, queueSizeRR);
	PointToPointDumbbellHelper dumbell(totalSenders, HostRouterLink, totalSenders, RouterRouterLink, RouterRouterLink);
	cout << "Stage 2: Installing internet stack" << endl;
	InternetStackHelper stack;
	dumbell.InstallStack(stack);
	Ipv4AddressHelper senderIP = Ipv4AddressHelper("10.1.0.0", "255.255.255.0");
	Ipv4AddressHelper receiverIP = Ipv4AddressHelper("10.2.0.0", "255.255.255.0");
	Ipv4AddressHelper routerIP = Ipv4AddressHelper("10.3.0.0", "255.255.255.0");
	//Assigning IP address to devices
	cout << "Stage 3: Assigning IP Addresses" << endl;
	dumbell.AssignIpv4Addresses (senderIP, receiverIP, routerIP);
	
	Ptr<Socket> TcpHyblaSocket = uniFlow(InetSocketAddress(dumbell.GetRightIpv4Address(0), port), port,
										 "hybla", dumbell.GetLeft(0), dumbell.GetRight(0), FirstFlowStartTime,
										  FirstFlowStartTime+netRunningTime, packetSize, totalPackets, 
										  transferSpeed, FirstFlowStartTime, FirstFlowStartTime+netRunningTime);
	Ptr<Socket> TcpWestwoodSocket = uniFlow(InetSocketAddress(dumbell.GetRightIpv4Address(1), port), port, 
											"westwood", dumbell.GetLeft(1), dumbell.GetRight(1), MultiFlowStartTime, 
											MultiFlowStartTime+netRunningTime, packetSize, totalPackets, 
											transferSpeed, MultiFlowStartTime, MultiFlowStartTime+netRunningTime);
	Ptr<Socket> TcpYeahSocket = uniFlow(InetSocketAddress(dumbell.GetRightIpv4Address(2), port), port, 
										"yeah", dumbell.GetLeft(2), dumbell.GetRight(2), MultiFlowStartTime, 
										MultiFlowStartTime+netRunningTime, packetSize, totalPackets, 
										transferSpeed, MultiFlowStartTime, MultiFlowStartTime+netRunningTime);

	//Running all simulations
	cout << "Stage 4a: Initiating TCP Hybla Simulation" << endl;
	InitiateSimulation(TcpHyblaSocket, "hybla", 5, 1, FirstFlowStartTime);
	cout << "Stage 4b: Initiating TCP Westwood+ Simulation" << endl;
	InitiateSimulation(TcpWestwoodSocket, "westwood", 6, 2, FirstFlowStartTime);
	cout << "Stage 4c: Initiating TCP Yeah Simulation" << endl;
	InitiateSimulation(TcpYeahSocket, "yeah", 7, 3, FirstFlowStartTime);

	Ipv4GlobalRoutingHelper::PopulateRoutingTables();
	//Initialize flow monitor
	cout << "Stage 5: Running Flow Monitor. Please Wait" << endl;
	Ptr<FlowMonitor> flowmonitor;
	FlowMonitorHelper flowmonitorHelper;
	flowmonitor = flowmonitorHelper.InstallAll();
	Simulator::Stop(Seconds(netRunningTime+MultiFlowStartTime));
	Simulator::Run();
	flowmonitor->CheckForLostPackets();

	Ptr<Ipv4FlowClassifier> flowClassifier = DynamicCast<Ipv4FlowClassifier>(flowmonitorHelper.GetClassifier());
	map<FlowId, FlowMonitor::FlowStats> flowStats = flowmonitor->GetFlowStats();
	createFlowSummary(flowClassifier, flowStats);

	cout << "Finished running app. Please check files created for output data" << endl;
	Simulator::Destroy();
	return 0;

}

//Configuring P2P links 
PointToPointHelper configureP2PHelper(string rate, string latency, string queueSize)
{
	PointToPointHelper p2p;
	p2p.SetDeviceAttribute("DataRate", StringValue(rate));
	p2p.SetChannelAttribute("Delay", StringValue(latency));
	p2p.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue(queueSize));
	return p2p;
}
static void CaptureCwndChange(Ptr<OutputStreamWrapper> stream, double startTime, uint oldCwnd, uint newCwnd) {
	*stream->GetStream() << Simulator::Now ().GetSeconds () - startTime << "," << newCwnd << endl;
}
static void CapturePacketDrop(double startTime, uint myId) {
	if(dropPacketBufferFull.find(myId) == dropPacketBufferFull.end()) {
		dropPacketBufferFull[myId] = 0;
	}
	dropPacketBufferFull[myId]++;
}

//Checking performance of packets
void ReceivedPacket(Ptr<OutputStreamWrapper> stream, double startTime, string context, Ptr<const Packet> p, const Address& addr){
	double timeNow = Simulator::Now().GetSeconds();

	if(mapBytesReceived.find(addr) == mapBytesReceived.end())
		mapBytesReceived[addr] = 0;
	mapBytesReceived[addr] += p->GetSize();
	double tp = (((mapBytesReceived[addr] * 8.0) / 1024)/(timeNow-startTime));
	if(timeNow - lastTimePrint >= printGap) {
		lastTimePrint = timeNow;
		*stream->GetStream() << timeNow-startTime << "," <<  tp << endl;
	}
}
void ReceivedPacketIPV4(Ptr<OutputStreamWrapper> stream, double startTime, string context, Ptr<const Packet> p, Ptr<Ipv4> ipv4, uint interface) {
	double timeNow = Simulator::Now().GetSeconds();

	if(mapBytesReceivedIPV4.find(context) == mapBytesReceivedIPV4.end())
		mapBytesReceivedIPV4[context] = 0;
	if(MaxFlowThroughput.find(context) == MaxFlowThroughput.end())
		MaxFlowThroughput[context] = 0;
	mapBytesReceivedIPV4[context] += p->GetSize();
	double tp = (((mapBytesReceivedIPV4[context] * 8.0) / 1024)/(timeNow-startTime));
	if(timeNow - lastTimePrintIPV4 >= printGap) {
		lastTimePrintIPV4 = timeNow;
		*stream->GetStream() << timeNow-startTime << "," <<  tp << endl;
		if(MaxFlowThroughput[context] < tp)
			MaxFlowThroughput[context] = tp;
	}
}

//Set up flow between sender and receiver
Ptr<Socket> uniFlow(Address sinkAddress, uint sinkPort, string tcpVariant, 
					Ptr<Node> hostNode, Ptr<Node> sinkNode, double startTime, 
					double stopTime, uint packetSize, uint totalPackets,
					string dataRate, double appStartTime, double appStopTime) {

	if(tcpVariant.compare("hybla") == 0) {
		Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpHybla::GetTypeId()));
	} else if(tcpVariant.compare("westwood") == 0) {
		Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpWestwood::GetTypeId()));
	} else if(tcpVariant.compare("yeah") == 0) {
		Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpYeah::GetTypeId()));
	} else {
		fprintf(stderr, "Invalid TCP version\n");
		exit(EXIT_FAILURE);
	}
	PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
	ApplicationContainer sinkApps = packetSinkHelper.Install(sinkNode);
	sinkApps.Start(Seconds(startTime));
	sinkApps.Stop(Seconds(stopTime));

	Ptr<Socket> ns3TcpSocket = Socket::CreateSocket(hostNode, TcpSocketFactory::GetTypeId());	
	Ptr<App_Part_B> app = CreateObject<App_Part_B>();
	app->Setup(ns3TcpSocket, sinkAddress, packetSize, totalPackets, DataRate(dataRate));
	hostNode->AddApplication(app);
	app->SetStartTime(Seconds(appStartTime));
	app->SetStopTime(Seconds(appStopTime));

	return ns3TcpSocket;
}
//Fucntion to start simulation 
void InitiateSimulation(Ptr<Socket> TcpSocket, string tcpVariant, int nodenum, int index, double startTime){
	AsciiTraceHelper asciiTraceHelper;
	Ptr<OutputStreamWrapper> CwndStats = asciiTraceHelper.CreateFileStream("PartB/data_"+tcpVariant+"_cwnd.csv");
	Ptr<OutputStreamWrapper> ThroughputStats = asciiTraceHelper.CreateFileStream("PartB/data_"+tcpVariant+"_tp.csv");
	Ptr<OutputStreamWrapper> GoodputStats = asciiTraceHelper.CreateFileStream("PartB/data_"+tcpVariant+"_gp.csv");
	TcpSocket->TraceConnectWithoutContext("CongestionWindow", MakeBoundCallback (&CaptureCwndChange, CwndStats, startTime));
	TcpSocket->TraceConnectWithoutContext("Drop", MakeBoundCallback (&CapturePacketDrop, startTime, index));

	string sink = "/NodeList/" + to_string(nodenum) + "/ApplicationList/0/$ns3::PacketSink/Rx";
	Config::Connect(sink, MakeBoundCallback(&ReceivedPacket, GoodputStats, startTime));
	string sink_ = "/NodeList/" + to_string(nodenum) + "/$ns3::Ipv4L3Protocol/Rx";
	Config::Connect(sink_, MakeBoundCallback(&ReceivedPacketIPV4, ThroughputStats, startTime));
}

//Print details of each iteration
void printIteration(string tcpVariant, int nodenum, int index, Ptr<OutputStreamWrapper> summary, 
					Ipv4FlowClassifier::FiveTuple flow_elem, map<FlowId, FlowMonitor::FlowStats>::const_iterator flow_stat){
	if(dropPacketBufferFull.find(index)==dropPacketBufferFull.end())
		dropPacketBufferFull[index] = 0;
	*summary->GetStream() << "TCP " << tcpVariant << " flow " << flow_stat->first  << " (" << flow_elem.sourceAddress << " ---> " << flow_elem.destinationAddress << ")\n";
	*summary->GetStream() << "Total number of Packets lost: " << flow_stat->second.lostPackets << "\n";
	*summary->GetStream() << "Number of Packets lost due to buffer overflow: " << dropPacketBufferFull[index] << "\n";
	*summary->GetStream() << "Number of Packets lost due to Congestion: " << flow_stat->second.lostPackets - dropPacketBufferFull[index] << "\n";
	*summary->GetStream() << "Maximum throughput(in kbps): " << MaxFlowThroughput["/NodeList/" + to_string(nodenum) + "/$ns3::Ipv4L3Protocol/Rx"] << endl;
}

//Summarize runs of all flows
void createFlowSummary(Ptr<Ipv4FlowClassifier> flowClassifier, map<FlowId, FlowMonitor::FlowStats> flowStats){

	AsciiTraceHelper asciiTraceHelper;
	Ptr<OutputStreamWrapper> HyblaSummary = asciiTraceHelper.CreateFileStream("PartB/hybla_summary.txt");
	Ptr<OutputStreamWrapper> WestwoodSummary = asciiTraceHelper.CreateFileStream("PartB/westwood_summary.txt");
	Ptr<OutputStreamWrapper> YeahSummary = asciiTraceHelper.CreateFileStream("PartB/yeah_summary.txt");

	for (map<FlowId, FlowMonitor::FlowStats>::const_iterator i = flowStats.begin(); i != flowStats.end(); ++i) {
		Ipv4FlowClassifier::FiveTuple flow_elem = flowClassifier->FindFlow (i->first);
		if(flow_elem.sourceAddress == "10.1.0.1") {
			printIteration("Hybla", 5, 1, HyblaSummary, flow_elem, i);
		} else if(flow_elem.sourceAddress == "10.1.1.1") {
			printIteration("Westwood+", 6, 2, WestwoodSummary, flow_elem, i);	
		} else if(flow_elem.sourceAddress == "10.1.2.1") {
			printIteration("Yeah", 7, 3, YeahSummary, flow_elem, i);
		}
	}
}
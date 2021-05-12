#include <string>
#include <unistd.h>
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/traffic-control-module.h"
#include "ns3/cardiac-stem-cell-module.h"
#include "ns3/log.h"

using namespace ns3;
NS_LOG_COMPONENT_DEFINE("piero-udp-test");
const uint32_t DEFAULT_PACKET_SIZE = 1500;
static int ip_base=1;
static NodeContainer BuildP2PTopo(uint32_t msDelay,uint32_t msQdelay){
    NodeContainer nodes;
    nodes.Create (2);
    uint32_t bps=1000000000;//1Gbps
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute ("DataRate", DataRateValue  (DataRate (bps)));
    pointToPoint.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (msDelay)));
    auto bufSize = std::max<uint32_t> (DEFAULT_PACKET_SIZE, bps * msQdelay / 8000);
    int packets=bufSize/DEFAULT_PACKET_SIZE;
    pointToPoint.SetQueue ("ns3::DropTailQueue",
                           "MaxSize", StringValue (std::to_string(10)+"p"));
    NetDeviceContainer devices = pointToPoint.Install (nodes);

    InternetStackHelper stack;
    stack.Install (nodes);

    TrafficControlHelper pfifoHelper;
    uint16_t handle = pfifoHelper.SetRootQueueDisc ("ns3::FifoQueueDisc", "MaxSize", StringValue (std::to_string(packets)+"p"));
    pfifoHelper.AddInternalQueues (handle, 1, "ns3::DropTailQueue", "MaxSize",StringValue (std::to_string(packets)+"p"));
    pfifoHelper.Install(devices);


    Ipv4AddressHelper address;
    std::string nodeip="10.1."+std::to_string(ip_base)+".0";
    ip_base++;
    address.SetBase (nodeip.c_str(), "255.255.255.0");
    address.Assign (devices);
    return nodes;
}
static float VIDEO_BIT_RATE[]={300,750,1200,1850,2850,4300}; //kbps
static    float appStart=0.001;
void InstallUdpApp(Ptr<Node> h1,Ptr<Node> h2,
                   Ptr<DashUdpSource> dash_source,Ptr<DashUdpSink> dash_sink,
                   uint16_t client_port,uint16_t serv_port,
                   Time  max_processing_delay){
    Ptr<UdpClientChannel> clientChan=CreateObject<UdpClientChannel>(max_processing_delay);
    Ptr<UdpServerChannel> servChan=CreateObject<UdpServerChannel>();
    h1->AddApplication(clientChan);
    h2->AddApplication(servChan);
    clientChan->Bind(client_port);
    servChan->Bind(serv_port);
    auto serv_addr=servChan->GetLocalAddress();
    clientChan->ConfigurePeer(serv_addr);
    dash_source->RegisterChannel(clientChan);
    dash_sink->RegisterChannel(servChan);
}
void dash_app_test(std::vector<std::string> &video_log,std::vector<double> &average_rate,std::string &algo,DatasetDescriptation *dataset,std::string &bid){
    uint32_t one_trans_delay=50;
    uint32_t link_queue_delay=100;
    int segment_ms=4000;
    int init_segment=2;
    uint16_t sendPort=3000;
    uint16_t recvPort=5000;
    DataRate rate(2000000);

    std::string trace;
    std::string group_id("0");
    std::string agent_id("0");
    {
        std::string delimit("_");
        char buf[FILENAME_MAX];
        memset(buf,0,FILENAME_MAX);        
        std::string parent=std::string (getcwd(buf, FILENAME_MAX));
        std::string result_folder("app");
        std::string trace_folder=parent+ "/traces/"+result_folder;
        MakePath(trace_folder);
        piero_set_trace_root_folder(trace_folder.c_str());
        trace=group_id+delimit+agent_id+delimit+bid+delimit+algo;
    }
    auto topo=BuildP2PTopo(one_trans_delay,link_queue_delay);
    
    Ptr<DashUdpSource> dash_source=CreateObject<DashUdpSource>(video_log,average_rate,trace
    ,segment_ms,init_segment,Seconds(appStart));
    Ptr<DashUdpSink> dash_sink=CreateObject<DashUdpSink>(Seconds(appStart),Time(0),dataset,rate);
    InstallUdpApp(topo.Get(0),topo.Get(1),dash_source,dash_sink,
                 sendPort,recvPort,Seconds(1));
    sendPort++;
    recvPort++;
    dash_source->SetAdaptationAlgorithm(algo,group_id,agent_id);
    dash_source->Initialize();
    dash_sink->Initialize();
    Simulator::Run ();
    Simulator::Destroy();
}
int main(int argc, char *argv[]){
    LogComponentEnable("piero-udp-test",LOG_LEVEL_ALL);
    LogComponentEnable("piero-udp-chan",LOG_LEVEL_ALL);
    LogComponentEnable("piero-dash-app",LOG_LEVEL_ALL);
    LogComponentEnable("piero-rate-limit",LOG_LEVEL_ALL);
    std::string ns3_path="/home/ipcom/zsy/ns-allinone-3.31/ns-3.31/";
    std::string video_path=ns3_path+std::string("video_data/");
    std::string video_name("video_size_");
    std::vector<std::string> video_log;
    std::vector<double> average_rate;
    {
        int n=6;
        for(int i=0;i<n;i++){
            std::string name=video_path+video_name+std::to_string(i);
            video_log.push_back(name);
        }
        for (int i=0;i<n;i++){
            double bps=VIDEO_BIT_RATE[i]*1000;
            average_rate.push_back(bps);
        }
    }
    std::string name=ns3_path+std::string("bw_data/Oboe-master/traces/trace_0.txt");
    DatasetDescriptation another_dataset(name,RateTraceType::TIME_BW,TimeUnit::TIME_MS,RateUnit::BW_Kbps);
    std::string bid("0");
    const char *algorithms[]={"festive","panda","tobasco","osmp","raahs","fdash","sftm","svaa"};
    int sz=sizeof(algorithms)/sizeof(algorithms[0]);
    uint64_t last_time=PieroTimeMillis();
    for (int i=0;i<sz;i++){
        std::string algo(algorithms[i]);
        dash_app_test(video_log,average_rate,algo,&another_dataset,bid);
    }
    uint64_t delta=PieroTimeMillis()-last_time;
    double seconds=1.0*delta/1000;
    NS_LOG_INFO("run time: "<<seconds);
}





#include <string>
#include <unistd.h>
#include "ns3/core-module.h"
#include "ns3/cardiac-stem-cell-module.h"
#include "ns3/hunnan-module.h"
#include "ns3/log.h"
using namespace ns3;
NS_LOG_COMPONENT_DEFINE("piero-hunnan-model");
bool compare_by_string(const DatasetDescriptation& t1, const DatasetDescriptation & t2) {
    return t1.name<t2.name;
}
const uint32_t DEFAULT_PACKET_SIZE = 1500;
static float VIDEO_BIT_RATE[]={300,750,1200,1850,2850,4300}; //kbps
static    float appStart=0.001;
static HunnanNodeContainer BuildP2PTopo(uint32_t msDelay,uint32_t msQdelay){
    HunnanNodeContainer nodes;
    nodes.Create (2);
    uint32_t bps=1000000000;//1Gbps
    auto bufSize = std::max<uint32_t> (DEFAULT_PACKET_SIZE, bps * msQdelay / 8000);
    HunnanPointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate",DataRateValue(DataRate(bps)));
    p2p.SetDeviceAttribute("MaxSize",UintegerValue(bufSize));
    p2p.SetChannelAttribute("Delay",TimeValue(MilliSeconds(msDelay)));
    p2p.Install(nodes.Get(0),nodes.Get(1));
    return nodes;
}
void InstallHunnanApp(Ptr<HunnanNode> h1,Ptr<HunnanNode> h2,
                   Ptr<DashHunnanSource> dash_source,Ptr<DashHunnanSink> dash_sink,
                   Time  max_processing_delay=Seconds(200)){
    Ptr<HunnanClientChannel> clientChan=CreateObject<HunnanClientChannel>(max_processing_delay);
    Ptr<HunnanServerChannel> servChan=CreateObject<HunnanServerChannel>();
    h1->AddApplication(clientChan);
    h2->AddApplication(servChan);
    dash_source->RegisterChannel(clientChan);
    dash_sink->RegisterChannel(servChan);
}
void dash_app_run(std::vector<std::string> &video_log,std::vector<double> &average_rate,std::string &algo,
                 DatasetDescriptation *dataset,std::string &result_folder,
        std::string &bid, std::string &group_id,std::string &agent_id){
    uint32_t one_trans_delay=50;
    uint32_t link_queue_delay=100;
    int segment_ms=4000;
    int init_segment=2;
    DataRate default_rate(2000000);
    std::string delimit("_");
    std::string trace;
    if(result_folder.size()>0){
        MakePath(result_folder);
        piero_set_trace_root_folder(result_folder.c_str());
        trace=group_id+delimit+agent_id+delimit+bid+delimit+algo;
    }
    auto topo=BuildP2PTopo(one_trans_delay,link_queue_delay);
    Ptr<DashHunnanSource> dash_source=CreateObject<DashHunnanSource>(video_log,average_rate,trace
    ,segment_ms,init_segment,Seconds(appStart));
    Ptr<DashHunnanSink> dash_sink=CreateObject<DashHunnanSink>(Seconds(appStart),Time(0),dataset,default_rate);
    dash_sink->SetSeed(12321);
    InstallHunnanApp(topo.Get(0),topo.Get(1),dash_source,dash_sink);
    dash_source->SetAdaptationAlgorithm(algo,group_id,agent_id);
    dash_source->Initialize();
    dash_sink->Initialize();
    Simulator::Run ();
    Simulator::Destroy();
}
int main(int argc,char *argv[]){
    LogComponentEnable("piero-hunnan-model",LOG_LEVEL_ALL);
    LogComponentEnable("piero_hunnan_chan",LOG_LEVEL_ALL);
    LogComponentEnable("piero_dash_app",LOG_LEVEL_ALL);
    LogComponentEnable("piero_rate_limit",LOG_LEVEL_ALL);
    LogComponentEnable("piero_dash_base",LOG_LEVEL_INFO);
    std::string instance("0");
    std::string bid("0");
    std::string group_id("0");
    std::string agent_id("0");
    CommandLine cmd;
    cmd.AddValue ("it", "instance",instance);
    cmd.Parse (argc, argv);
    std::string prefix("hunnan_");
    std::string ns3_path="/home/ipcom/zsy/ns-allinone-3.31/ns-3.31/";
    std::string trace_folder=ns3_path+"traces/";
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
    const char *algorithms[]={"festive","panda","tobasco","osmp","raahs","fdash","sftm","svaa"};
    int algorithms_sz=sizeof(algorithms)/sizeof(algorithms[0]);
    DatasetDescriptation cooked_dataset[]={
        {ns3_path+std::string("bw_data/cooked_traces/"),
        RateTraceType::TIME_BW,TimeUnit::TIME_S,RateUnit::BW_Mbps},
    };
    DatasetDescriptation cooked_test_dataset[]={
        {ns3_path+std::string("bw_data/cooked_test_traces/"),
        RateTraceType::TIME_BW,TimeUnit::TIME_S,RateUnit::BW_Mbps},
    };
    DatasetDescriptation oboe_dataset[]={
        {ns3_path+std::string("bw_data/Oboe_traces/"),
        RateTraceType::TIME_BW,TimeUnit::TIME_S,RateUnit::BW_Mbps},
    };
    int dataset_n=0;
    DatasetDescriptation *dataset_ptr=nullptr;
    std::vector<DatasetDescriptation> bw_traces;
    if(0==instance.compare("1")){
        dataset_n=sizeof(cooked_dataset)/sizeof(cooked_dataset[0]);
        dataset_ptr=cooked_dataset;
    }else if(0==instance.compare("2")){
        dataset_n=sizeof(cooked_test_dataset)/sizeof(cooked_test_dataset[0]);
        dataset_ptr=cooked_test_dataset;
    }else if(0==instance.compare("3")){
        dataset_n=sizeof(oboe_dataset)/sizeof(oboe_dataset[0]);
        dataset_ptr=oboe_dataset;
    }
    if(dataset_n>0&&dataset_ptr){
        for(int i=0;i<dataset_n;i++){
            std::vector<std::string> files;
            std::string folder=dataset_ptr[i].name;
            GetFiles(folder,files);
            for (size_t j=0;j<files.size();j++){
                std::string path_name=folder+files[j];
                DatasetDescriptation des(path_name,dataset_ptr[i].type,
                dataset_ptr[i].time_unit,dataset_ptr[i].rate_unit);
                bw_traces.push_back(des);
            }
        }
        std::sort(bw_traces.begin(), bw_traces.end(),compare_by_string);
    }
    int dataset_sz=bw_traces.size();
    uint64_t last_time=PieroTimeMillis();
    if(0==instance.compare("0")){
        std::string name=ns3_path+std::string("bw_data/Oboe_traces/trace_0.txt");
        std::string result_folder=trace_folder+prefix+"sample";
        DatasetDescriptation another_dataset(name,RateTraceType::TIME_BW,TimeUnit::TIME_S,RateUnit::BW_Mbps);
        for (int i=0;i<1/*algorithms_sz*/;i++){
            std::string algo(algorithms[i]);
            dash_app_run(video_log,average_rate,algo,nullptr/*&another_dataset*/,result_folder,bid,group_id,agent_id);
        }
    }else if(0==instance.compare("1")){
        std::string result_folder=trace_folder+prefix+"cooked";
        for(int i=0;i<algorithms_sz;i++){
            std::string algo(algorithms[i]);
            for(int j=0;j<dataset_sz;j++){
                DatasetDescriptation bandwidth_des=bw_traces.at(j);
                bid=std::to_string(j);
                dash_app_run(video_log,average_rate,algo,&bandwidth_des,result_folder,bid,group_id,agent_id);
            }
        }
    }else if(0==instance.compare("2")){
        std::string result_folder=trace_folder+prefix+"cooked_test";
        for(int i=0;i<algorithms_sz;i++){
            std::string algo(algorithms[i]);
            for(int j=0;j<dataset_sz;j++){
                DatasetDescriptation bandwidth_des=bw_traces.at(j);
                bid=std::to_string(j);
                dash_app_run(video_log,average_rate,algo,&bandwidth_des,result_folder,bid,group_id,agent_id);
            }
        }
    }else if(0==instance.compare("3")){
        std::string result_folder=trace_folder+prefix+"oboe";
        for(int i=0;i<algorithms_sz;i++){
            std::string algo(algorithms[i]);
            for(int j=0;j<dataset_sz;j++){
                DatasetDescriptation bandwidth_des=bw_traces.at(j);
                bid=std::to_string(j);
                dash_app_run(video_log,average_rate,algo,&bandwidth_des,result_folder,bid,group_id,agent_id);
            }
        }
    }
    uint64_t delta=PieroTimeMillis()-last_time;
    double seconds=1.0*delta/1000;
    NS_LOG_INFO("run time: "<<seconds);
    return 0;
}

#include <string>
#include <vector>
#include <iostream>
#include "ns3/core-module.h"
#include "ns3/cardiac-stem-cell-module.h"
using namespace ns3;
//https://github.com/USC-NSL/Oboe/blob/master/traces/trace_0.txt
void test_algorithm(std::string &algo){
    std::string video_path("/home/zsy/ns-allinone-3.31/ns-3.31/video_data/");
    std::string bw_path("/home/zsy/ns-allinone-3.31/ns-3.31/bw_data/");
    std::string bw=bw_path+"trace_0.txt";
    std::string video_name("video_size_");
    int n=6;
    std::vector<std::string> video_log;
    for(int i=0;i<n;i++){
        std::string name=video_path+video_name+std::to_string(i);
        video_log.push_back(name);
    }
    Ptr<PieroBiChannel> channel=CreateObject<PieroBiChannel>();
    Time start=MicroSeconds(10);
    std::string trace=std::string("client_")+algo;
    Ptr<PieroDashClient> client=CreateObject<PieroDashClient>(video_log,trace,4000,3,channel->GetSocketA(),start);
    client->SetAdaptationAlgorithm(algo);
    StopBroadcast *broadcast=client->GetBroadcast();
    Ptr<PieroDashServer> server=CreateObject<PieroDashServer>(channel->GetSocketB(),broadcast);
    server->SetBandwidthTrace(bw,Time(0),RateTraceType::TIME_BW,TimeUnit::TIME_MS,RateUnit::BW_Kbps);
    Simulator::Run ();
    Simulator::Destroy();
}
int main(){
    const char *algo[]={"festive","panda","tobasco","osmp","raahs","fdash","sftm","svaa"};
    int n=sizeof(algo)/sizeof(algo[0]);
    for(int i=0;i<n;i++){
        std::string algorithm(algo[i]);
        test_algorithm(algorithm);
        std::cout<<i<<std::endl;
    }
}

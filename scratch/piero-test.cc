#include <string>
#include <vector>
#include <iostream>
#include "ns3/core-module.h"
#include "ns3/cardiac-stem-cell-module.h"
namespace ns3{
const uint32_t kContextAny=0xffffffff;
class MockApplication{
public:
    MockApplication(Ptr<PieroSocket> socket){
        channel_=CreateObject<PieroTraceChannel>(socket,MilliSeconds(4000));
        channel_->SetRecvCallback(MakeCallback(&MockApplication::RecvPacket,this));
    }
    void Start(int64_t delay_ms){
        Time next=ns3::MilliSeconds(delay_ms);
        Simulator::ScheduleWithContext(kContextAny,next,
                    MakeEvent(&MockApplication::GenerateFakePacket, this));        
    }
    void RecvPacket(PieroTraceChannel *channel,PieroPacket *packet){
        Time now=Simulator::Now();
        if(recv_bytes_==0){
            start_=now;
        }
        recv_bytes_+=packet->length;
        if(recv_bytes_==2000000/8){
            end_=now;
            Time delta=end_-start_;
            int64_t bps=(recv_bytes_-packet->length)*8*1000/delta.GetMilliSeconds();
            std::cout<<"bps "<<bps<<std::endl;
            std::cout<<"delta "<<delta.GetMilliSeconds()<<std::endl;
        }
        count_++;
        delete packet;
    }
private:
    void GenerateFakePacket(){
        int bytes=2000000/8;
        std::cout<<"send bytes: " <<bytes<<std::endl;
        channel_->SendData(bytes);
    }
    Ptr<PieroTraceChannel> channel_;
    Time start_;
    Time end_;
    int64_t recv_bytes_=0;
    int count_=0;
};
}
using namespace ns3;
int main(){
    /*Ptr<PieroBiChannel> channel=CreateObject<PieroBiChannel>();
    MockApplication source(channel->GetSocketA());
    source.Start(100);
    MockApplication sink(channel->GetSocketB());
    Simulator::Stop (MilliSeconds(10000));
    Simulator::Run ();
    Simulator::Destroy();*/
    std::string video_path("/home/zsy/ns-allinone-3.31/ns-3.31/video_data/");
    std::string video_name("video_size_");
    int n=6;
    std::vector<std::string> video_log;
    for(int i=0;i<n;i++){
        std::string name=video_path+video_name+std::to_string(i);
        video_log.push_back(name);
    }
    Ptr<PieroBiChannel> channel=CreateObject<PieroBiChannel>();
    MockApplication source();
    Time start=MicroSeconds(10);
    Time stop=MilliSeconds(10*60*1000);
    Ptr<PieroDashServer> server=CreateObject<PieroDashServer>(channel->GetSocketB(),stop);
    std::string trace("client");
    Ptr<PieroDashClient> client=CreateObject<PieroDashClient>(video_log,trace,4000,3,channel->GetSocketA(),start,stop);
    //Simulator::Stop (MilliSeconds(10000));
    Simulator::Run ();
    Simulator::Destroy();    
}

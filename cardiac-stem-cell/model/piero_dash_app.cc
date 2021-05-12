#include <string.h>
#include "ns3/log.h"
#include "ns3/piero_udp_chan.h"
#include "piero_dash_app.h"
namespace ns3{
const char *piero_dash_app_name="piero-dash-app";
NS_LOG_COMPONENT_DEFINE(piero_dash_app_name);
DashUdpSource::DashUdpSource(std::vector<std::string> &video_log,std::vector<double> &average_rate,
                    std::string &trace_name,int segment_ms,int init_segments,
                    Time start,Time stop)
:PieroDashBase(video_log,average_rate,trace_name,segment_ms,init_segments,
                start,stop){}
DashUdpSource::~DashUdpSource(){
    NS_LOG_INFO("DashUdpSource dtor"<<this);
}
bool DashUdpSource::RegisterChannel(Ptr<UdpClientChannel> channel){
    bool found=false;
    int sz=channels_.size();
    for(int i=0;i<sz;i++){
        if(PeekPointer(channel)==PeekPointer(channels_[i])){
            found=true;
            break;
        }
    }
    if(!found){
        channel->SetDownloadDoneCallback(MakeCallback(&DashUdpSource::DownloadDone,this));
        channel->SetRecvCallBack(MakeCallback(&DashUdpSource::RecvPacket,this));
        channels_.push_back(channel);
    }
    return !found;
}
void DashUdpSource::RecvPacket(Ptr<UdpClientChannel> channel,int size){
    ReceiveOnePacket(size);
}
void DashUdpSource::DownloadDone(Ptr<UdpClientChannel> channel,int size){
#if defined (PIERO_LOG_RATE)
    LogChannelRate(channel);
#endif
    PostProcessingAfterPacket();
}
void DashUdpSource::OnRequestEvent(){
    if(video_data_.representation.size()>0){
        int segment_num=video_data_.representation[0].size();
        Time now=Simulator::Now();
        if(index_<segment_num){
            throughput_.transmissionRequested.push_back(now);
            NS_ASSERT(channels_.size()>0);
            bool ret=channels_[0]->RequestChunk(index_,segment_bytes_);
            NS_ASSERT(ret);
            index_++;
        }
    }
}
void DashUdpSource::FireTerminateSignal(){
    int sz=channels_.size();
    for(int i=0;i<sz;i++){
        channels_[i]->FireStopSignal();
    }
}
void DashUdpSource::LogChannelRate(Ptr<UdpClientChannel> channel){
#if defined (PIERO_LOG_RATE)
    if(f_rate_.is_open()){
        uintptr_t ptr=(uintptr_t)PeekPointer(channel);
        uint32_t bytes=0;
        Time t1,t2;
        channel->GetStats(bytes,t1,t2);
        double kbps=0.0;
        if(t2>t1){
            kbps=1.0*bytes*8/(t2-t1).GetMilliSeconds();
        }
        f_rate_<<index_<<"\t"<<ptr<<"\t"<<kbps<<std::endl;
    }
#endif
}
DashUdpSink::DashUdpSink(Time start,Time stop,DatasetDescriptation *des,DataRate init_rate):
PieroRateLimitBase(start,stop,des,init_rate){}
DashUdpSink::~DashUdpSink(){
    NS_LOG_INFO("DashUdpSink dtor"<<this);
}
void DashUdpSink::RegisterChannel(Ptr<UdpServerChannel> ch){
    channel_=ch;
    if(channel_){
        channel_->SetRecvDataStub(MakeCallback(&DashUdpSink::ReceivePacket,this));
    }
}
void DashUdpSink::ReceivePacket(Ptr<Packet> packet){
    OnIncomingPacket(packet);
}
void DashUdpSink::SendToNetwork(Ptr<Packet> packet){
    channel_->SendToNetwork(packet);
}
}

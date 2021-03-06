#include <string.h>
#include "ns3/log.h"
#include "ns3/data-rate.h"
#include "piero_udp_chan.h"
#include "piero_hunnan_chan.h"
#include "piero_dash_app.h"
#include "piero_flag.h"
namespace ns3{
const char *piero_dash_app_name="piero_dash_app";
NS_LOG_COMPONENT_DEFINE(piero_dash_app_name);
DashUdpSource::DashUdpSource(std::vector<std::string> &video_log,std::vector<double> &average_rate,
                    std::string &trace_name,int segment_ms,int init_segments,
                    Time start,Time stop)
:PieroDashBase(video_log,average_rate,trace_name,segment_ms,init_segments,
                start,stop){}
DashUdpSource::~DashUdpSource(){
#if (PIERO_MODULE_DEBUG)
    NS_LOG_INFO("DashUdpSource dtor"<<this);
#endif
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
#if  (PIERO_LOG_RATE)
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
#if  (PIERO_LOG_RATE)
    if(f_chan_rate_.is_open()){
        uintptr_t ptr=(uintptr_t)PeekPointer(channel);
        uint32_t bytes=0;
        Time t1,t2;
        channel->GetStats(bytes,t1,t2);
        double kbps=0.0;
        if(t2>t1){
            kbps=1.0*bytes*8/(t2-t1).GetMilliSeconds();
        }
        f_chan_rate_<<index_<<"\t"<<ptr<<"\t"<<kbps<<std::endl;
    }
#endif
}
DashUdpSink::DashUdpSink(Time start,Time stop,DatasetDescriptation *des,DataRate init_rate):
PieroRateLimitBase(start,stop,des,init_rate){}
DashUdpSink::~DashUdpSink(){
#if (PIERO_MODULE_DEBUG)
    NS_LOG_INFO("DashUdpSink dtor"<<this);
#endif
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

DashHunnanSource::DashHunnanSource(std::vector<std::string> &video_log,std::vector<double> &average_rate,
                    std::string &trace_name,int segment_ms,int init_segments,
                    Time start,Time stop)
:PieroDashBase(video_log,average_rate,trace_name,segment_ms,init_segments,
                start,stop){}
DashHunnanSource::~DashHunnanSource(){
#if (PIERO_MODULE_DEBUG)
    NS_LOG_INFO("DashHunnanSource dtor"<<this);
#endif
}
bool DashHunnanSource::RegisterChannel(Ptr<HunnanClientChannel> channel){
    bool found=false;
    int sz=channels_.size();
    for(int i=0;i<sz;i++){
        if(PeekPointer(channel)==PeekPointer(channels_[i])){
            found=true;
            break;
        }
    }
    if(!found){
        channel->SetDownloadDoneCallback(MakeCallback(&DashHunnanSource::DownloadDone,this));
        channel->SetRecvCallBack(MakeCallback(&DashHunnanSource::RecvPacket,this));
        channels_.push_back(channel);
    }
    return !found;
}
void DashHunnanSource::RecvPacket(Ptr<HunnanClientChannel> channel,int size){
    ReceiveOnePacket(size);
}
void DashHunnanSource::DownloadDone(Ptr<HunnanClientChannel> channel,int size){
#if  (PIERO_LOG_RATE)
    LogChannelRate(channel);
#endif
    PostProcessingAfterPacket();
}
Ptr<HunnanClientChannel> DashHunnanSource::GetChannel(int chan_id){
    Ptr<HunnanClientChannel> channel;
    if(chan_id>=0&&chan_id<(int)channels_.size()){
        channel=channels_.at(chan_id);
    }
    return channel;
}
void DashHunnanSource::OnRequestEvent(){
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
void DashHunnanSource::FireTerminateSignal(){
    int sz=channels_.size();
    for(int i=0;i<sz;i++){
        channels_[i]->FireStopSignal();
    }
}
void DashHunnanSource::LogChannelRate(Ptr<HunnanClientChannel> channel){
#if  (PIERO_LOG_RATE)
    if(f_chan_rate_.is_open()){
        uintptr_t ptr=(uintptr_t)PeekPointer(channel);
        uint32_t bytes=0;
        Time t1,t2;
        channel->GetStats(bytes,t1,t2);
        double kbps=0.0;
        if(t2>t1){
            kbps=1.0*bytes*8/(t2-t1).GetMilliSeconds();
        }
        f_chan_rate_<<channel->GetUuid()<<"\t"<<ptr<<"\t"<<kbps<<std::endl;
    }
#endif
}

DashChunkSelector::DashChunkSelector(std::vector<std::string> &video_log,std::vector<double> &average_rate,std::string &trace_name,
                int segment_ms,int init_segments,Time start,Time stop)
:DashHunnanSource(video_log,average_rate,trace_name,segment_ms,init_segments,start,stop){}
void DashChunkSelector::SetDispatchAlgo(uint8_t type,PieroPathInfo& info){
    switch (type){
        case DEF_CHUNK_SPLIT:{
            dispatcher_.reset(new DefaultChunkDispatch(this,ChunkDispatchInterface::CHUNK_SPLIT));
            break;
        }
        case DEF_CHUNK_UNSPLIT:{
            dispatcher_.reset(new DefaultChunkDispatch(this,ChunkDispatchInterface::CHUNK_UNSPLIT));
            break;
        }
        case EPS_CHUNK_SPLIT:{
            dispatcher_.reset(new EpsilonChunkDispatch(this,ChunkDispatchInterface::CHUNK_SPLIT));
            break;
        }
        case EPS_CHUNK_UNSPLIT:{
            dispatcher_.reset(new EpsilonChunkDispatch(this,ChunkDispatchInterface::CHUNK_UNSPLIT));
            break;
        }
        case UCB_CHUNK_SPLIT:{
            dispatcher_.reset(new UcbChunkDispatch(this,ChunkDispatchInterface::CHUNK_SPLIT));
            break;
        }
        case UCB_CHUNK_UNSPLIT:{
            dispatcher_.reset(new UcbChunkDispatch(this,ChunkDispatchInterface::CHUNK_UNSPLIT));
            break;
        }
    }
    NS_ASSERT(dispatcher_);
    dispatcher_->RegisterPathInfo(info);
}
ChunkDispatchInterface *DashChunkSelector::PeekDispatcher(){
    return dispatcher_.get();
}
void DashChunkSelector::ChunkDispatch(int chan_id,int chunk_id,uint64_t bytes){
    NS_ASSERT(chan_id>=0&&chan_id<(int)channels_.size());
    channels_.at(chan_id)->RequestChunk(chunk_id,bytes);
}
void DashChunkSelector::FireTerminateSignal(){
    round_timer_.Cancel();
    DashHunnanSource::FireTerminateSignal();
}
void DashChunkSelector::UpdateRound(){
    Time now=Simulator::Now();
    int sz=channels_.size();
    if(round_timer_.IsExpired()){
        for(int i=0;i<sz;i++){
            channels_.at(i)->UpdateRound(now);
        }
        Time next=MilliSeconds(100);
        round_timer_=Simulator::Schedule(next,&DashChunkSelector::UpdateRound,this);
    }
}
void DashChunkSelector::OnRequestEvent(){
    if(first_request_){
        round_timer_=Simulator::ScheduleNow(&DashChunkSelector::UpdateRound,this);
        first_request_=false;
    }
    if(video_data_.representation.size()>0){
        int segment_num=video_data_.representation[0].size();
        Time now=Simulator::Now();
        if(index_<segment_num){
            throughput_.transmissionRequested.push_back(now);
            NS_ASSERT(dispatcher_);
            dispatcher_->DispatchRequest(index_,segment_bytes_);
            index_++;
        }
    }
}

MultiNicMultiServerDash::MultiNicMultiServerDash(std::vector<std::string> &video_log,std::vector<double> &average_rate,std::string &trace_name,
                int segment_ms,int init_segments,Time start,Time stop)
:DashChunkSelector(video_log,average_rate,trace_name,segment_ms,init_segments,start,stop){}
MultiNicMultiServerDash::~MultiNicMultiServerDash(){
#if (PIERO_MODULE_DEBUG)
    NS_LOG_INFO("MultiNicMultiServerDash dtor"<<this);
#endif
}

DashHunnanSink::DashHunnanSink(Time start,Time stop,DatasetDescriptation *des,DataRate init_rate):
PieroRateLimitBase(start,stop,des,init_rate){
    if(des){
        dataset_name_=des->name;
    }
}
DashHunnanSink::~DashHunnanSink(){
#if (PIERO_MODULE_DEBUG)
    NS_LOG_INFO("DashHunnanSink dtor"<<this);
#endif
}
void DashHunnanSink::RegisterChannel(Ptr<HunnanServerChannel> ch){
    channel_=ch;
    if(channel_){
        channel_->SetRecvDataStub(MakeCallback(&DashHunnanSink::ReceivePacket,this));
    }
    channel_->GetNode()->GetDevice(0)->SetPacketDropTrace(
        MakeCallback(&DashHunnanSink::PacketDropTrace,this)
    );
}
void DashHunnanSink::PacketDropTrace(Ptr<const Packet> packet){
    NS_ASSERT_MSG(0,"drop packet "<<dataset_name_<<bandwidth_);
}
void DashHunnanSink::ReceivePacket(Ptr<Packet> packet){
    OnIncomingPacket(packet);
}
void DashHunnanSink::SendToNetwork(Ptr<Packet> packet){
    channel_->SendToNetwork(packet);
}

}

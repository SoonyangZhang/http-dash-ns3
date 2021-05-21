#include "piero_hunnan_chan.h"
#include "piero_flag.h"
#include  "piero_misc.h"
#include "ns3/simulator.h"
#include "ns3/log.h"
namespace ns3{
const char *piero_hunnan_chan_name="piero_hunnan_chan";
NS_LOG_COMPONENT_DEFINE(piero_hunnan_chan_name);
#define LOC piero_hunnan_chan_name<<__LINE__<<":"
HunnanClientChannel::HunnanClientChannel(Time max_processing_delay):mpt_(max_processing_delay){}
HunnanClientChannel::~HunnanClientChannel(){
#if (PIERO_MODULE_DEBUG)
    NS_LOG_INFO("HunnanClientChannel dtor"<<this);
#endif
}
bool HunnanClientChannel::RequestChunk(uint32_t id,uint32_t sz){
    if(dowloading_){
        return false;
    }
    chunk_start_time_=Time(0);
    chunk_end_time_=Time(0);
    chunk_size_=sz;
    chunk_recv_=0;
    last_packet_seq_=-1;
    bw_last_time_=Time(0);
    bw_resample_=true;
    dowloading_=true;
    PieroMessageHeader header(PieroUdpMessageType::PUM_REQ_TYPE);
    PieroChunkRequest req;
    req.id=id;
    req.sz=sz;
    header.SetChunkRequest(req);
    Ptr<Packet> p=Create<Packet>(0);
    p->AddHeader(header);
    SendToNetwork(p);
    NS_ASSERT(health_timer_.IsExpired());
    health_timer_=Simulator::Schedule(mpt_,&HunnanClientChannel::OnHealthEvent,this);
    return true;
}
void HunnanClientChannel::FireStopSignal(){
    PieroMessageHeader header(PieroUdpMessageType::PUM_STOP_TYPE);
    Ptr<Packet> p=Create<Packet>(0);
    p->AddHeader(header);
    SendToNetwork(p);
}
void HunnanClientChannel::GetStats(uint32_t &bytes,Time &start,Time &stop){
    bytes=chunk_recv_old_;
    start=chunk_start_time_;
    stop=chunk_end_time_;
}
DataRate HunnanClientChannel::GetEstimateBandwidth() const{
    return bw_est_;
}
void HunnanClientChannel::SetDownloadDoneCallback(Callback<void,Ptr<HunnanClientChannel>,int> cb){
    download_done_cb_=cb;
}
void HunnanClientChannel::SetRecvCallBack(Callback<void,Ptr<HunnanClientChannel>,int> cb){
    recv_packet_cb_=cb;
}
void HunnanClientChannel::DoDispose(){
    download_done_cb_=MakeNullCallback<void,Ptr<HunnanClientChannel>,int>();
    recv_packet_cb_=MakeNullCallback<void,Ptr<HunnanClientChannel>,int>();
    HunnanApplication::DoDispose();
}
void HunnanClientChannel::Receive(Ptr<Packet> packet){
    Time now=Simulator::Now();
    int sz=packet->GetSize();
#if defined (PIERO_HEADER_DBUG)
    Time delta(0);
    if(!last_packet_time_.IsZero()){
        delta=now-last_packet_time_;
    }
    last_packet_time_=now;
    PieroMessageTag tag;
    packet->PeekPacketTag (tag);
    if(PieroUdpMessageType::PUM_RES_TYPE==tag.GetType()){
        PieroChunkResponce *res=tag.PeekChunkResponce();
        int owd=now.GetMilliSeconds()-res->event_time;
        if(last_packet_seq_<0){
            NS_ASSERT(0==res->seq);
            last_packet_seq_=(int32_t)res->seq;
        }else{
            NS_ASSERT((last_packet_seq_+1)==(int32_t)res->seq);
            last_packet_seq_=(int32_t)res->seq;
        }
        //NS_LOG_INFO(LOC<<last_packet_seq_<<" "<<owd<<" "<<sz<<" "<<delta.GetMilliSeconds());
    }
#endif
    chunk_recv_old_=chunk_recv_;
    chunk_recv_+=sz;
    if(chunk_start_time_.IsZero()){
        chunk_start_time_=now;
    }
    chunk_end_time_=now;
    if(!recv_packet_cb_.IsNull()){
        recv_packet_cb_(this,sz);
    }
    bool done=false;
    if(chunk_size_==chunk_recv_){
#if  (PIERO_MODULE_DEBUG)
        if(chunk_end_time_>chunk_start_time_){
            double rate=1.0*chunk_recv_old_*8/(chunk_end_time_-chunk_start_time_).GetSeconds();
            NS_LOG_INFO(LOC<<" rate "<<rate);
        }
#endif
        done=true;   
    }
    UpdateBandwidth(now,sz,done);
    if(chunk_size_==chunk_recv_){
        dowloading_=false;
        health_timer_.Cancel();
        if(!download_done_cb_.IsNull()){
            download_done_cb_(this,chunk_recv_);
        }
#if  (PIERO_HEADER_DBUG)
    last_packet_time_=Time(0);
#endif
    }else{
        if(health_timer_.IsRunning()){
            health_timer_.Cancel();
            health_timer_=Simulator::Schedule(mpt_,&HunnanClientChannel::OnHealthEvent,this);
        }        
    }
}
void HunnanClientChannel::SendToNetwork(Ptr<Packet> packet){
    Send(packet);
}
void HunnanClientChannel::OnHealthEvent(){
    if(!download_done_cb_.IsNull()){
        NS_ASSERT_MSG(0,"packet loss");
        download_done_cb_(this,0);    
    }
}
void HunnanClientChannel::UpdateBandwidth(Time now,int bytes,bool done){
    if(bw_last_time_.IsZero()){
        bw_last_time_=now;
        bw_last_bytes_=sum_bytes_;
    }
    if((now>=bw_last_time_+MilliSeconds(kBandwidthUpdateTime))||done){
        Time delta=now-bw_last_time_;
        double bps=0.0;
        int numerator=sum_bytes_-bw_last_bytes_;
        if(!delta.IsZero()){
            bps=1.0*numerator*8000/delta.GetMilliSeconds();
        }
        bw_last_bytes_=sum_bytes_;
        bw_last_time_=now;
        if(bps>0){
            if(!bw_resample_){
                double old=bw_est_.GetBitRate();
                double smooth=(1.0-kBandwidthSmoothFactor)*old+kBandwidthSmoothFactor*bps;
                bw_est_=DataRate(smooth);
            }else{
                bw_est_=DataRate(bps);
                bw_resample_=false;
            }
        }
        //NS_LOG_INFO(LOC<<DataRate(bps)<<" "<<bw_est_);
    }
    sum_bytes_+=bytes;
}

HunnanServerChannel::HunnanServerChannel(){}
HunnanServerChannel::~HunnanServerChannel(){
#if (PIERO_MODULE_DEBUG)
    NS_LOG_INFO("HunnanServerChannel dtor"<<this);
#endif
}
void HunnanServerChannel::SetRecvDataStub(Callback<void,Ptr<Packet> > cb){
    recv_data_stub_=cb;
}
void HunnanServerChannel::SendToNetwork(Ptr<Packet> packet){
    Send(packet);
}
void HunnanServerChannel::DoDispose (void){
    recv_data_stub_=MakeNullCallback<void,Ptr<Packet> >();
    HunnanApplication::DoDispose();
}
void HunnanServerChannel::Receive(Ptr<Packet> packet){
    if(!recv_data_stub_.IsNull()){
        recv_data_stub_(packet);
    }
}
}

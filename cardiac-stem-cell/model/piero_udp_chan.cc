#include <time.h>
#include <algorithm>
#include <climits>
#include "ns3/log.h"
#include "piero_udp_chan.h"
namespace ns3{
const char *piero_udp_chan_name="piero-udp-chan";
NS_LOG_COMPONENT_DEFINE(piero_udp_chan_name);
#define LOC piero_udp_chan_name<<__LINE__<<":"
namespace {
    const uint32_t kPacketSize=1400;
}
MockPieroDashDispatcher::MockPieroDashDispatcher(Time start,Time stop):
start_time_(start),stop_time_(stop){
    chunk_stop_=5;
    chunk_sz_=kPacketSize*10;
}
void MockPieroDashDispatcher::RegisterChannel(Ptr<PieroClientChannel> channel){
    channel->SetDownloadDoneCallback(MakeCallback(&MockPieroDashDispatcher::DownloadDone,this));
    channels_.push_back(channel);
}
void MockPieroDashDispatcher::DownloadDone(Ptr<PieroClientChannel> channel,int size){
    if(size){
        if(chunk_id_<chunk_stop_){
            channel->RequestChunk(chunk_id_,chunk_sz_);
            chunk_id_++;
        }else{
            channel->FireStopSignal();
        }
    }
}
void MockPieroDashDispatcher::DoDispose (void){
    start_event_.Cancel();
    stop_event_.Cancel();
}
void MockPieroDashDispatcher::DoInitialize (void){
    NS_LOG_INFO(LOC);
    start_event_=Simulator::Schedule(start_time_,&MockPieroDashDispatcher::StartApplication,this);
    if(stop_time_!=Time(0)){
        stop_event_=Simulator::Schedule(stop_time_,&MockPieroDashDispatcher::StopApplication,this);
    }
}
void MockPieroDashDispatcher::StartApplication(){
    if(chunk_id_<chunk_stop_&&channels_.size()>0){
        channels_[0]->RequestChunk(chunk_id_,chunk_sz_);
        chunk_id_++;
    }
}
void MockPieroDashDispatcher::StopApplication(){}

PieroClientChannel::PieroClientChannel(Time max_processing_delay):mpt_(max_processing_delay){}
void PieroClientChannel::Bind(uint16_t port){
    if (socket_== NULL) {
        socket_ = Socket::CreateSocket (GetNode (),UdpSocketFactory::GetTypeId ());
        auto local = InetSocketAddress{Ipv4Address::GetAny (), port};
        auto res = socket_->Bind (local);
        NS_ASSERT (res == 0);
    }
    bind_port_=port;
    socket_->SetRecvCallback (MakeCallback(&PieroClientChannel::RecvPacket,this));
}
PieroClientChannel::~PieroClientChannel(){
    NS_LOG_INFO("PieroClientChannel dtor");
}
Address PieroClientChannel::GetLocalAddress(){
    Ptr<Node> node=GetNode();
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
    Ipv4Address local_ip = ipv4->GetAddress (1, 0).GetLocal ();
    InetSocketAddress socket_addr=InetSocketAddress{local_ip,bind_port_};
    Address addr=socket_addr;
    return addr;
}
void PieroClientChannel::ConfigurePeer(Address &addr){
    peer_addr_=addr;
}
bool PieroClientChannel::RequestChunk(uint32_t id,uint32_t sz){
    if(dowloading_){
        return false;
    }
    chunk_start_time_=Time(0);
    chunk_end_time_=Time(0);
    chunk_size_=sz;
    chunk_recv_=0;
    last_packet_seq_=-1;
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
    health_timer_=Simulator::Schedule(mpt_,&PieroClientChannel::OnHealthEvent,this);
    return true;
}
void PieroClientChannel::FireStopSignal(){
    PieroMessageHeader header(PieroUdpMessageType::PUM_STOP_TYPE);
    Ptr<Packet> p=Create<Packet>(0);
    p->AddHeader(header);
    SendToNetwork(p);
}
void PieroClientChannel::GetStats(uint32_t &bytes,Time &start,Time &stop){
    bytes=chunk_recv_old_;
    start=chunk_start_time_;
    stop=chunk_end_time_;
}
void PieroClientChannel::SetDownloadDoneCallback(Callback<void,Ptr<PieroClientChannel>,int> cb){
    download_done_cb_=cb;
}
void PieroClientChannel::SetRecvCallBack(Callback<void,Ptr<PieroClientChannel>,int> cb){
    recv_packet_cb_=cb;
}
void PieroClientChannel::SendToNetwork(Ptr<Packet> p){
    socket_->SendTo(p,0,peer_addr_);
}
void PieroClientChannel::RecvPacket(Ptr<Socket> socket){
    Time now=Simulator::Now();
    Address remoteAddr;
    auto packet = socket->RecvFrom (remoteAddr);
    int sz=packet->GetSize();
#if defined (PIEROMSGDEBUG)
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
    if(chunk_size_==chunk_recv_){
        dowloading_=false;
        if(chunk_end_time_>chunk_start_time_){
            double rate=1.0*chunk_recv_old_*8/(chunk_end_time_-chunk_start_time_).GetSeconds();
            NS_LOG_INFO(LOC<<" rate "<<rate);
        }
        health_timer_.Cancel();
        if(!download_done_cb_.IsNull()){
            download_done_cb_(this,chunk_recv_);
        }
#if defined (PIEROMSGDEBUG)
    last_packet_time_=Time(0);
#endif
    }else{
    if(health_timer_.IsRunning()){
        health_timer_.Cancel();
        health_timer_=Simulator::Schedule(mpt_,&PieroClientChannel::OnHealthEvent,this);
    }        
    }
}
void PieroClientChannel::OnHealthEvent(){
    if(!download_done_cb_.IsNull()){
        NS_LOG_ERROR(LOC<<" packet loss");
        NS_ASSERT(0);
        download_done_cb_(this,0);    
    }
}

PieroServerChannel::PieroServerChannel(DatasetDescriptation *des,DataRate init_rate){
    random_=CreateObject<UniformRandomVariable>();
    random_->SetStream(time(NULL));
    if(des){
        file_lines_=count_file_lines(des->name);
        bw_trace_.open(des->name);
        if(!bw_trace_){
            NS_ASSERT(0);
        }
        if(TimeUnit::TIME_S==des->time_unit){
            time_unit_=1000;
        }
        if(RateUnit::BW_Kbps==des->rate_unit){
            rate_unit_=1000;
        }else if(RateUnit::BW_Mbps==des->rate_unit){
            rate_unit_=1000000;
        }
        bool success=ReadBandwidthFromFile(0)&&ReadBandwidthFromFile(1);
        NS_ASSERT(success);
    }else{
        bandwidth_=init_rate;
    }
    
}

PieroServerChannel::~PieroServerChannel(){
    if(bw_trace_.is_open()){
        bw_trace_.close();
    }
}
void PieroServerChannel::Bind(uint16_t port){
    if (socket_== NULL) {
        socket_ = Socket::CreateSocket (GetNode (),UdpSocketFactory::GetTypeId ());
        auto local = InetSocketAddress{Ipv4Address::GetAny (), port};
        auto res = socket_->Bind (local);
        NS_ASSERT (res == 0);
    }
    bind_port_=port;
    socket_->SetRecvCallback (MakeCallback(&PieroServerChannel::RecvPacket,this));
}
Address PieroServerChannel::GetLocalAddress(){
    Ptr<Node> node=GetNode();
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
    Ipv4Address local_ip = ipv4->GetAddress (1, 0).GetLocal ();
    InetSocketAddress socket_addr=InetSocketAddress{local_ip,bind_port_};
    Address addr=socket_addr;
    return addr;
}
void PieroServerChannel::SetSeed(int stream){
    random_->SetStream(stream);
}
void PieroServerChannel::StartApplication(){
    if(file_lines_>1){
        Time next=trace_time_[1]-trace_time_[0];
        bandwidth_=trace_rate_[0];
        NS_LOG_INFO(LOC<<"trace rate "<<bandwidth_<<" "<<next.GetMilliSeconds());
        bandwidth_timer_=Simulator::Schedule(next,&PieroServerChannel::OnUpdateBandwidthEvent,this);
    }
}
void PieroServerChannel::StopApplication(){
    bandwidth_timer_.Cancel();
}
void PieroServerChannel::SendToNetwork(Ptr<Packet> p){
    NS_ASSERT(!peer_addr_.IsInvalid());
    socket_->SendTo(p,0,peer_addr_);
}
void PieroServerChannel::RecvPacket(Ptr<Socket> socket){
    if(seen_stop_){
        return;
    }
    Address remoteAddr;
    auto packet = socket->RecvFrom (remoteAddr);
    if(peer_addr_.IsInvalid()){
        peer_addr_=remoteAddr;
    }
    PieroMessageHeader header;
    packet->RemoveHeader(header);
    if(PieroUdpMessageType::PUM_REQ_TYPE==header.GetType()){
        PieroChunkRequest *req=header.PeekChunkRequest();
        uint32_t id=req->id;
        uint32_t bytes=req->sz;
        //NS_LOG_INFO(LOC<<id<<" "<<bytes);
        if(pacer_.IsRunning()||request_q_.size()>0){
            request_q_.push_back(std::make_pair(id,bytes));
        }else{
            chunk_pos_=id;
            request_bytes_=bytes;
            send_bytes_=0;
            packet_seq_=0;
            pacer_=Simulator::ScheduleNow(&PieroServerChannel::OnPacerEvent,this);
        }
    }else if(PieroUdpMessageType::PUM_STOP_TYPE==header.GetType()){
        seen_stop_=true;
        if(pacer_.IsExpired()){
            bandwidth_timer_.Cancel();
            NS_LOG_INFO(LOC<<"task done");
        }
    }
}
void PieroServerChannel::OnPacerEvent(){
    if(pacer_.IsExpired()&&request_bytes_>0){
        if(send_bytes_<request_bytes_){
            uint32_t pkt_sz=std::min(kPacketSize,request_bytes_-send_bytes_);
            send_bytes_+=pkt_sz;
            Ptr<Packet> p=Create<Packet>(pkt_sz);
            #if defined (PIEROMSGDEBUG)
            uint32_t ms=Simulator::Now().GetMilliSeconds();
            PieroChunkResponce res;
            res.id=chunk_pos_;
            res.seq=packet_seq_;
            res.event_time=ms;
            PieroMessageTag tag(PieroUdpMessageType::PUM_RES_TYPE);
            tag.SetChunkResponce(res);
            p->AddPacketTag(tag);
            #endif
            NS_ASSERT(p->GetSize()==pkt_sz);
            packet_seq_++;
            SendToNetwork(p);
            Time next=bandwidth_.CalculateBytesTxTime(pkt_sz);
            pacer_=Simulator::Schedule(next,&PieroServerChannel::OnPacerEvent,this);
        }else if(send_bytes_==request_bytes_){
            if(request_q_.size()>0){
                ProcessRequestQueue();
            }else{
                if(seen_stop_){
                    NS_LOG_INFO(LOC<<"task done");
                    bandwidth_timer_.Cancel();
                }
            }
        }
    }
}
void PieroServerChannel::OnUpdateBandwidthEvent(){
    if(file_lines_>1&&bandwidth_timer_.IsExpired()){
        trace_time_[0]=trace_time_[1];
        trace_rate_[0]=trace_rate_[1];
        if(ReadBandwidthFromFile(1)){
            Time next=trace_time_[1]-trace_time_[0];
            bandwidth_=trace_rate_[0];
            bandwidth_timer_=Simulator::Schedule(next,&PieroServerChannel::OnUpdateBandwidthEvent,this);
        }else{
            //read eof clear first;
            bw_trace_.clear();
            bw_trace_.seekg(0, std::ios::beg);
            int offset=random_->GetInteger(0,file_lines_-2);
            for(int i=0;i<offset;i++){
                std::string buffer;
                getline(bw_trace_,buffer);
            }
            bool success=ReadBandwidthFromFile(0)&&ReadBandwidthFromFile(1);
            NS_ASSERT(success);
            Time next=trace_time_[1]-trace_time_[0];
            bandwidth_=trace_rate_[0];
            bandwidth_timer_=Simulator::Schedule(next,&PieroServerChannel::OnUpdateBandwidthEvent,this);
        }
        NS_LOG_INFO(LOC<<"trace rate "<<bandwidth_);
    }
}
void PieroServerChannel::ProcessRequestQueue(){
    auto info=request_q_.front();
    request_q_.pop_front();
    chunk_pos_=info.first;
    request_bytes_=info.second;
    send_bytes_=0;
    packet_seq_=0;
    NS_LOG_FUNCTION(this);
    pacer_=Simulator::ScheduleNow(&PieroServerChannel::OnPacerEvent,this);
}
bool PieroServerChannel::ReadBandwidthFromFile(uint32_t index){
    bool ret=false;
    if(bw_trace_.is_open()&&!bw_trace_.eof()){
        std::string buffer;
        getline(bw_trace_,buffer);
            std::vector<std::string> numbers;
            buffer_split(buffer,numbers);
            if(numbers.size()<2){
                return ret;
            }
            int64_t ms=time_unit_*std::stod(numbers[0]);
            trace_time_[index]=MilliSeconds(ms);
            double bw=std::stod(numbers[1]);
            int64_t bps=rate_unit_*bw;
            trace_rate_[index]=DataRate(bps);
            //NS_LOG_INFO(trace_rate_[index]);
            ret=true;
    }
    return ret;
}

}

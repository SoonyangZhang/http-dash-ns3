#include <algorithm>
#include <climits>
#include "piero.h"
#include "piero_misc.h"
#include "piero_header.h"
#include "piero_udp_chan.h"
#include "ns3/log.h"
namespace ns3{
const char *piero_udp_chan_name="piero_udp_chan";
NS_LOG_COMPONENT_DEFINE(piero_udp_chan_name);
#define LOC piero_udp_chan_name<<__LINE__<<":"
UdpClientChannel::UdpClientChannel(Time max_processing_delay):mpt_(max_processing_delay){}
void UdpClientChannel::Bind(uint16_t port){
    if (socket_== NULL) {
        socket_ = Socket::CreateSocket (GetNode (),UdpSocketFactory::GetTypeId ());
        auto local = InetSocketAddress{Ipv4Address::GetAny (), port};
        auto res = socket_->Bind (local);
        NS_ASSERT (res == 0);
    }
    bind_port_=port;
    socket_->SetRecvCallback (MakeCallback(&UdpClientChannel::RecvPacket,this));
}
UdpClientChannel::~UdpClientChannel(){
    NS_LOG_INFO("UdpClientChannel dtor"<<this);
}
Address UdpClientChannel::GetLocalAddress(){
    Ptr<Node> node=GetNode();
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
    Ipv4Address local_ip = ipv4->GetAddress (1, 0).GetLocal ();
    InetSocketAddress socket_addr=InetSocketAddress{local_ip,bind_port_};
    Address addr=socket_addr;
    return addr;
}
void UdpClientChannel::ConfigurePeer(Address &addr){
    peer_addr_=addr;
}
bool UdpClientChannel::RequestChunk(uint32_t id,uint32_t sz){
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
    health_timer_=Simulator::Schedule(mpt_,&UdpClientChannel::OnHealthEvent,this);
    return true;
}
void UdpClientChannel::FireStopSignal(){
    PieroMessageHeader header(PieroUdpMessageType::PUM_STOP_TYPE);
    Ptr<Packet> p=Create<Packet>(0);
    p->AddHeader(header);
    SendToNetwork(p);
}
void UdpClientChannel::GetStats(uint32_t &bytes,Time &start,Time &stop){
    bytes=chunk_recv_old_;
    start=chunk_start_time_;
    stop=chunk_end_time_;
}
void UdpClientChannel::SetDownloadDoneCallback(Callback<void,Ptr<UdpClientChannel>,int> cb){
    download_done_cb_=cb;
}
void UdpClientChannel::SetRecvCallBack(Callback<void,Ptr<UdpClientChannel>,int> cb){
    recv_packet_cb_=cb;
}
void UdpClientChannel::DoDispose(){
    download_done_cb_=MakeNullCallback<void,Ptr<UdpClientChannel>,int>();
    recv_packet_cb_=MakeNullCallback<void,Ptr<UdpClientChannel>,int>();
    Application::DoDispose();
}
void UdpClientChannel::SendToNetwork(Ptr<Packet> p){
    socket_->SendTo(p,0,peer_addr_);
}
void UdpClientChannel::RecvPacket(Ptr<Socket> socket){
    Time now=Simulator::Now();
    Address remoteAddr;
    auto packet = socket->RecvFrom (remoteAddr);
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
#if defined (PIERO_HEADER_DBUG)
    last_packet_time_=Time(0);
#endif
    }else{
    if(health_timer_.IsRunning()){
        health_timer_.Cancel();
        health_timer_=Simulator::Schedule(mpt_,&UdpClientChannel::OnHealthEvent,this);
    }        
    }
}
void UdpClientChannel::OnHealthEvent(){
    if(!download_done_cb_.IsNull()){
        NS_ASSERT_MSG(0,"packet loss");
        download_done_cb_(this,0);    
    }
}

UdpServerChannel::UdpServerChannel(){}

UdpServerChannel::~UdpServerChannel(){
    NS_LOG_INFO("UdpServerChannel dtor"<<this);
}
void UdpServerChannel::Bind(uint16_t port){
    if (socket_== NULL) {
        socket_ = Socket::CreateSocket (GetNode (),UdpSocketFactory::GetTypeId ());
        auto local = InetSocketAddress{Ipv4Address::GetAny (), port};
        auto res = socket_->Bind (local);
        NS_ASSERT (res == 0);
    }
    bind_port_=port;
    socket_->SetRecvCallback (MakeCallback(&UdpServerChannel::RecvPacket,this));
}
Address UdpServerChannel::GetLocalAddress(){
    Ptr<Node> node=GetNode();
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
    Ipv4Address local_ip = ipv4->GetAddress (1, 0).GetLocal ();
    InetSocketAddress socket_addr=InetSocketAddress{local_ip,bind_port_};
    Address addr=socket_addr;
    return addr;
}
void UdpServerChannel::SetRecvDataStub(Callback<void,Ptr<Packet> > cb){
    recv_data_stub_=cb;
}
void UdpServerChannel::DoDispose (void){
    recv_data_stub_=MakeNullCallback<void,Ptr<Packet> >();
    Application::DoDispose();
}
void UdpServerChannel::SendToNetwork(Ptr<Packet> p){
    NS_ASSERT(!peer_addr_.IsInvalid());
    socket_->SendTo(p,0,peer_addr_);
}
void UdpServerChannel::RecvPacket(Ptr<Socket> socket){
    Address remoteAddr;
    auto packet = socket->RecvFrom (remoteAddr);
    if(peer_addr_.IsInvalid()){
        peer_addr_=remoteAddr;
    }
    if(!recv_data_stub_.IsNull()){
        recv_data_stub_(packet);
    }
}
}

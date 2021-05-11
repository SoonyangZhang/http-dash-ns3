#include <unistd.h>
#include <iostream>
#include "ns3/core-module.h"
#include "ns3/log.h"
#include "piero_dash_client.h"
namespace ns3{
NS_LOG_COMPONENT_DEFINE("piero_dash_client");
PieroDashRequest::PieroDashRequest(int request_bytes){
    sent_time=Simulator::Now().GetMilliSeconds();
    length=sizeof(int);
    extension=1;
    request_bytes_=request_bytes;
}

PieroDashServer::PieroDashServer(Ptr<PieroSocket> socket,StopBroadcast *broadcast){
    channel_=CreateObject<PieroTraceChannel>(socket,broadcast);
    channel_->SetRecvCallback(MakeCallback(&PieroDashServer::RecvPacket,this));    
}
void PieroDashServer::SetBandwidthTrace(DatasetDescriptation &des,Time interval){
    channel_->SetBandwidthTrace(des,interval);
}
void PieroDashServer::RecvPacket(PieroTraceChannel *channel,PieroPacket *packet){
    if(packet->extension){
        PieroDashRequest *request=static_cast<PieroDashRequest*>(packet);
        int bytes=request->RequestBytes();
        channel_->SendData(bytes);
    }
    delete packet;
}
PieroDashClient::PieroDashClient(std::vector<std::string> &video_log,std::vector<double> &average_rate,
                std::string &trace_name,int segment_ms,int init_segments,
                Ptr<PieroSocket> socket,Time start,Time stop)
:PieroDashBase(video_log,average_rate,trace_name,segment_ms,init_segments,start,stop){
    channel_=CreateObject<PieroTraceChannel>(socket,&broadcast_);
    channel_->SetSeed(12321);
    channel_->SetRecvCallback(MakeCallback(&PieroDashClient::RecvPacket,this));
}
PieroDashClient::~PieroDashClient(){}
void PieroDashClient::RecvPacket(PieroTraceChannel *channel,PieroPacket *packet){
    OnReadEvent(packet->length);
    delete packet;
}
void PieroDashClient::OnReadEvent(int bytes){
    ReceiveOnePacket(bytes);
    PostProcessingAfterPacket();
}
void PieroDashClient::FireTerminateSignal(){
    request_timer_.Cancel();
    player_timer_.Cancel();
    broadcast_.Fire();
    player_state_=PLAYER_DONE;
}
void PieroDashClient::OnRequestEvent(){
    if(video_data_.representation.size()>0){
        int segment_num=video_data_.representation[0].size();
        Time now=Simulator::Now();
        if(index_<segment_num){
            throughput_.transmissionRequested.push_back(now);
            SendRequest(segment_bytes_);
            index_++;
        }
    }    
}
void PieroDashClient::SendRequest(int bytes){
    if(channel_){
        PieroDashRequest *request=new PieroDashRequest(bytes);
        channel_->SendMessage(request);
    }
}
}

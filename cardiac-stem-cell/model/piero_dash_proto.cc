#include "ns3/core-module.h"
#include "piero_dash_proto.h"

namespace ns3{
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
}

#include "ns3/core-module.h"
#include "piero_dash_proto.h"

namespace ns3{
PieroDashRequest::PieroDashRequest(int request_bytes){
    sent_time=Simulator::Now().GetMilliSeconds();
    length=sizeof(int);
    extension=1;
    request_bytes_=request_bytes;
}
PieroDashServer::PieroDashServer(Ptr<PieroSocket> socket,Time stop){
    channel_=CreateObject<PieroTraceChannel>(socket,stop);
    channel_->SetRecvCallback(MakeCallback(&PieroDashServer::RecvPacket,this));    
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
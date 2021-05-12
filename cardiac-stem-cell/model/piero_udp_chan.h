#pragma once
#include <string>
#include <fstream>
#include <deque>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
namespace ns3{
class UdpClientChannel: public Application{
public:
    UdpClientChannel(Time max_processing_delay);
    ~UdpClientChannel();
    void Bind(uint16_t port);
    Address GetLocalAddress();
    void ConfigurePeer(Address &addr);
    bool RequestChunk(uint32_t id,uint32_t sz);
    void FireStopSignal();
    void GetStats(uint32_t &bytes,Time &start,Time &stop);
    void SetDownloadDoneCallback(Callback<void,Ptr<UdpClientChannel>,int> cb);
    void SetRecvCallBack(Callback<void,Ptr<UdpClientChannel>,int> cb);
private:
    virtual void DoDispose (void);
    void StartApplication() override {}
    void StopApplication() override {}
    void SendToNetwork(Ptr<Packet> p);
    void RecvPacket(Ptr<Socket> socket);
    void OnHealthEvent();
    Time mpt_;//link maxmium processing delay
    uint16_t bind_port_=0;
    Address peer_addr_;
    uint32_t chunk_size_=0;
    uint32_t chunk_recv_old_=0;
    uint32_t chunk_recv_=0;
    int32_t last_packet_seq_=-1;
    Time chunk_start_time_;
    Time chunk_end_time_;
    Ptr<Socket> socket_;
    EventId health_timer_;
    bool dowloading_=false;
    Callback<void,Ptr<UdpClientChannel>,int> download_done_cb_;
    Callback<void,Ptr<UdpClientChannel>,int>  recv_packet_cb_;
#if defined (PIERO_HEADER_DBUG)
    Time last_packet_time_=Time(0);
#endif
};
//one server app only serves one client app.
class UdpServerChannel: public Application{
public:
    UdpServerChannel();
    ~UdpServerChannel();
    void Bind(uint16_t port);
    Address GetLocalAddress();
    void SetRecvDataStub(Callback<void,Ptr<Packet> > cb);
    void SendToNetwork(Ptr<Packet> p);
private:
    virtual void DoDispose (void);
    void StartApplication() override{}
    void StopApplication() override{}
    void RecvPacket(Ptr<Socket> socket);
    Ptr<Socket> socket_;
    uint16_t bind_port_=0;
    Address peer_addr_;
    Callback<void,Ptr<Packet> > recv_data_stub_;
};
}

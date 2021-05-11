#pragma once
#include <string>
#include <fstream>
#include <deque>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/piero.h"
#include "piero_udp_message.h"
namespace ns3{
class PieroClientChannel;
class MockPieroDashDispatcher: public Object{
public:
    MockPieroDashDispatcher(Time start,Time stop=Time(0));
    void RegisterChannel(Ptr<PieroClientChannel> channel);
    void DownloadDone(Ptr<PieroClientChannel> channel,int size);
private:
    void DoDispose (void) override;
    void DoInitialize (void) override;
    void StartApplication();
    void StopApplication();
    Time start_time_;
    Time stop_time_;
    EventId start_event_;
    EventId stop_event_;
    int chunk_id_=0;
    int chunk_stop_=0;
    int chunk_sz_=0;
    std::deque<Ptr<PieroClientChannel>> channels_;
};
class PieroClientChannel: public Application{
public:
    PieroClientChannel(Time max_processing_delay);
    ~PieroClientChannel();
    void Bind(uint16_t port);
    Address GetLocalAddress();
    void ConfigurePeer(Address &addr);
    bool RequestChunk(uint32_t id,uint32_t sz);
    void FireStopSignal();
    void GetStats(uint32_t &bytes,Time &start,Time &stop);
    void SetDownloadDoneCallback(Callback<void,Ptr<PieroClientChannel>,int> cb);
    void SetRecvCallBack(Callback<void,Ptr<PieroClientChannel>,int> cb);
private:
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
    Callback<void,Ptr<PieroClientChannel>,int> download_done_cb_;
    Callback<void,Ptr<PieroClientChannel>,int>  recv_packet_cb_;
#if defined (PIEROMSGDEBUG)
    Time last_packet_time_=Time(0);
#endif
};
//one server app only serves one client app.
class PieroServerChannel: public Application{
public:
    PieroServerChannel(DatasetDescriptation *des,DataRate init_rate=DataRate(2000000));
    ~PieroServerChannel();
    void Bind(uint16_t port);
    Address GetLocalAddress();
    void SetSeed(int stream);
    
private:
    void StartApplication() override;
    void StopApplication() override;
    void SendToNetwork(Ptr<Packet> p);
    void RecvPacket(Ptr<Socket> socket);
    void OnPacerEvent();
    void OnUpdateBandwidthEvent();
    void ProcessRequestQueue();
    bool ReadBandwidthFromFile(uint32_t index);
    Ptr<UniformRandomVariable> random_;
    uint32_t file_lines_=0;
    std::fstream bw_trace_;
    DataRate bandwidth_;
    int64_t time_unit_=1;
    int64_t rate_unit_=1;
    Time trace_time_[2];
    DataRate trace_rate_[2];
    Ptr<Socket> socket_;
    uint16_t bind_port_=0;
    Address peer_addr_;
    EventId bandwidth_timer_;
    EventId pacer_;
    std::deque<std::pair<uint8_t,uint32_t>> request_q_;
    bool seen_stop_=false;
    int32_t chunk_pos_=-1;
    uint32_t request_bytes_=0;
    uint32_t send_bytes_=0;
    uint32_t packet_seq_=0;
};
}

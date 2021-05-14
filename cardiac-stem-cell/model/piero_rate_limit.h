#pragma once
#include <fstream>
#include "ns3/ptr.h"
#include "ns3/object.h"
#include "ns3/event-id.h"
#include "ns3/data-rate.h"
#include "ns3/random-variable-stream.h"
#include "ns3/piero.h"
namespace ns3{
class PieroRateLimitBase:public Object{
public:
    PieroRateLimitBase(Time start,Time stop,DatasetDescriptation *des,DataRate init_rate);
    virtual ~PieroRateLimitBase();
    void SetSeed(int stream);
protected:
    virtual void DoDispose (void);
    virtual void DoInitialize (void);
    virtual void SendToNetwork(Ptr<Packet> packet)=0;
    void OnIncomingPacket(Ptr<Packet> packet);
    void StartApplication();
    void StopApplication();
    
    DataRate bandwidth_;
private:
    void OnPacerEvent();
    void OnUpdateBandwidthEvent();
    void ProcessRequestQueue();
    bool ReadBandwidthFromFile(uint32_t index);
    
    Time start_time_;
    Time stop_time_;
    EventId start_event_;
    EventId stop_event_;
    EventId bandwidth_timer_;
    EventId pacer_;
    Ptr<UniformRandomVariable> random_;
    uint32_t file_lines_=0;
    std::fstream bw_trace_;
    int64_t time_unit_=1;
    int64_t rate_unit_=1;
    Time trace_time_[2];
    DataRate trace_rate_[2];
    std::deque<std::pair<uint8_t,uint32_t>> request_q_;
    bool seen_stop_=false;
    int32_t chunk_pos_=-1;
    uint32_t request_bytes_=0;
    uint32_t send_bytes_=0;
    uint32_t packet_seq_=0;
};
}

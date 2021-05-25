#pragma once
#include <vector>
#include <memory>
#include "ns3/event-id.h"
#include "ns3/piero_dash_base.h"
#include "ns3/piero_rate_limit.h"
#include "ns3/piero_chunk_dispatch.h"
namespace ns3{
class UdpClientChannel;
class UdpServerChannel;
class DashUdpSource :public PieroDashBase{
public:
    DashUdpSource(std::vector<std::string> &video_log,std::vector<double> &average_rate,std::string &trace_name,
                int segment_ms,int init_segments,Time start,Time stop=Time(0));
    virtual ~DashUdpSource();
    bool RegisterChannel(Ptr<UdpClientChannel> channel);
    void RecvPacket(Ptr<UdpClientChannel> channel,int size);
    virtual void DownloadDone(Ptr<UdpClientChannel> channel,int size);
protected:
    void OnRequestEvent() override;
    void FireTerminateSignal() override;
    void LogChannelRate(Ptr<UdpClientChannel> channel);
    std::deque<Ptr<UdpClientChannel>> channels_;
};
class DashUdpSink:public PieroRateLimitBase{
public:
    DashUdpSink(Time start,Time stop=Time(0),DatasetDescriptation *des=nullptr,DataRate init_rate=DataRate(2000000));
    ~DashUdpSink();
    void RegisterChannel(Ptr<UdpServerChannel> ch);
    void ReceivePacket(Ptr<Packet> packet);
private:
    void SendToNetwork(Ptr<Packet> packet) override;
    Ptr<UdpServerChannel> channel_;
};
class HunnanClientChannel;
class HunnanServerChannel;
class DashHunnanSource :public PieroDashBase{
public:
    DashHunnanSource(std::vector<std::string> &video_log,std::vector<double> &average_rate,std::string &trace_name,
                int segment_ms,int init_segments,Time start,Time stop=Time(0));
    virtual ~DashHunnanSource();
    bool RegisterChannel(Ptr<HunnanClientChannel> channel);
    void RecvPacket(Ptr<HunnanClientChannel> channel,int size);
    virtual void DownloadDone(Ptr<HunnanClientChannel> channel,int size);
    Ptr<HunnanClientChannel> GetChannel(int chan_id);
protected:
    virtual void OnRequestEvent();
    virtual void FireTerminateSignal();
    void LogChannelRate(Ptr<HunnanClientChannel> channel);
    std::deque<Ptr<HunnanClientChannel>> channels_;
};
class DashChunkSelector:public DashHunnanSource{
public:
    DashChunkSelector(std::vector<std::string> &video_log,std::vector<double> &average_rate,std::string &trace_name,
                int segment_ms,int init_segments,Time start,Time stop);
    virtual ~DashChunkSelector(){}
    void SetDispatchAlgo(uint8_t type,PieroPathInfo& info);
    ChunkDispatchInterface *PeekDispatcher();
    void ChunkDispatch(int chan_id,int chunk_id,uint64_t bytes);
protected:
    void OnRequestEvent() override;
    void FireTerminateSignal() override;
    void UpdateRound();
    bool first_request_=true;
    EventId round_timer_;
    std::unique_ptr<ChunkDispatchInterface> dispatcher_;
};
// one nic, multiple servers
// for each chunk, determine which server to download from.
//two nic, multiple servers
// for each chunk, split it and choose two server to download from.
class MultiNicMultiServerDash:public DashChunkSelector{
public:
    MultiNicMultiServerDash(std::vector<std::string> &video_log,std::vector<double> &average_rate,std::string &trace_name,
                int segment_ms,int init_segments,Time start,Time stop=Time(0));
    ~MultiNicMultiServerDash();
};

class DashHunnanSink:public PieroRateLimitBase{
public:
    DashHunnanSink(Time start,Time stop=Time(0),DatasetDescriptation *des=nullptr,DataRate init_rate=DataRate(2000000));
    ~DashHunnanSink();
    void RegisterChannel(Ptr<HunnanServerChannel> ch);
    void ReceivePacket(Ptr<Packet> packet);
    void PacketDropTrace(Ptr<const Packet> packet);
private:
    void SendToNetwork(Ptr<Packet> packet) override;
    Ptr<HunnanServerChannel> channel_;
    std::string  dataset_name_;
};
}

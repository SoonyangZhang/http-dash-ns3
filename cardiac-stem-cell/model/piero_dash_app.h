#pragma once
#include "ns3/piero_dash_base.h"
#include "ns3/piero_rate_limit.h"
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
}

#pragma once
#include "ns3/piero_udp_chan.h"
#include "ns3/piero_dash_base.h"
namespace ns3{
class PieroDashApp :public PieroDashBase{
public:
    PieroDashApp(std::vector<std::string> &video_log,std::vector<double> &average_rate,std::string &trace_name,
                int segment_ms,int init_segments,Time start,Time stop=Time(0));
    virtual ~PieroDashApp();
    bool RegisterChannel(Ptr<PieroClientChannel> channel);
    void RecvPacket(Ptr<PieroClientChannel> channel,int size);
    virtual void DownloadDone(Ptr<PieroClientChannel> channel,int size);
protected:
    void OnRequestEvent() override;
    void FireTerminateSignal() override;
    void LogChannelRate(Ptr<PieroClientChannel> channel);
    std::deque<Ptr<PieroClientChannel>> channels_;
};
}

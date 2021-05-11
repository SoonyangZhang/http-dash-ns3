#include <vector>
#include <memory>
#include <fstream>
#include "piero.h"
#include "piero_misc.h"
#include "piero_dash_base.h"
namespace ns3{
class PieroDashRequest:public PieroPacket{
public:
    PieroDashRequest(int request_bytes);
    int RequestBytes() const {return request_bytes_;}
private:
    int request_bytes_=0;
};

class PieroDashServer:public Object{
public:
    PieroDashServer(Ptr<PieroSocket> socket,StopBroadcast *broadcast);
    void SetBandwidthTrace(DatasetDescriptation &des,Time interval);
    void RecvPacket(PieroTraceChannel *channel,PieroPacket *packet);
private:
    Ptr<PieroTraceChannel> channel_;
};

class PieroDashClient:public PieroDashBase{
public:
    PieroDashClient(std::vector<std::string> &video_log,std::vector<double> &average_rate,
                std::string &trace_name,int segment_ms,int init_segments,
                Ptr<PieroSocket> socket,Time start,Time stop=Time(0));
    ~PieroDashClient();
    void RecvPacket(PieroTraceChannel *channel,PieroPacket *packet);
    StopBroadcast* GetBroadcast() {return &broadcast_;}
private:
    void FireTerminateSignal() override;
    void OnRequestEvent() override;
    void OnReadEvent(int bytes);
    void RequestSegment();
    void SendRequest(int bytes);
    Ptr<PieroTraceChannel> channel_;
    StopBroadcast broadcast_;
};
}

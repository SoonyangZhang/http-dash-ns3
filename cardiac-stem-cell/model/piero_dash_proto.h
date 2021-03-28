#pragma once
#include <vector>
#include <deque>
#include <chrono>
#include "piero.h"
namespace ns3{
const int kFramesPerSecond=25;
const int kInitialSegments=3;
const int kThroughputHorizon=5;
const int kFestiveTarget=20000;
const int kFestiveHorizon=5;
const double kRebufPenality=4.3;
extern const char *g_rl_server_ip;
extern uint16_t g_rl_server_port;
static volatile bool g_running=true;
static int kDefaultQuality=1;
struct AlgorithmReply
{
  AlgorithmReply():nextQuality(kDefaultQuality),nextDownloadDelay(Time(0)),
  terminate(false){}
  int nextQuality;
  Time nextDownloadDelay;
  bool terminate;
};
struct ThroughputData
{
  std::deque<Time> transmissionRequested;       //!< Simulation time in microseconds when a segment was requested by the client
  std::deque<Time> transmissionStart;       //!< Simulation time in microseconds when the first packet of a segment was received
  std::deque<Time> transmissionEnd;       //!< Simulation time in microseconds when the last packet of a segment was received
  std::deque<int64_t> bytesReceived;       //!< Number of bytes received, i.e. segment size
};
struct BufferData
{
  std::vector<Time> timeNow;       //!< current simulation time
  std::vector<int64_t> bufferLevelOld;       //!< buffer level in microseconds before adding segment duration (in microseconds) of just downloaded segment
  std::vector<int64_t> bufferLevelNew;       //!< buffer level in microseconds after adding segment duration (in microseconds) of just downloaded segment
};
typedef std::vector<int64_t> SegmentSize;
struct VideoData
{
  std::vector<SegmentSize> representation;       //!< vector holding representation levels in the first dimension and their particular segment sizes in bytes in the second dimension
  std::vector <double> averageBitrate;       //!< holding the average bitrate of a segment in representation i in bits
  int64_t segmentDuration;       //!< duration of a segment in microseconds
}; 
struct PlaybackData
{
  std::vector <int64_t> playbackIndex;       //!< Index of the video segment
  std::vector <Time> playbackStart; //!< Point in time in microseconds when playback of this segment started
};
class PieroDashRequest:public PieroPacket{
public:
    PieroDashRequest(int request_bytes);
    int RequestBytes() const {return request_bytes_;}
private:
    int request_bytes_=0;
};
class PieroDashClient;
class AdaptationAlgorithm{
public:
    virtual ~AdaptationAlgorithm(){}
    virtual AlgorithmReply GetNextQuality(PieroDashClient *client,Time now,int pre_quality,int segment_count)=0;
    virtual void LastSegmentArrive(PieroDashClient *client){}
};
class PieroDashServer:public Object{
public:
    PieroDashServer(Ptr<PieroSocket> socket,StopBroadcast *broadcast);
    void SetBandwidthTrace(DatasetDescriptation &des,Time interval);
    void RecvPacket(PieroTraceChannel *channel,PieroPacket *packet);
private:
    Ptr<PieroTraceChannel> channel_;
};
inline int64_t WallTimeNowInUsec(){
    std::chrono::system_clock::duration d = std::chrono::system_clock::now().time_since_epoch();    
    std::chrono::microseconds mic = std::chrono::duration_cast<std::chrono::microseconds>(d);
    return mic.count(); 
}
inline int64_t TimeMillis(){
    return WallTimeNowInUsec()/1000;
}
}

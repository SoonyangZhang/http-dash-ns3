#pragma once
#include <string>
#include <deque>
#include <vector>
#include "ns3/nstime.h"
namespace ns3{
const int kFramesPerSecond=25;
const int kInitialSegments=3;
const int kThroughputHorizon=5;
const int kFestiveTarget=20000;
const int kFestiveHorizon=5;
const double kRebufPenality=4.3;
static int kDefaultQuality=1;
const float kMUnit=1000000;
enum DashPlayerState{
    PLAYER_NOT_STARTED,
    PLAYER_INIT_BUFFERING,
    PLAYER_PLAY,
    PLAYER_PAUSED,
    PLAYER_DONE,
};
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
class PieroDashBase;
class AdaptationAlgorithm{
public:
    virtual ~AdaptationAlgorithm(){}
    virtual AlgorithmReply GetNextQuality(PieroDashBase *client,Time now,int pre_quality,int segment_count)=0;
    virtual void LastSegmentArrive(PieroDashBase *client){}
};
void ReadSegmentFromFile(std::vector<std::string>& video_log,struct VideoData &video_data);
bool MakePath(const std::string& path);
void GetFiles(std::string &cate_dir,std::vector<std::string> &files);
int64_t PieroTimeMillis();
int CountFileLines(std::string &name);
void BufferSplit(std::string &line,std::vector<std::string>&numbers);
}

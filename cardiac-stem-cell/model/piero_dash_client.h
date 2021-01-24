#include <vector>
#include <memory>
#include <fstream>
#include "piero_dash_proto.h"
namespace ns3{
enum DashPlayerState{
    PLAYER_NOT_STARTED,
    PLAYER_INIT_BUFFERING,
    PLAYER_PLAY,
    PLAYER_PAUSED,
    PLAYER_DONE,
};
void ReadSegmentFromFile(std::vector<std::string>& video_log,struct VideoData &video_data);
class PieroDashClient:public Object{
public:
    PieroDashClient(std::vector<std::string> &video_log,std::string &trace_name,int segment_ms,int init_segments,Ptr<PieroSocket> socket,Time start,Time stop);
    ~PieroDashClient();
    void RecvPacket(PieroTraceChannel *channel,PieroPacket *packet);
    void SetAdaptationAlgorithm(std::string &algo);
    void StartApplication();
    const VideoData & get_video_data() {return video_data_ ;}
    const ThroughputData &get_throughput(){return throughput_;}
    const std::deque<int> & get_history_quality(){return history_quality_;}
    const BufferData & get_buffer_data(){return buffer_data_;}
    int  get_played_frames() {return played_frames_;}
    int64_t get_buffer_level_ms();
    int64_t get_buffer_diff_ms();
private:
    void OnReadEvent(int bytes);
    void RequestSegment();
    void OnRequestEvent();
    void SendRequest(int bytes);
    void CheckBufferStatus();
    void OnPlayBackEvent();
    void LogInfomation();
    void CalculateQoE();
    void OpenTrace(std::string &name);
    void CloseTrace();
    Ptr<PieroTraceChannel> channel_;
    struct VideoData video_data_;
    ThroughputData throughput_;
    BufferData buffer_data_;
    std::unique_ptr<AdaptationAlgorithm> algorithm_;
    std::deque<int> history_quality_;
    DashPlayerState player_state_=PLAYER_NOT_STARTED;
    int quality_=0;
    int index_=0;
    int playback_index_=0;
    int played_frames_=0;
    int frames_in_segment_=0;
    int64_t segments_in_buffer_=0;
    uint64_t segment_bytes_=0;
    uint64_t read_bytes_=0;
    uint64_t session_bytes_=0;
    Time first_request_time_=Time(0);
    Time pause_start_=Time(0);
    Time pause_time_=Time(0);
    Time startup_time_=Time(0);
    int init_segments_;
    int segment_counter_=0;
    EventId player_timer_;
    EventId request_timer_;
    std::fstream m_trace;
};    
}

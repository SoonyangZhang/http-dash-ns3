#pragma once
#include <string>
#include <memory>
#include <vector>
#include <fstream>

#include "ns3/nstime.h"
#include "ns3/object.h"
#include "ns3/callback.h"
#include "ns3/event-id.h"
#include "piero_misc.h"
namespace ns3{
#define PIERO_LOG_RATE 1
void piero_set_trace_root_folder(const char *name);
class PieroDashBase: public Object{
public:
    PieroDashBase(std::vector<std::string> &video_log,std::vector<double> &average_rate,
                    std::string &trace_name,int segment_ms,int init_segments,
                    Time start,Time stop);
    virtual ~PieroDashBase();
    void SetAdaptationAlgorithm(std::string &algo,std::string & group_id,std::string &agent_id);
    const VideoData & get_video_data() {return video_data_ ;}
    const ThroughputData &get_throughput(){return throughput_;}
    const std::deque<int> & get_history_quality(){return history_quality_;}
    const BufferData & get_buffer_data(){return buffer_data_;}
    int  get_played_frames() {return played_frames_;}
    const std::deque<std::pair<double,double>> & get_reward() {return qoe_rebuf_;}
    int64_t get_buffer_level_ms();
    int64_t get_buffer_diff_ms();
protected:
    virtual void DoDispose (void);
    virtual void DoInitialize (void);
    virtual void StartApplication(void);
    virtual void StopApplication(void);
    virtual void OnRequestEvent()=0;
    virtual void FireTerminateSignal()=0;
    void Terminate();
    void RequestSegment();
    void ReceiveOnePacket(int size);
    void PostProcessingAfterPacket();
    void CheckBufferStatus();
    void OnPlayBackEvent();
    void CalculateOneQoE(Time pause_time);
    void OpenTrace(std::string &name);
    void LogPlayStatus();
    void LogQoEInfo();
    void LogAverageRate();
    int init_segments_=3;
    int frames_in_segment_=0;
    Time start_time_;
    Time stop_time_;
    struct VideoData video_data_;
    ThroughputData throughput_;
    BufferData buffer_data_;
    std::string algo_name_;
    std::unique_ptr<AdaptationAlgorithm> algorithm_;
    std::deque<int> history_quality_;
    std::deque<std::pair<double,double>> qoe_rebuf_;
    DashPlayerState player_state_=PLAYER_NOT_STARTED;
    
    int quality_=0;
    int index_=0;
    int segments_in_buffer_=0;
    int segment_counter_=0;
    uint64_t segment_bytes_=0;
    uint64_t recv_bytes_=0;
    uint64_t total_bytes_=0;
    
    int playback_index_=0;
    int played_frames_=0;
    
    Time first_packet_time_=Time(0);
    Time pause_start_=Time(0);
    Time pause_time_=Time(0);
    Time startup_time_=Time(0);
    
    EventId start_event_;
    EventId stop_event_;
    EventId player_timer_;
    EventId request_timer_;
    
    std::fstream f_play_;
    std::fstream f_reward_;
#if defined (PIERO_LOG_RATE)
    std::fstream f_rate_;
#endif
};
}

#include <unistd.h>
#include "ns3/simulator.h"
#include "piero_dash_base.h"
#include "piero_dash_algo.h"
#include "piero_rl_algo.h"
namespace ns3{
char root_folder[FILENAME_MAX]={0};
void piero_set_trace_root_folder(const char *name){
    int i=0;
    while(0!=name[i]){
        root_folder[i]=name[i];
        i++;
    }
    root_folder[i]=0;
}
PieroDashBase::PieroDashBase(std::vector<std::string> &video_log,std::vector<double> &average_rate,
                    std::string &trace_name,int segment_ms,int init_segments,
                    Time start,Time stop){
    init_segments_=init_segments;
    frames_in_segment_=segment_ms*kFramesPerSecond/1000;
    start_time_=start;
    stop_time_=stop;
    video_data_.segmentDuration=segment_ms;
    ReadSegmentFromFile(video_log,video_data_);
    if(average_rate.size()>0){
       video_data_.averageBitrate.clear(); 
       for(int i=0;i<average_rate.size();i++){
           double bps=average_rate.at(i);
           video_data_.averageBitrate.push_back(bps);
       }
    }
    algorithm_.reset(new FestiveAlgorithm(kFestiveTarget,kFestiveHorizon,segment_ms));
    OpenTrace(trace_name);
}
PieroDashBase::~PieroDashBase(){
    LogAverageRate();
    if(f_play_.is_open()){
        f_play_.close();
    }
    if(f_reward_.is_open()){
        f_reward_.close();
    }
#if defined (PIERO_LOG_RATE)
    if(f_rate_.is_open()){
        f_rate_.close();
    }
#endif
}
void PieroDashBase::SetAdaptationAlgorithm(std::string &algo,std::string & group_id,std::string &agent_id){
    if(algo.compare("panda")==0){
        algorithm_.reset(new PandaAlgorithm());
    }else if(algo.compare("tobasco")==0){
        algorithm_.reset(new TobascoAlgorithm());    
    }else if(algo.compare("osmp")==0){
        algorithm_.reset(new OsmpAlgorithm());
    }else if(algo.compare("raahs")==0){
        algorithm_.reset(new RaahsAlgorithm()); 
    }else if(algo.compare("fdash")==0){
        algorithm_.reset(new FdashAlgorithm());
    }else if(algo.compare("sftm")==0){
        algorithm_.reset(new SftmAlgorithm());
    }else if(algo.compare("svaa")==0){
        algorithm_.reset(new SvaaAlgorithm());
    }else if(algo.compare("reinforce")==0){
        int gid=std::stoi(group_id);
        int aid=std::stoi(agent_id);
        algorithm_.reset(new ReinforceAlgorithm(gid,aid));
    }
}
int64_t PieroDashBase::get_buffer_level_ms(){
    int64_t ms=0;
    if(!buffer_data_.bufferLevelNew.empty()){
        ms=buffer_data_.bufferLevelNew.back();
    }
    return ms;
}
int64_t PieroDashBase::get_buffer_diff_ms(){
    int64_t diff=0;
    if(buffer_data_.bufferLevelNew.size()>1){
        int64_t now=buffer_data_.bufferLevelNew.end()[-1];
        int64_t old=buffer_data_.bufferLevelNew.end()[-2];
        diff=now-old;
    }
    return diff;
}
void PieroDashBase::DoDispose (void){
    start_event_.Cancel();
    stop_event_.Cancel();
    Object::DoDispose();
}
void PieroDashBase::DoInitialize (void){
    start_event_=Simulator::Schedule(start_time_,&PieroDashBase::StartApplication,this);
    if(stop_time_!=Time(0)){
        stop_event_=Simulator::Schedule(stop_time_,&PieroDashBase::StopApplication,this);
    }
    Object::DoInitialize();
}
void PieroDashBase::StartApplication(){
    player_state_=PLAYER_INIT_BUFFERING;
    RequestSegment();
}
void PieroDashBase::StopApplication(){}

void PieroDashBase::RequestSegment(){
    Time now=Simulator::Now();
    AlgorithmReply reply=algorithm_->GetNextQuality(this,now,quality_,index_);
    quality_=reply.nextQuality;
    history_quality_.push_back(quality_);
    recv_bytes_=0;
    segment_bytes_=video_data_.representation[quality_][index_];
    if(!reply.terminate){
        if(reply.nextDownloadDelay!=Time(0)){
            request_timer_=Simulator::Schedule(reply.nextDownloadDelay,&PieroDashBase::OnRequestEvent,this);    
        }else{
            OnRequestEvent();
        }        
    }else{
        FireTerminateSignal();
    }
}
void PieroDashBase::ReceiveOnePacket(int size){
    Time now=Simulator::Now();
    if(first_packet_time_.IsZero()){
        first_packet_time_=now;
    }
    if(recv_bytes_==0){
        throughput_.transmissionStart.push_back(now);
    }
    uint64_t previous_bytes=recv_bytes_;
    recv_bytes_+=size;
    if(recv_bytes_==segment_bytes_){
        buffer_data_.timeNow.push_back(now);
        if(segment_counter_>0){
            int64_t ms=buffer_data_.bufferLevelNew.back ();
            if(player_state_==PLAYER_INIT_BUFFERING){
                
            }else{
                ms=ms-(now-throughput_.transmissionEnd.back()).GetMilliSeconds();
            }
            buffer_data_.bufferLevelOld.push_back(std::max(ms,(int64_t)0));
        }else{
            buffer_data_.bufferLevelOld.push_back(0);
        }
        buffer_data_.bufferLevelNew.push_back (buffer_data_.bufferLevelOld.back () + video_data_.segmentDuration);
        throughput_.transmissionEnd.push_back(now);
        throughput_.bytesReceived.push_back(previous_bytes);
        segment_counter_++;
        segments_in_buffer_++;
        Time pause=pause_time_;
        CheckBufferStatus();
        pause=pause_time_-pause;
        LogPlayStatus();
        CalculateOneQoE(pause);
    }
}
void PieroDashBase::PostProcessingAfterPacket(){
    if(segment_bytes_==recv_bytes_){
        total_bytes_+=segment_bytes_;
        NS_ASSERT(video_data_.representation.size()>0);
        int segment_num=video_data_.representation[0].size();
        if(index_==segment_num){
            LogQoEInfo();
            algorithm_->LastSegmentArrive(this);
            return;
        }
        RequestSegment();
    }
}
void PieroDashBase::OnPlayBackEvent(){
    if(player_timer_.IsRunning()) {return ;}
    Time now=Simulator::Now();
    if(segments_in_buffer_>0){
        played_frames_++;
        if(played_frames_%frames_in_segment_==0){
            segments_in_buffer_--;
            playback_index_++;
            LogPlayStatus();
        }
    }
    if(segments_in_buffer_==0){
        int segment_num=video_data_.representation[0].size();
        if(playback_index_>=segment_num){
            FireTerminateSignal();
            player_state_=PLAYER_DONE;
        }else{
            player_state_=PLAYER_PAUSED;
            pause_start_=now;
        }
    }
    if(segments_in_buffer_>0){
        int ms=1000/kFramesPerSecond;
        Time next=MilliSeconds(ms);
        player_timer_=Simulator::Schedule(next,&PieroDashBase::OnPlayBackEvent,this);
    }
}
void PieroDashBase::CheckBufferStatus(){
    Time now=Simulator::Now();
    if((player_state_==PLAYER_INIT_BUFFERING)&&(segments_in_buffer_>=init_segments_)){
        player_state_=PLAYER_PLAY;
        startup_time_=now-start_time_;
        player_timer_=Simulator::ScheduleNow(&PieroDashBase::OnPlayBackEvent,this);
    }
    if((player_state_==PLAYER_PAUSED)&&(segments_in_buffer_>0)){
        player_state_=PLAYER_PLAY;
        if(pause_start_>Time(0)){
            pause_time_=pause_time_+(now-pause_start_);
        }
        pause_start_=Time(0);
        NS_ASSERT(player_timer_.IsExpired());
        player_timer_=Simulator::ScheduleNow(&PieroDashBase::OnPlayBackEvent,this);
    }
}
void PieroDashBase::CalculateOneQoE(Time pause_time){
    int n=history_quality_.size();
    double qoe=0.0;
    double rebuf=pause_time.GetMilliSeconds()*1.0/1000;
    auto a=video_data_.averageBitrate.at(history_quality_.at(n-1))/kMUnit;
    if(1==n){
        qoe=a-kRebufPenality*rebuf;
    }else{
        auto b=video_data_.averageBitrate.at(history_quality_.at(n-2))/kMUnit;
        qoe=a-std::abs(b-a)-kRebufPenality*rebuf;
    }
    qoe_rebuf_.push_back(std::make_pair(qoe,rebuf));
}
void PieroDashBase::OpenTrace(std::string &name){
    if(0==name.size()){
        return ;
    }
    char buf[FILENAME_MAX];
    memset(buf,0,FILENAME_MAX);
    std::string path=std::string (getcwd(buf, FILENAME_MAX)) + "/traces/";
    int len=strlen(root_folder);
    if(len>0){
        path=std::string(root_folder);
        if('/'!=root_folder[len-1]){
           path=path+"/";
        }
    }
    {
        std::string pathname =path+name+".txt";
        f_play_.open(pathname.c_str(),std::fstream::out);
    }
    {
        std::string pathname =path+name+"_r.txt";
        f_reward_.open(pathname.c_str(),std::fstream::out);
    }
#if defined (PIERO_LOG_RATE)
    {
        std::string pathname =path+name+"_bw.txt";
        f_rate_.open(pathname.c_str(),std::fstream::out);
    }
#endif
}
void PieroDashBase::LogPlayStatus(){
    if(f_play_.is_open()){
        Time now=Simulator::Now();
        int wall_time=(now-start_time_).GetMilliSeconds();
        f_play_<<wall_time<<"\t"<<segments_in_buffer_<<std::endl;
        f_play_.flush();
    }
}
void PieroDashBase::LogQoEInfo(){
    if(f_reward_.is_open()){
        int i=1;
        for(auto it=qoe_rebuf_.begin();it!=qoe_rebuf_.end();it++){
            double qoe=it->first;
            double rebuf=it->second;
            f_reward_<<i<<"\t"<<qoe<<"\t"<<rebuf<<std::endl;
            i++;
        }
    }
}
void PieroDashBase::LogAverageRate(){
    if(f_play_.is_open()){
        double average_kbps_1=0.0;
        double average_kbps_2=0.0;
        if(video_data_.representation.size()==0){
            return ;
        }
        int segment_num=video_data_.representation[0].size();
        average_kbps_1=1.0*total_bytes_*8/(segment_num*video_data_.segmentDuration);
        Time denominator=Time(0);
        int n=throughput_.transmissionEnd.size();
        for(int i=0;i<n;i++){
            denominator=denominator+(throughput_.transmissionEnd.at(i)-throughput_.transmissionStart.at(i));
        }
        if(denominator!=Time(0)){
            average_kbps_2=1.0*total_bytes_*8/(denominator.GetMilliSeconds());
        }
        f_play_<<"\t"<<average_kbps_1<<"\t"<<average_kbps_2<<std::endl;
    }
}
}

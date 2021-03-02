#include <unistd.h>
#include <iostream>
#include "ns3/core-module.h"
#include "ns3/log.h"
#include "piero_dash_client.h"
#include "piero_dash_algo.h"
#include "piero_rl_algo.h"
namespace ns3{
NS_LOG_COMPONENT_DEFINE("piero_dash_client");
const uint32_t kContextAny=0xffffffff;
const float kMUnit=1000000;
bool has_digit(std::string & line){
    bool ret=false;
    for(size_t i=0;i<line.size();i++){
        if(isdigit(line[i])){
            ret=true;
            break;
        }
    }
    return ret;
}
void ReadSegmentFromFile(std::vector<std::string>& video_log,struct VideoData &video_data){
    for(auto it=video_log.begin();it!=video_log.end();it++){
        const char *name=it->c_str();
        SegmentSize segments;
        std::ifstream file;
        file.open(name);
        if(!file.is_open()){
            std::cout<<"video file path is not right"<<std::endl;
            return;
        }
        int counter=0;
        int64_t sum_bytes=0;
        while(!file.eof()){
            std::string buffer;
            getline(file,buffer);
            if(has_digit(buffer)){
                int size=std::atoi(buffer.c_str());
                segments.push_back(size);
                counter++;
                sum_bytes+=size;
            }
        }
        double average_bitrate=0.0;
        int64_t ms=video_data.segmentDuration;
        if(ms>0&&counter>0){
            average_bitrate=sum_bytes*1000*8*1.0/(ms*counter);
        }
        file.close();
        video_data.representation.push_back(segments);
        video_data.averageBitrate.push_back(average_bitrate);
    }
}
PieroDashClient::PieroDashClient(std::vector<std::string> &video_log,std::string &trace_name,int segment_ms,int init_segments,Ptr<PieroSocket> socket,Time start){
    init_segments_=init_segments;
    frames_in_segment_=segment_ms*kFramesPerSecond/1000;
    video_data_.segmentDuration=segment_ms;
    ReadSegmentFromFile(video_log,video_data_);
    channel_=CreateObject<PieroTraceChannel>(socket,&broadcast_);
    channel_->SetRecvCallback(MakeCallback(&PieroDashClient::RecvPacket,this));
    OpenTrace(trace_name);
    algorithm_.reset(new FestiveAlgorithm(kFestiveTarget,kFestiveHorizon,segment_ms));
    Simulator::ScheduleWithContext(kContextAny,start,MakeEvent(&PieroDashClient::StartApplication,this));
}
PieroDashClient::~PieroDashClient(){
    LogQoEInfo();
    CloseTrace();
}
void PieroDashClient::RecvPacket(PieroTraceChannel *channel,PieroPacket *packet){
    OnReadEvent(packet->length);
    delete packet;
}
void PieroDashClient::SetAdaptationAlgorithm(std::string &algo,std::string & group_id,std::string &agent_id){
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
void PieroDashClient::StartApplication(){
    player_state_=PLAYER_INIT_BUFFERING;
    first_request_time_=Simulator::Now();
    RequestSegment();
}
int64_t PieroDashClient::get_buffer_level_ms(){
    int64_t ms=0;
    if(!buffer_data_.bufferLevelNew.empty()){
        ms=buffer_data_.bufferLevelNew.back();
    }
    return ms;
}
int64_t PieroDashClient::get_buffer_diff_ms(){
    int64_t diff=0;
    if(buffer_data_.bufferLevelNew.size()>1){
        int64_t now=buffer_data_.bufferLevelNew.end()[-1];
        int64_t old=buffer_data_.bufferLevelNew.end()[-2];
        diff=now-old;
    }
    return diff;
}
void PieroDashClient::OnReadEvent(int bytes){
    Time now=Simulator::Now();
    if(read_bytes_==0){
        throughput_.transmissionStart.push_back(now);
    }
    int previous=read_bytes_;
    read_bytes_+=bytes;
    if(read_bytes_==segment_bytes_){
        session_bytes_+=segment_bytes_;
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
        throughput_.bytesReceived.push_back(previous);
        segment_counter_++;
        segments_in_buffer_++;
        Time pause=pause_time_;
        CheckBufferStatus();
        pause=pause_time_-pause;
        LogPlayStatus();
        CalculateOneQoE(pause);
        if(video_data_.representation.size()>0){
            int segment_num=video_data_.representation[0].size();
            if(index_==segment_num){
                algorithm_->LastSegmentArrive(this);
                return ;
            }
        }
        RequestSegment();
    }
}
void PieroDashClient::RequestSegment(){
    Time now=Simulator::Now();
    AlgorithmReply reply=algorithm_->GetNextQuality(this,now,quality_,index_);
    quality_=reply.nextQuality;
    history_quality_.push_back(quality_);
    read_bytes_=0;
    segment_bytes_=video_data_.representation[quality_][index_];
    if(!reply.terminate){
        if(reply.nextDownloadDelay!=Time(0)){
            request_timer_=Simulator::Schedule(reply.nextDownloadDelay,&PieroDashClient::OnRequestEvent,this);    
        }else{
            OnRequestEvent();
        }        
    }else{
        HandleTerminateSignal();
    }
}
void PieroDashClient::HandleTerminateSignal(){
    request_timer_.Cancel();
    player_timer_.Cancel();
    broadcast_.Fire();
    player_state_=PLAYER_DONE;
}
void PieroDashClient::OnRequestEvent(){
    if(video_data_.representation.size()>0){
        int segment_num=video_data_.representation[0].size();
        Time now=Simulator::Now();
        if(index_<segment_num){
            throughput_.transmissionRequested.push_back(now);
            SendRequest(segment_bytes_);
            index_++;
        }
    }    
}
void PieroDashClient::SendRequest(int bytes){
    if(channel_){
        PieroDashRequest *request=new PieroDashRequest(bytes);
        channel_->SendMessage(request);
    }
}
void PieroDashClient::CheckBufferStatus(){
    Time now=Simulator::Now();
    if((player_state_==PLAYER_INIT_BUFFERING)&&(segments_in_buffer_>=init_segments_)){
        player_state_=PLAYER_PLAY;
        startup_time_=now-first_request_time_;
        player_timer_=Simulator::ScheduleNow(&PieroDashClient::OnPlayBackEvent,this);
    }
    if((player_state_==PLAYER_PAUSED)&&(segments_in_buffer_>0)){
        player_state_=PLAYER_PLAY;
        if(pause_start_>Time(0)){
            pause_time_=pause_time_+(now-pause_start_);
        }
        pause_start_=Time(0);
        NS_ASSERT(player_timer_.IsExpired());
        player_timer_=Simulator::ScheduleNow(&PieroDashClient::OnPlayBackEvent,this);
    }
}
void PieroDashClient::OnPlayBackEvent(){
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
            broadcast_.Fire();
            player_state_=PLAYER_DONE;
        }else{
            player_state_=PLAYER_PAUSED;
            pause_start_=now;
        }
    }
    if(segments_in_buffer_>0){
        int ms=1000/kFramesPerSecond;
        Time next=MilliSeconds(ms);
        player_timer_=Simulator::Schedule(next,&PieroDashClient::OnPlayBackEvent,this);
    }
}
void PieroDashClient::LogPlayStatus(){
    if(m_play_trace.is_open()){
        Time now=Simulator::Now();
        int wall_time=(now-first_request_time_).GetMilliSeconds();
        m_play_trace<<wall_time<<"\t"<<segments_in_buffer_<<"\t"<<quality_<<std::endl;
        m_play_trace.flush();        
    }
}
void PieroDashClient::LogQoEInfo(){
    if(m_reward_trace.is_open()){
        int i=1;
        for(auto it=qoe_rebuf_.begin();it!=qoe_rebuf_.end();it++){
            double qoe=it->first;
            double rebuf=it->second;
            m_reward_trace<<i<<"\t"<<qoe<<"\t"<<rebuf<<std::endl;
            i++;
        }
    }
    CalculateQoE();
}
void PieroDashClient::CalculateQoE(){
    double total_qoe=0.0;
    double average_kbps_1=0.0;
    double average_kbps_2=0.0;
    if(video_data_.representation.size()==0){
        return ;
    }
    int segment_num=video_data_.representation[0].size();
    average_kbps_1=1.0*session_bytes_*8/(segment_num*video_data_.segmentDuration);
    Time denominator=Time(0);
    int n=throughput_.transmissionEnd.size();
    for(int i=0;i<n;i++){
        denominator=denominator+(throughput_.transmissionEnd.at(i)-throughput_.transmissionStart.at(i));
    }
    if(denominator!=Time(0)){
        average_kbps_2=1.0*session_bytes_*8/(denominator.GetMilliSeconds());
    }
    double total_bitrate=0.0;
    double total_instability=0.0;
    n=history_quality_.size();
    for(int i=0;i<n;i++){
        double a=video_data_.averageBitrate.at(history_quality_.at(i));
        total_bitrate+=a;
        if(i<n-1){
            double b=video_data_.averageBitrate.at(history_quality_.at(i+1));
            total_instability+=std::abs(b-a);
        }
    }
    float rebuf=pause_time_.GetMilliSeconds()*1.0/1000;
    total_qoe=total_bitrate/kMUnit-total_instability/kMUnit-kRebufPenality*rebuf;
    if(m_play_trace.is_open()){
        Time now=Simulator::Now();
        int wall_time=(now-first_request_time_).GetMilliSeconds();
        m_play_trace<<wall_time<<"\t"<<average_kbps_1<<"\t"<<average_kbps_2<<"\t"<<rebuf<<"\t"<<total_qoe<<std::endl;        
    }
}
void PieroDashClient::CalculateOneQoE(Time pause_time){
    int n=history_quality_.size();
    double qoe=0.0;
    double rebuf=pause_time.GetMilliSeconds()*1.0/1000;
    auto a=video_data_.averageBitrate.at(history_quality_.at(n-1))/kMUnit;
    if(1==n){
        qoe=a/kMUnit-kRebufPenality*rebuf;
    }else{
        auto b=video_data_.averageBitrate.at(history_quality_.at(n-2))/kMUnit;
        qoe=a-std::abs(b-a)-kRebufPenality*rebuf;
    }
    qoe_rebuf_.push_back(std::make_pair(qoe,rebuf));
}
void PieroDashClient::OpenTrace(std::string &name){
    if(name.size()==0){
        return;
    }
    {
        std::string folder_file =name+".txt";
        m_play_trace.open(folder_file.c_str(), std::fstream::out);
    }
    {
        std::string folder_file =name+"_r.txt";
        m_reward_trace.open(folder_file.c_str(), std::fstream::out);        
    }

}
void PieroDashClient::CloseTrace(){
    if(m_play_trace.is_open()){
        m_play_trace.close();
    }
    if(m_reward_trace.is_open()){
        m_reward_trace.close();
    }
}
}

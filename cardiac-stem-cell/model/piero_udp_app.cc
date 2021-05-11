#include <string.h>
#include "ns3/log.h"
#include "piero_udp_app.h"
namespace ns3{
const char *piero_udp_app_name="piero-udp-app";
NS_LOG_COMPONENT_DEFINE(piero_udp_app_name);
PieroDashApp::PieroDashApp(std::vector<std::string> &video_log,std::vector<double> &average_rate,
                    std::string &trace_name,int segment_ms,int init_segments,
                    Time start,Time stop)
:PieroDashBase(video_log,average_rate,trace_name,segment_ms,init_segments,
                start,stop){}
PieroDashApp::~PieroDashApp(){}
bool PieroDashApp::RegisterChannel(Ptr<PieroClientChannel> channel){
    bool found=false;
    int sz=channels_.size();
    for(int i=0;i<sz;i++){
        if(PeekPointer(channel)==PeekPointer(channels_[i])){
            found=true;
            break;
        }
    }
    if(!found){
        channel->SetDownloadDoneCallback(MakeCallback(&PieroDashApp::DownloadDone,this));
        channel->SetRecvCallBack(MakeCallback(&PieroDashApp::RecvPacket,this));
        channels_.push_back(channel);
    }
    return !found;
}
void PieroDashApp::RecvPacket(Ptr<PieroClientChannel> channel,int size){
    ReceiveOnePacket(size);
}
void PieroDashApp::DownloadDone(Ptr<PieroClientChannel> channel,int size){
#if defined (PIERO_LOG_RATE)
    LogChannelRate(channel);
#endif
    PostProcessingAfterPacket();
}
void PieroDashApp::OnRequestEvent(){
    if(video_data_.representation.size()>0){
        int segment_num=video_data_.representation[0].size();
        Time now=Simulator::Now();
        if(index_<segment_num){
            throughput_.transmissionRequested.push_back(now);
            NS_ASSERT(channels_.size()>0);
            bool ret=channels_[0]->RequestChunk(index_,segment_bytes_);
            NS_ASSERT(ret);
            index_++;
        }
    }
}
void PieroDashApp::FireTerminateSignal(){
    request_timer_.Cancel();
    player_timer_.Cancel();
    player_state_=PLAYER_DONE;
    int sz=channels_.size();
    for(int i=0;i<sz;i++){
        channels_[i]->FireStopSignal();
    }
}
void PieroDashApp::LogChannelRate(Ptr<PieroClientChannel> channel){
#if defined (PIERO_LOG_RATE)
    if(f_rate_.is_open()){
        uintptr_t ptr=(uintptr_t)PeekPointer(channel);
        uint32_t bytes=0;
        Time t1,t2;
        channel->GetStats(bytes,t1,t2);
        double kbps=0.0;
        if(t2>t1){
            kbps=1.0*bytes*8/(t2-t1).GetMilliSeconds();
        }
        f_rate_<<index_<<"\t"<<ptr<<"\t"<<kbps<<std::endl;
    }
#endif
}
}

#include <time.h>
#include <cmath>
#include "piero_chunk_dispatch.h"
#include "piero_hunnan_chan.h"
#include "piero_dash_app.h"
#include "piero_misc.h"
#include "piero_param_config.h"
#include "ns3/log.h"
namespace ns3{
namespace{
    const int kMinNumPacket=2;
    float kEpsilon=0.3;
    int64_t kRandomStream=0;
}
void piero_set_epsilon(float v){
    kEpsilon=v;
}
void piero_set_random_stream(int64_t v){
    kRandomStream=v;
}
std::string DispatchTypeToString(uint8_t type){
    switch(type){
        case DEF_CHUNK_SPLIT:
            return "def_chunk_split";
        case DEF_CHUNK_UNSPLIT:
            return "def_chunk_unsplit";
        case EPS_CHUNK_SPLIT:
            return "eps_chunk_split";
        case EPS_CHUNK_UNSPLIT:
            return "eps_chunk_unsplit";
        case UCB_CHUNK_SPLIT:
            return "ucb_chunk_split";
        case UCB_CHUNK_UNSPLIT:
            return "ucb_chunk_unsplit";
    }
    return "def_chunk_unsplit";
}
PieroPathInfo::PieroPathInfo(){
    for(int i=0;i<kPieroPathXDim;i++){
        for(int j=0;j<kPieroPathYDim;j++){
            id_[i][j]=-1;
        }
    }
    for(int i=0;i<kPieroPathXDim;i++){
        default_index_[i]=-1;
        length_[i]=0;
    }
}
bool PieroPathInfo::AddId(int x,int id,bool is_default){
    bool added=false;
    if(x>=0&&x<kPieroPathXDim){
        int y=length_[x];
        id_[x][y]=id;
        if(is_default){
            default_index_[x]=y;
        }
        length_[x]++;
        added=true;
    }
    return added;
}
int PieroPathInfo::GetDefaultId(int x) const{
    int ret=-1;
    if(x>=0&&x<kPieroPathXDim){
        int y=default_index_[x];
        if(y>=0){
            ret=id_[x][y];
        }
    }
    return ret;
}
int PieroPathInfo::GetLength(int x) const{
    int len=-1;
    if(x>=0&&x<kPieroPathXDim){
        len=length_[x];
    }
    return len;
}
void ChunkDispatchInterface::RegisterPathInfo(PieroPathInfo& info){
    info_=info;
}
void ChunkDispatchInterface::DispatchRequest(int chunk_id,uint64_t bytes){
    if(CHUNK_SPLIT==mode_){
        DispatchWithSplit(chunk_id,bytes);
    }else{
        DispatchNoSplit(chunk_id,bytes);
    }
}
void ChunkDispatchInterface::SplitChunk(std::vector<int> & id_vec,std::vector<float> &ratio_vec,int chunk_id,uint64_t bytes){
    int sz=id_vec.size();
    int path_available=sz;
    uint64_t remain=bytes;
    if(0==bytes){
        return;
    }
    for(int i=0;i<sz;i++){
        int id=id_vec.at(i);
        float ratio=ratio_vec.at(i);
        if(1==path_available){
            uint64_t target=remain;
            source_->ChunkDispatch(id,chunk_id,target);
            remain=0;
        }else{
            double a=1.0*bytes*ratio;
            uint64_t target=a;
            if(target<kMinNumPacket*kPieroPacketSize){
                target=kMinNumPacket*kPieroPacketSize;
            }
            target=std::min(target,remain);
            source_->ChunkDispatch(id,chunk_id,target);
            remain-=target;                
        }
        path_available--;
        if(0==remain){
            break;
        }
    }
    NS_ASSERT(0==remain);
}
DefaultChunkDispatch::DefaultChunkDispatch(DashChunkSelector *source,Mode mode){
    source_=source;
    mode_=mode;
}
DispatchType DefaultChunkDispatch::GetType(){
    if(CHUNK_SPLIT==mode_){
        return DEF_CHUNK_SPLIT;
    }else{
        return DEF_CHUNK_UNSPLIT;
    }
}
void DefaultChunkDispatch::DispatchWithSplit(int chunk_id,uint64_t bytes){
    std::vector<int> default_chan_id;
    for(int i=0;i<kPieroPathXDim;i++){
        int id=info_.GetDefaultId(i);
        if(id>=0){
            default_chan_id.push_back(id);
        }
    }
    int sz=default_chan_id.size();
    NS_ASSERT(sz>0);
    if(1==sz){
        int id=default_chan_id.at(0);
        Ptr<HunnanClientChannel> channel=source_->GetChannel(id);
        source_->ChunkDispatch(id,chunk_id,bytes);
        return ;
    }
    uint64_t sum_bps=0;
    const VideoData & video_data=source_->get_video_data();
    DataRate min_rate(video_data.averageBitrate[0]);
    int path_available=0;
    std::vector<int> id_vec;
    std::vector<DataRate> rate_vec;
    std::vector<float> ratio_vec;
    for(int i=0;i<sz;i++){
        int id=default_chan_id.at(i);
        DataRate rate(0);
        if(id>=0){
            Ptr<HunnanClientChannel> channel=source_->GetChannel(id);
            rate=channel->GetAverageBandwidth();
            if(rate<min_rate){
                rate=min_rate;
            }
            id_vec.push_back(id);
            rate_vec.push_back(rate);
            sum_bps+=rate.GetBitRate();
        }
    }
    sz=id_vec.size();
    for(int k=0;k<sz;k++){
        float ratio=1.0*rate_vec.at(k).GetBitRate()/sum_bps;
        ratio_vec.push_back(ratio);
    }
    SplitChunk(id_vec,ratio_vec,chunk_id,bytes);
}
void DefaultChunkDispatch::DispatchNoSplit(int chunk_id,uint64_t bytes){
    std::vector<int> default_chan_id;
    for(int i=0;i<kPieroPathXDim;i++){
        int id=info_.GetDefaultId(i);
        if(id>=0){
            default_chan_id.push_back(id);
        }
    }
    int sz=default_chan_id.size();
    bool send=false;
    for(int i=0;i<sz;i++){
        int id=default_chan_id.at(i);
        if(id>=0){
            source_->ChunkDispatch(id,chunk_id,bytes);
            send=true;
            break;
        }
    }
    NS_ASSERT(send);
}
EpsilonChunkDispatch::EpsilonChunkDispatch(DashChunkSelector *source,Mode mode){
    source_=source;
    mode_=mode;
    epsilon_=kEpsilon;
    random_=CreateObject<UniformRandomVariable>();
    if(0==kRandomStream){
        random_->SetStream(time(NULL));
    }else{
        random_->SetStream(kRandomStream);
    }
}
DispatchType EpsilonChunkDispatch::GetType(){
    if(CHUNK_SPLIT==mode_){
        return EPS_CHUNK_SPLIT;
    }else{
        return EPS_CHUNK_UNSPLIT;
    }
}
void EpsilonChunkDispatch::SetRandomStream(int64_t stream){
    random_->SetStream(stream);
}
void EpsilonChunkDispatch::SetEpsilon(float v){
    epsilon_=v;
}
void EpsilonChunkDispatch::DispatchWithSplit(int chunk_id,uint64_t bytes){
    std::vector<int> id_vec;
    std::vector<float> ratio_vec;
    std::vector<DataRate> rate_vec;
    double sum_bps=0.0;
    const VideoData & video_data=source_->get_video_data();
    DataRate min_rate(video_data.averageBitrate[0]);
    int concurrency=0;
    for(int i=0;i<kPieroPathXDim;i++){
        int channel_num=info_.GetLength(i);
        if(channel_num<=0){
            break;
        }
        concurrency++;
    }
    for(int i=0;i<concurrency;i++){
        int channel_num=info_.GetLength(i);
        bool explore=false;
        if(random_->GetValue(0.0,1.0)<epsilon_){
            explore=true;
        }
        if(explore||0==times_){
            int which=random_->GetInteger()%channel_num;
            int id=info_.id_[i][which];
            NS_ASSERT(id>=0);
            Ptr<HunnanClientChannel> channel=source_->GetChannel(id);
            DataRate rate=channel->GetAverageBandwidth();
            if(rate<min_rate){
                rate=min_rate;
            }
            id_vec.push_back(id);
            rate_vec.push_back(rate);
            sum_bps+=rate.GetBitRate();
        }else{
            DataRate max_rate(0);
            int chan_id=-1;
            for(int j=0;j<channel_num;j++){
                int id=info_.id_[i][j];
                NS_ASSERT(id>=0);
                Ptr<HunnanClientChannel> channel=source_->GetChannel(id);
                DataRate rate=channel->GetAverageBandwidth();
                if(rate>max_rate){
                    max_rate=rate;
                    chan_id=id;
                }
            }
            NS_ASSERT(chan_id>=0);
            if(max_rate<min_rate){
                max_rate=min_rate;
            }
            id_vec.push_back(chan_id);
            rate_vec.push_back(max_rate);
            sum_bps+=max_rate.GetBitRate();
        }
    }
    int sz=rate_vec.size();
    for(int k=0;k<sz;k++){
        float ratio=1.0*rate_vec.at(k).GetBitRate()/sum_bps;
        ratio_vec.push_back(ratio);
    }
    SplitChunk(id_vec,ratio_vec,chunk_id,bytes);
    times_++;
}
void EpsilonChunkDispatch::DispatchNoSplit(int chunk_id,uint64_t bytes){
    int channel_num=info_.GetLength(0);
    NS_ASSERT(channel_num>0);
    bool explore=false;
    if(random_->GetValue(0.0,1.0)<epsilon_){
        explore=true;
    }
    if(explore||0==times_){
        //exploration
        int which=random_->GetInteger()%channel_num;
        int id=info_.id_[0][which];
        NS_ASSERT(id>=0);
        source_->ChunkDispatch(id,chunk_id,bytes);
    }else{
        //exploitation
        DataRate max_rate(0);
        int chan_id=-1;
        for(int j=0;j<channel_num;j++){
            int id=info_.id_[0][j];
            if(id>=0){
                Ptr<HunnanClientChannel> channel=source_->GetChannel(id);
                DataRate rate=channel->GetAverageBandwidth();
                if(rate>max_rate){
                    max_rate=rate;
                    chan_id=id;
                }
            }
        }
        NS_ASSERT(chan_id>=0);
        source_->ChunkDispatch(chan_id,chunk_id,bytes);
    }
    times_++;
}

UcbChunkDispatch::UcbChunkDispatch(DashChunkSelector *source,Mode mode){
    source_=source;
    mode_=mode;
    random_=CreateObject<UniformRandomVariable>();
    if(0==kRandomStream){
        random_->SetStream(time(NULL));
    }else{
        random_->SetStream(kRandomStream);
    }
    for(int i=0;i<kPieroPathXDim;i++){
        for(int j=0;j<kPieroPathYDim;j++){
            pull_count_[i][j]=0;
        }
    }
}
DispatchType UcbChunkDispatch::GetType(){
    if(CHUNK_SPLIT==mode_){
        return UCB_CHUNK_SPLIT;
    }else{
        return UCB_CHUNK_UNSPLIT;
    }
}
void UcbChunkDispatch::SetRandomStream(int64_t stream){
    random_->SetStream(stream);
}
void UcbChunkDispatch::DispatchWithSplit(int chunk_id,uint64_t bytes){
    const VideoData & video_data=source_->get_video_data();
    DataRate min_rate(video_data.averageBitrate[0]);
    std::vector<int> id_vec;
    std::vector<float> ratio_vec;
    std::vector<DataRate> rate_vec;
    double sum_bps=0.0;
    bool send=false;
    int concurrency=0;
    for(int i=0;i<kPieroPathXDim;i++){
        int channel_num=info_.GetLength(i);
        if(channel_num<=0){
            break;
        }
        concurrency++;
    }
    if(times_==0){
        for(int i=0;i<concurrency;i++){
            int channel_num=info_.GetLength(i);
            int which=random_->GetInteger()%channel_num;
            int id=info_.id_[i][which];
            NS_ASSERT(id>=0);
            Ptr<HunnanClientChannel> channel=source_->GetChannel(id);
            DataRate rate=channel->GetAverageBandwidth();
            if(rate<min_rate){
                rate=min_rate;
            }
            pull_count_[i][which]++;
            id_vec.push_back(id);
            rate_vec.push_back(rate);
            sum_bps+=rate.GetBitRate();
        }
        int sz=id_vec.size();
        NS_ASSERT(sz>0);
        for(int i=0;i<sz;i++){
            DataRate rate=rate_vec.at(i);
            float ratio=1.0*rate.GetBitRate();
            ratio_vec.push_back(ratio);
        }
        SplitChunk(id_vec,ratio_vec,chunk_id,bytes);
        send=true;
    }else{
        for(int i=0;i<concurrency;i++){
            double best_performance=0.0;
            int best_id_index=-1;
            int best_id=-1;
            int channel_num=info_.GetLength(i);
            for(int j=0;j<channel_num;j++){
                int N=pull_count_[i][j];
                int id=info_.id_[i][j];
                NS_ASSERT(id>=0);
                if(0==N){
                    best_id=id;
                    best_id_index=j;
                    break;
                }else{
                    Ptr<HunnanClientChannel> channel=source_->GetChannel(id);
                    DataRate avg=channel->GetAverageBandwidth();
                    DataRate window_rate=channel->GetMaxBandwidth();
                    if(avg<min_rate){
                        avg=min_rate;
                    }
                    if(window_rate<min_rate){
                        window_rate=min_rate;
                    }
                    double x=1.0*avg.GetBitRate()+1.0*sqrt(1.0*concurrency*log(1.0*times_)/(1.0*N));
                    if(x>best_performance){
                        best_performance=x;
                        best_id=id;
                        best_id_index=j;
                    }
                }
            }
            NS_ASSERT(best_id>=0);
            pull_count_[i][best_id_index]++;
            Ptr<HunnanClientChannel> channel=source_->GetChannel(best_id);
            DataRate avg=channel->GetAverageBandwidth();
            if(avg<min_rate){
                avg=min_rate;
            }
            id_vec.push_back(best_id);
            rate_vec.push_back(avg);
            sum_bps+=avg.GetBitRate();
        }
        for(int k=0;k<(int)rate_vec.size();k++){
            float ratio=1.0*rate_vec.at(k).GetBitRate()/sum_bps;
            ratio_vec.push_back(ratio);
        }
        SplitChunk(id_vec,ratio_vec,chunk_id,bytes);
        send=true;
    }
    times_++;
    NS_ASSERT(send);
}
void UcbChunkDispatch::DispatchNoSplit(int chunk_id,uint64_t bytes){
    const VideoData & video_data=source_->get_video_data();
    DataRate min_rate(video_data.averageBitrate[0]);
    int channel_num=info_.GetLength(0);
    NS_ASSERT(channel_num>0);
    bool send=false;
    if(times_==0){
        int which=random_->GetInteger()%channel_num;
        int id=info_.id_[0][which];
        NS_ASSERT(id>=0);
        pull_count_[0][which]++;
        source_->ChunkDispatch(id,chunk_id,bytes);
        send=true;
    }else{
        int N[kPieroPathYDim];
        int zero_v_index=-1;
        for(int j=0;j<channel_num;j++){
            N[j]=pull_count_[0][j];
            if(0==N[j]){
                zero_v_index=j;
            }
        }
        if(zero_v_index>=0){
            int id=info_.id_[0][zero_v_index];
            NS_ASSERT(id>=0);
            pull_count_[0][zero_v_index]++;
            send=true;
            source_->ChunkDispatch(id,chunk_id,bytes);
        }else{
            double best_performance=0.0;
            int best_id_index=-1;
            int best_id=-1;
            for(int j=0;j<channel_num;j++){
                int id=info_.id_[0][j];
                NS_ASSERT(id>=0);
                Ptr<HunnanClientChannel> channel=source_->GetChannel(id);
                DataRate avg=channel->GetAverageBandwidth();
                DataRate window_rate=channel->GetMaxBandwidth();
                if(avg<min_rate){
                    avg=min_rate;
                }
                if(window_rate<min_rate){
                    window_rate=min_rate;
                }
                double x=1.0*avg.GetBitRate()+1.0*sqrt(log(1.0*times_)/(1.0*N[j]));
                if(x>best_performance){
                    best_performance=x;
                    best_id=id;
                    best_id_index=j;
                }
            }
            NS_ASSERT(best_id>=0);
            pull_count_[0][best_id_index]++;
            send=true;
            source_->ChunkDispatch(best_id,chunk_id,bytes);
        }
    }
    times_++;
    NS_ASSERT(send);
}
}

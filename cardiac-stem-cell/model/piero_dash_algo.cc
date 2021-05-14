#include "piero_dash_algo.h"
#include "piero_dash_client.h"
#include "ns3/log.h"
namespace ns3{
NS_LOG_COMPONENT_DEFINE("piero_dash_algo");
namespace{
    const double kBandwidhtScale=0.8;
    const double kFestiveAlpha=12;
    const int kSwitchUpThreshold[]= {0,1,2,3,4,5,6,7,8,9};
}
//refer from https://github.com/hongzimao/pensieve/blob/master/dash.js/src/streaming/controllers/AbrController.js#L291
double BandwidthPredicator::Predict(const ThroughputData &history,int horizon){
    double throughput=0.0;
    int length=history.transmissionStart.size();
    if(length>0){
        int index=std::max(0,length-horizon);
        int64_t sum_download_time=0;
        double temp_sum=0.0;
        for(int i=index;i<length;i++){
            Time start=history.transmissionStart.at(i);
            Time end=history.transmissionEnd.at(i);
            int64_t bytes=history.bytesReceived.at(i);
            NS_ASSERT(end>start);
            int64_t download_time=(end-start).GetMilliSeconds();
            sum_download_time+=download_time;
            temp_sum=temp_sum+1.0*download_time*download_time/(8*bytes);
        }
        throughput=1.0*1000*sum_download_time/temp_sum;
    }
    return throughput;
}
FestiveAlgorithm::FestiveAlgorithm(int64_t target,int horizon,int delta):
target_buffer_(target),horizon_(horizon),delta_(delta){
    random_=CreateObject<UniformRandomVariable>();
    random_->SetStream(12321);
    bandwidth_predicator_.reset(new BandwidthPredicator());
}
AlgorithmReply FestiveAlgorithm::GetNextQuality(PieroDashBase *client,Time now,int pre_quality,int segment_count){
    const VideoData & video_data=client->get_video_data();
    const ThroughputData &throughput=client->get_throughput();
    const  std::deque<int> &history_quality=client->get_history_quality();
    int64_t buffer_level_ms=client->get_buffer_level_ms();
    AlgorithmReply reply;
    if(segment_count==0){
        return reply;
    }
    int b_ref=0;
    int b_target=0;
    int b_cur=pre_quality;
    double predict_bandwith=bandwidth_predicator_->Predict(throughput,kThroughputHorizon);
    double temp_bitrate=kBandwidhtScale*predict_bandwith;
    const std::vector<double> &bitrate_array=video_data.averageBitrate;
    int top_quality=bitrate_array.size()-1;
    int i=top_quality;
    for(;i>=0;i--){
        if(bitrate_array.at(i)<=temp_bitrate){
            b_target=i;
            break;
        }
        b_target=i;
    }
    if(b_target>b_cur){
        switch_up_count_=switch_up_count_+1;
        if(switch_up_count_>kSwitchUpThreshold[b_cur]){
            b_ref=b_cur+1;
        }else{
            b_ref=b_cur;
        }
    }else if(b_target<b_cur){
        b_ref=b_cur-1;
        switch_up_count_=0;
    }else{
        b_ref = b_cur;
        switch_up_count_=0;
    }
    if(b_ref>top_quality){
        b_ref=top_quality;
    }
    if(b_ref<0){
        b_ref=0;
    }
    if(b_ref!=b_cur){
        double score_cur=CalculateCombineScore(b_cur,b_ref,b_cur,predict_bandwith,history_quality,bitrate_array);
        double socre_ref=CalculateCombineScore(b_ref,b_ref,b_cur,predict_bandwith,history_quality,bitrate_array);
        if(score_cur<=socre_ref){
            reply.nextQuality=b_cur;
        }else{
            reply.nextQuality=b_ref;
            if(b_ref>b_cur){
                switch_up_count_=0;
            }
        }
    }else{
        reply.nextQuality=b_cur;
    }
    int64_t lowerBound = target_buffer_ - delta_;
    int64_t upperBound = target_buffer_ + delta_;
    int64_t randBuf=lowerBound+random_->GetInteger(0,2*delta_);
    if(buffer_level_ms>randBuf){
        reply.nextDownloadDelay=MilliSeconds(buffer_level_ms-randBuf);
    }
    return reply;
}
double FestiveAlgorithm::CalculateCombineScore(int b,int b_ref,int b_cur,double predict_bandwith,
                                const std::deque<int> &history_quality,const std::vector<double> &bitrate_array){
    double total_score=0.0;
    double stability_score=CalculateStabilityScore(b,b_cur,history_quality);
    double efficiency_score=CalculateEfficiencyScore(b,b_ref,predict_bandwith,bitrate_array);
    total_score=stability_score+kFestiveAlpha*efficiency_score;
    return total_score;
}
double FestiveAlgorithm::CalculateStabilityScore(int b,int b_cur,const std::deque<int> &history_quality){
    double score=0;
    int length=history_quality.size();
    int n=0;
    if(length>0){
        int index=std::max(0,length-horizon_);
        for(int i=index;i<length-1;i++){
            if(history_quality.at(i)!=history_quality.at(i+1)){
                n=n+1;
            }
        }
    }
    if(b!=b_cur){
        n = n + 1;
    }
    score=::pow(2,n);
    return score;
}
//https://github.com/hongzimao/pensieve/blob/master/dash.js/src/streaming/controllers/AbrController.js#L217
double FestiveAlgorithm::CalculateEfficiencyScore(int b,int b_ref, double predict_bandwith,const std::vector<double> &bitrate_array){
    double score=0;
    double denominator=std::min(predict_bandwith,bitrate_array[b_ref]);
    double ratio=bitrate_array[b]/denominator;
    score=std::abs(ratio-1);
    return score;
}
PandaAlgorithm::PandaAlgorithm():
m_kappa (0.28),
m_omega (0.3),
m_alpha (0.2),
m_beta (0.2),
m_epsilon (0.15),
m_bMin (26){}
AlgorithmReply PandaAlgorithm::GetNextQuality(PieroDashBase *client,Time now,int pre_quality,int segment_count){
    const VideoData & video_data=client->get_video_data();
    const ThroughputData &throughput=client->get_throughput();
    //const  std::deque<int> &history_quality=client->get_history_quality();
    int64_t buffer_level_ms=client->get_buffer_level_ms();
    AlgorithmReply reply;
    if(segment_count==0){
        m_lastVideoIndex = 0;
        m_lastBuffer = (double)video_data.segmentDuration*1.0/1000;
        m_lastTargetInterrequestTime=Time(0);
        return reply;
    }
    Time start=throughput.transmissionStart.back ();
    Time end=throughput.transmissionEnd.back ();
    double throughputMeasured=((double)1.0*video_data.averageBitrate.at(m_lastVideoIndex)*video_data.segmentDuration)/
                                (double((end-start).GetMilliSeconds()));
    throughputMeasured=throughputMeasured/1e6; //in Mbps                          
    if(segment_count==1){
        m_lastBandwidthShare = throughputMeasured;
        m_lastSmoothBandwidthShare = m_lastBandwidthShare;        
    }
    double actualInterrequestTime=0.0;
    if(now-throughput.transmissionRequested.back ()>m_lastTargetInterrequestTime){
        actualInterrequestTime=1.0*((now-throughput.transmissionRequested.back ()).GetMilliSeconds())/1000;
    }else{
        actualInterrequestTime=1.0*m_lastTargetInterrequestTime.GetMilliSeconds()/1000;
    }
    double bandwidthShare = (m_kappa * (m_omega - std::max (0.0, m_lastBandwidthShare - throughputMeasured + m_omega)))
    * actualInterrequestTime + m_lastBandwidthShare;   
    if (bandwidthShare < 0)
    {
        bandwidthShare = 0;
    }
    m_lastBandwidthShare = bandwidthShare;

    double smoothBandwidthShare;
    smoothBandwidthShare = ((-m_alpha
                            * (m_lastSmoothBandwidthShare - bandwidthShare))
                            * actualInterrequestTime)+ m_lastSmoothBandwidthShare;
    
    m_lastSmoothBandwidthShare = smoothBandwidthShare;
    
    double deltaUp = m_omega + m_epsilon * smoothBandwidthShare;
    double deltaDown = m_omega;
    
    int rUp = FindLargest(video_data,smoothBandwidthShare,deltaUp);
    int rDown = FindLargest(video_data,smoothBandwidthShare,deltaDown);


    int videoIndex;
    if ((video_data.averageBitrate.at (m_lastVideoIndex))
        < (video_data.averageBitrate.at (rUp)))
    {
        videoIndex = rUp;
    }
    else if ((video_data.averageBitrate.at (rUp))
            <= (video_data.averageBitrate.at (m_lastVideoIndex))
            && (video_data.averageBitrate.at (m_lastVideoIndex))
            <= (video_data.averageBitrate.at (rDown)))
    {
        videoIndex = m_lastVideoIndex;
    }
    else
    {
        videoIndex = rDown;
    }
    m_lastVideoIndex = videoIndex;

    double seconds=(video_data.averageBitrate.at(videoIndex)/1e6)*(1.0*video_data.segmentDuration/1000)/smoothBandwidthShare;
    double targetInterrequestTime = std::max (0.0, (seconds+ m_beta * (m_lastBuffer - m_bMin)));
    Time delay=Time(0);
    if (throughput.transmissionEnd.back () - throughput.transmissionRequested.back () < m_lastTargetInterrequestTime)
    {
        delay =m_lastTargetInterrequestTime - (throughput.transmissionEnd.back () - throughput.transmissionRequested.back ());
    }
    
    m_lastTargetInterrequestTime =MilliSeconds(targetInterrequestTime*1000);    
    m_lastBuffer =(double)buffer_level_ms*1.0/1000;
    reply.nextQuality=videoIndex;
    reply.nextDownloadDelay=delay;
    return reply;
}
int PandaAlgorithm::FindLargest (const VideoData & video_data,const double smooth_bw, const double delta){
    int largestBitrateIndex = 0;
    int top_quality=video_data.averageBitrate.size()-1;
    for (int i = 0; i <top_quality; i++)
    {
        double currentBitrate =  video_data.averageBitrate.at (i) / 1e6;
        if (currentBitrate <= (smooth_bw - delta))
        {
            largestBitrateIndex = i;
        }
    }
    return largestBitrateIndex;    
}
TobascoAlgorithm::TobascoAlgorithm(){
    m_running_fast_start = true;
}
AlgorithmReply TobascoAlgorithm::GetNextQuality(PieroDashBase *client,Time now,int pre_quality,int segment_count){
    AlgorithmReply reply;
    if(segment_count==0){
        return reply;
    }
    double a1 = 0.75;
    double a2 = 0.33;
    double a3 = 0.5;
    double a4 = 0.75;
    double a5 = 0.9;
    int64_t b_min=10000000;
    int64_t b_low=20000000;
    int64_t b_high=40000000;
    int64_t b_opt=(int64_t(0.5*(b_low + b_high)));
    const VideoData & video_data=client->get_video_data();
    const ThroughputData &throughput=client->get_throughput();
    //const  std::deque<int> &history_quality=client->get_history_quality();
    const BufferData &buffer_data=client->get_buffer_data();
    int64_t b_t=client->get_buffer_level_ms()*1000;
    int rates_size=video_data.averageBitrate.size();
    double bitrate_est=predicator_.Predict(throughput,5);
    int64_t delay=0;
    int64_t taf=video_data.segmentDuration*1000;
    int cur_quality=pre_quality;
    int next_quality=pre_quality;
    double curr_rate=video_data.averageBitrate[cur_quality];
    double r_up_index=cur_quality;
    double r_down_index=cur_quality;
    if(cur_quality+1<rates_size){
        r_up_index=cur_quality+1;
    }
    if(cur_quality>=1){
        r_down_index=cur_quality-1;
    }
    
    if (m_running_fast_start && (cur_quality != rates_size - 1) &&BufferInc(buffer_data) &&
        (curr_rate <= a1 * bitrate_est)){
        if (b_t < b_min)
        {
            if (video_data.averageBitrate[r_up_index]<= a2 * bitrate_est)
            {
                next_quality = r_up_index;
            }
        }else if (b_t < b_low){
            if (video_data.averageBitrate[r_up_index]<= a3 * bitrate_est){
                next_quality = r_up_index;
            }
        }else{
            if (video_data.averageBitrate[r_up_index] <= a4 * bitrate_est){
                next_quality = r_up_index;
            }
            if (b_t > b_high)
            {
                delay = b_high - taf;
            }
        }
    }else{
        m_running_fast_start = false;
        if (b_t < b_min)
        {
            next_quality=0;
        }
        else if (b_t < b_low)
        {
            if (cur_quality != 0 && curr_rate >= bitrate_est)
            {
                next_quality = r_down_index;
            }
        }
        else if (b_t < b_high)
        {
            if ((cur_quality ==rates_size-1) || (video_data.averageBitrate[r_up_index]>= a5 * bitrate_est))
            {
                delay = std::max (b_t - taf, b_opt);
            }
        }else
        {
            if ((cur_quality ==rates_size - 1) || (video_data.averageBitrate[r_up_index]>= a5 * bitrate_est))
            {
                delay = std::max (b_t - taf, b_opt);
            }
            else
            {
                next_quality = r_up_index;
            }
        }
    }    
    reply.nextQuality =next_quality;
    reply.nextDownloadDelay =MicroSeconds(delay);
    return reply;
}
bool TobascoAlgorithm::BufferInc(const BufferData&buffer_data){
    int64_t last=0;
    for(auto it=buffer_data.bufferLevelNew.begin();it!=buffer_data.bufferLevelNew.end();it++){
        int64_t buffer_level=(*it);
        if(buffer_level<last){
            return false;
        }
        last=buffer_level;
    }
    return true;
}
AlgorithmReply OsmpAlgorithm::GetNextQuality(PieroDashBase *client,Time now,int pre_quality,int segment_count){
    const VideoData & video_data=client->get_video_data();
    const ThroughputData &throughput=client->get_throughput();
    //const  std::deque<int> &history_quality=client->get_history_quality(); 
    AlgorithmReply reply;
    if(segment_count==0){
        return reply;
    }
    if(throughput.transmissionEnd.size()>0){
        Time start=throughput.transmissionStart.back ();
        Time end=throughput.transmissionEnd.back ();
        double t_last_frag=1.0*(end-start).GetMilliSeconds();
        int l_cur=pre_quality;
        int l_nxt = 0; // The next quality level
        int l_min = 0; // The lowest quality level
        int l_max =video_data.averageBitrate.size()-1; // The highest quality level;
        double r_download =double(1.0*video_data.segmentDuration)/t_last_frag;        
        if (r_download < 1)
        {
            if (l_cur > l_min)
            {
                if (r_download < ((1.0 * video_data.averageBitrate[l_cur - 1]) /video_data.averageBitrate[l_cur]))
                {
                    l_nxt = l_min;
                }else{
                    l_nxt = l_cur - 1;
                }
            }
        }else{
            if (l_cur < l_max)
            {
                if (l_cur == 0 || r_download > ((1.0 * video_data.averageBitrate[l_cur - 1]) / video_data.averageBitrate[l_cur]))
                {
                    do
                    {
                        l_nxt = l_nxt + 1;
                    }
                    while (!(l_nxt == l_max || r_download < ((1.0 * video_data.averageBitrate[l_nxt + 1]) / video_data.averageBitrate[l_cur])));
                }
            }
        }
        reply.nextQuality=l_nxt;
    }
    return reply;
}
RaahsAlgorithm::RaahsAlgorithm():t_min_ms_(9000){}
AlgorithmReply RaahsAlgorithm::GetNextQuality(PieroDashBase *client,Time now,int pre_quality,int segment_count){
    const VideoData & video_data=client->get_video_data();
    const ThroughputData &throughput=client->get_throughput();
    //const  std::deque<int> &history_quality=client->get_history_quality();
    int64_t buffer_level_ms=client->get_buffer_level_ms();    
    AlgorithmReply reply;
    if(segment_count==0){
        return reply;
    }
    if(throughput.transmissionEnd.size()>0){
        Time start=throughput.transmissionStart.back ();
        Time end=throughput.transmissionEnd.back ();
        double sft=1.0*(end-start).GetMilliSeconds();
        double mi  =double(1.0*video_data.segmentDuration)/sft;  
        double epsilon = 0.0;
        int i;
        int rates_size=video_data.averageBitrate.size();
        for(i = 0; i < rates_size - 1; i++){
            epsilon=std::max (epsilon, (video_data.averageBitrate[i + 1] - video_data.averageBitrate[i])/video_data.averageBitrate[i]);
        }
        int cur_quality=pre_quality;
        int rateInd=pre_quality;
        int next_quality=pre_quality;
        if (mi > 1 + epsilon) // Switch Up
        {
            if (rateInd < rates_size - 1)
            {
                next_quality =rateInd+1;
            }
        }else if (mi < gamma_d_) // Switch down
        {
            bool found=false;
            for (i =cur_quality-1; i>=0; i--)
            {
                if (video_data.averageBitrate[i] < mi * video_data.averageBitrate[cur_quality])
                {
                    next_quality =i;
                    found=true;
                    break;
                }
            }
            if(!found){
                next_quality=0;
            }
        }
        double temp=video_data.averageBitrate[cur_quality]*video_data.segmentDuration/video_data.averageBitrate[0];
        int64_t delay=buffer_level_ms-t_min_ms_-(int)temp;
        if(delay>0){           
            reply.nextDownloadDelay = MilliSeconds(delay);            
        }
        reply.nextQuality =next_quality;
    }
    return reply;
}
AlgorithmReply FdashAlgorithm::GetNextQuality(PieroDashBase *client,Time now,int pre_quality,int segment_count){
    AlgorithmReply reply;
    if(segment_count==0){
        return reply;
    }
    double slow = 0, ok = 0, fast = 0, falling = 0, steady = 0, rising = 0, r1 = 0, r2 = 0, r3 = 0,
         r4 = 0, r5 = 0, r6 = 0, r7 = 0, r8 = 0, r9 = 0, p2 = 0, p1 = 0, z = 0, n1 = 0, n2 = 0,
         output = 0;
    double t =(double)target_buffer_/1000;
    double currDt=(double)client->get_buffer_level_ms()/1000;
    double diff =(double)client->get_buffer_diff_ms()/1000;
    if (currDt < 2 * t / 3)
    {
        slow = 1.0;
    }
    else if (currDt < t)
    {
        slow = 1 - 1 / (t / 3) * (currDt - 2 * t / 3);
        ok = 1 / (t / 3) * (currDt - 2 * t / 3);
    }
    else if (currDt < 4 * t)
    {
        ok = 1 - 1 / (3 * t) * (currDt - t);
        fast = 1 / (3 * t) * (currDt - t);
    }
    else
    {
        fast = 1;
    }
    
    if (diff < -2 * t / 3)
    {
        falling = 1;
    }
    else if (diff < 0)
    {
        falling = 1 - 1 / (2 * t / 3) * (diff + 2 * t / 3);
        steady = 1 / (2 * t / 3) * (diff + 2 * t / 3);
    }
    else if (diff < 4 * t)
    {
        steady = 1 - 1 / (4 * t) * diff;
        rising = 1 / (4 * t) * diff;
    }
    else
    {
        rising = 1;
    }

    r1 = std::min (slow, falling);
    r2 = std::min (ok, falling);
    r3 = std::min (fast, falling);
    r4 = std::min (slow, steady);
    r5 = std::min (ok, steady);
    r6 = std::min (fast, steady);
    r7 = std::min (slow, rising);
    r8 = std::min (ok, rising);
    r9 = std::min (fast, rising);
    
    p2 = std::sqrt (std::pow (r9, 2));
    p1 = std::sqrt (std::pow (r6, 2) + std::pow (r8, 2));
    z = std::sqrt (std::pow (r3, 2) + std::pow (r5, 2) + std::pow (r7, 2));
    n1 = std::sqrt (std::pow (r2, 2) + std::pow (r4, 2));
    n2 = std::sqrt (std::pow (r1, 2));
    
    /*output = (n2 * 0.25 + n1 * 0.5 + z * 1 + p1 * 1.4 + p2 * 2)*/
    output = (n2 * 0.25 + n1 * 0.5 + z * 1 + p1 * 2 + p2 * 4) / (n2 + n1 + z + p1 + p2);
    const ThroughputData &throughput=client->get_throughput();
    const VideoData & video_data=client->get_video_data();
    double bitrate_est=predicator_.Predict(throughput,5);
    double result = 0;
    result = output *bitrate_est;
    int rates_size=video_data.averageBitrate.size();
    int next_quality=pre_quality;
    for (int i = 0; i <rates_size;i++)
    {
        if (result > video_data.averageBitrate[i])
        {
            next_quality =i;
        }
    }
    int64_t delay =0;
    if (video_data.averageBitrate[next_quality] > video_data.averageBitrate[pre_quality]){
        double t_60 = currDt + (bitrate_est/video_data.averageBitrate[next_quality] - 1) * 60;
        if (t_60 < t)
        {
            next_quality =pre_quality;
            t_60 = currDt + (bitrate_est / video_data.averageBitrate[next_quality] - 1) * 60;
            if (t_60 > t)
            {
                // delay = Seconds(t_60 - t);
            }
        }        
    }else if(video_data.averageBitrate[next_quality]<video_data.averageBitrate[pre_quality]){
        double t_60 = currDt + (bitrate_est / video_data.averageBitrate[next_quality] - 1) * 60;
        if (t_60 > t)
        {
            t_60 = currDt + (bitrate_est /video_data.averageBitrate[next_quality] - 1) * 60;
            if (t_60 > t)
            {
                next_quality =pre_quality;
            }
        }        
    }

    if (next_quality ==rates_size-1)
    {
        double time_to_download = video_data.averageBitrate[next_quality] *video_data.segmentDuration/
                                1000.0/bitrate_est;
        double sleep_time = currDt - t - time_to_download;
        if (sleep_time > 0)
        {
            delay =sleep_time*1000;
            reply.nextDownloadDelay =MilliSeconds(delay);
        }
    }
    reply.nextQuality =next_quality;
    return reply;
}
AlgorithmReply SftmAlgorithm::GetNextQuality(PieroDashBase *client,Time now,int pre_quality,int segment_count){
    AlgorithmReply reply;
    if(segment_count==0){
        return reply;
    }
    const VideoData & video_data=client->get_video_data();
    const ThroughputData &throughput=client->get_throughput();
    int next_quality=pre_quality;
    Time fetch_time=throughput.transmissionEnd.back()-throughput.transmissionStart.back ();
    double msd=video_data.segmentDuration;
    double sft=fetch_time.GetMilliSeconds();
    double tbmt=target_buffer_;
    double ts_ns = segment_count * msd;  // Timestamp of the next segment
    double ts_o =client->get_played_frames()*1000/kFramesPerSecond;
    double rsft = ts_ns - ts_o - tbmt; // Remaining segment fetch time
    double rho = 0.75;
    double rsft_s = rho * msd;
    if (rsft < rsft_s && !rsft_exceeded_)
    {
        rsft = rsft_s;
    }else{
        rsft_exceeded_ = true;
    }
    double sftm = std::min (msd, rsft) / sft;
    int rateInd=pre_quality;
    int rates_size=video_data.averageBitrate.size();
    double ec_u = rateInd < rates_size - 1
                ? (video_data.averageBitrate[rateInd + 1] - video_data.averageBitrate[rateInd]) / video_data.averageBitrate[rateInd]
                : std::numeric_limits<double>::max ();        
    double emax_u = 0;
    for (int i = 0; i < rates_size - 1; i++)
    {
        double e = (video_data.averageBitrate[i + 1] - video_data.averageBitrate[i]) / video_data.averageBitrate[i];
        emax_u = std::max (emax_u, e);
    }
    double e_u = std::min (emax_u, 2 * ec_u);
    double ec_d = rateInd > 0 ? (video_data.averageBitrate[rateInd] - video_data.averageBitrate[rateInd - 1]) / video_data.averageBitrate[rateInd] : 1.0;
    
    double emin_d = std::numeric_limits<double>::max ();
    for (int i = 1; i < rates_size; i++)
    {
        double e = (video_data.averageBitrate[i] -video_data.averageBitrate[i - 1])/video_data.averageBitrate[i];
        emin_d = std::min (emin_d, e);
    }
    double e_d = std::max (2 * emin_d, ec_d);
    if (sftm > 1 + e_u && rateInd < rates_size - 1){ 
    // Switch up
        next_quality =rateInd + 1;
    }else if (sftm < 1 - e_d && rateInd != 0){
        // Switch down
        for (int i = 0; i < rateInd; i++)
        {
            if (video_data.averageBitrate[i] >= sftm * video_data.averageBitrate[rateInd])
            {
                break;
            }
            next_quality =i;
        }
    }        
    double bmt_c =client->get_buffer_level_ms();
    double b_max = video_data.averageBitrate[rates_size - 1];
    double b_min =video_data.averageBitrate[0];
    double t_id = bmt_c  - video_data.segmentDuration* b_max / b_min;
    reply.nextQuality =next_quality;
    if (t_id > 0){
        reply.nextDownloadDelay =MilliSeconds(t_id);
    }
    return reply;
}
SvaaAlgorithm::SvaaAlgorithm(int64_t target_buffer):target_buffer_(target_buffer),m_m_k_1 (0), m_m_k_2 (0), m_counter (0){}
AlgorithmReply SvaaAlgorithm::GetNextQuality(PieroDashBase *client,Time now,int pre_quality,int segment_count){
    AlgorithmReply reply;
    if(segment_count==0){
        return reply;
    }
    const VideoData & video_data=client->get_video_data();
    const ThroughputData &throughput=client->get_throughput();
    int rates_size=video_data.averageBitrate.size();
    int next_quality=pre_quality;
    double diff =1.0*client->get_buffer_diff_ms()/1000;
    double q_tk =1.0*client->get_buffer_level_ms()/1000;
    double q_ref=target_buffer_/1000;
    double p = 1.0;
    double f_q = 2 * std::exp (p * (q_tk - q_ref)) / (1 + std::exp (p * (q_tk - q_ref)));
    double delta=1.0*video_data.segmentDuration/1000;
    double f_t = delta / (delta - diff);
    double f_u = 1;
    
    double f = f_q * f_t * f_u;
    double t_k=predicator_.Predict(throughput,5);

    double u_k = f * t_k;

    int m_k = 0;
    if (diff >= 0.4 * delta)
    {
        m_k = 1;
    }
    else if (diff >= 0.2 * delta)
    {
        m_k = 5;
    }
    else if (diff >= 0)
    {
        m_k = 15;
    }
    else
    {
        m_k = 20;
    }
    
    int m = (m_k + m_m_k_1 + m_m_k_2) / 3;
    m_m_k_2 = m_m_k_1;
    m_m_k_1 = m_k;

    double currRate=video_data.averageBitrate[pre_quality];
    if (q_tk < q_ref / 2)
    {
        int i = rates_size - 1;
        while (video_data.averageBitrate[i] > t_k && i > 0)
        {
            i--;
        }
        next_quality =i;
        reply.nextQuality =next_quality;
        return reply;
    }
    else if (u_k > currRate)
    {
        m_counter++;
        if (m_counter > m)
        {
            int i = rates_size - 1;
            while (video_data.averageBitrate[i] > t_k && i > 0)
            {
                i--;
            }
            m_counter = 0;
            next_quality =i;
            reply.nextQuality =next_quality;
            return reply;
        }
    }
    else if (u_k < currRate)
    {
        m_counter = 0;
    }
    next_quality = pre_quality;
    reply.nextQuality =next_quality;
    return reply;    
    
}
}

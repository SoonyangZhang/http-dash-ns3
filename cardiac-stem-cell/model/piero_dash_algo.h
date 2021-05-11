#include <memory>
#include "ns3/core-module.h"
#include "piero_misc.h"
namespace ns3{
class BandwidthPredicator{
public:
    virtual ~BandwidthPredicator(){}
    virtual double Predict(const ThroughputData &history,int horizon);
};
class FestiveAlgorithm:public AdaptationAlgorithm{
public:
    FestiveAlgorithm(int64_t target,int horizon,int delta);
    AlgorithmReply GetNextQuality(PieroDashBase *client,Time now,int pre_quality,int segment_count) override;
private:
    double CalculateCombineScore(int b,int b_ref,int b_cur,double predict_bandwith,
    const  std::deque<int> &history_quality,const std::vector<double> &bitrate_array);
    double CalculateStabilityScore(int b,int b_cur,const  std::deque<int> &history_quality);
    double CalculateEfficiencyScore(int b,int b_ref, double predict_bandwith,const std::vector<double> &bitrate_array);
    std::unique_ptr<BandwidthPredicator> bandwidth_predicator_;
    int64_t target_buffer_;
    int horizon_;
    int delta_;
    Ptr<UniformRandomVariable> random_;
    int switch_up_count_=0;
};
class PandaAlgorithm:public AdaptationAlgorithm{
public:
    PandaAlgorithm();
    AlgorithmReply GetNextQuality(PieroDashBase *client,Time now,int pre_quality,int segment_count) override;
private:
    int FindLargest (const VideoData & video_data,const double smooth_bw, const double delta);
    const double m_kappa;
    const double m_omega;
    const double m_alpha;
    const double m_beta;
    const double m_epsilon;
    const int64_t m_bMin;
    double m_lastBuffer;
    Time m_lastTargetInterrequestTime=Time(0);
    double m_lastBandwidthShare;
    double m_lastSmoothBandwidthShare;
    double m_lastVideoIndex;
};
//Simulation Framework for HTTP-Based Adaptive Streaming Applications
//Adaptation Algorithm for Adaptive Streaming over HTTP
class TobascoAlgorithm :public AdaptationAlgorithm{
public:
    TobascoAlgorithm ();
    AlgorithmReply GetNextQuality(PieroDashBase *client,Time now,int pre_quality,int segment_count) override;
private:
    bool BufferInc(const BufferData&buffer_data);
    bool m_running_fast_start=true;
    BandwidthPredicator predicator_;    
};
//QDASH: A QoE-aware DASH system
//reference: https://github.com/djvergad/dash
class OsmpAlgorithm:public AdaptationAlgorithm{
public:
    OsmpAlgorithm (){}
    AlgorithmReply GetNextQuality(PieroDashBase *client,Time now,int pre_quality,int segment_count) override;  
};
//Rate adaptation for adaptive HTTP streaming
class RaahsAlgorithm:public AdaptationAlgorithm{
public:
    RaahsAlgorithm ();
    AlgorithmReply GetNextQuality(PieroDashBase *client,Time now,int pre_quality,int segment_count) override;
private:
    int t_min_ms_;
    double gamma_d_= 0.67; //Switch down factor
};
//FDASH: A Fuzzy-Based MPEG/DASH Adaptation Algorithm."
class FdashAlgorithm:public AdaptationAlgorithm{
public:
    FdashAlgorithm(int64_t target_buffer=20000):target_buffer_(target_buffer){}
    AlgorithmReply GetNextQuality(PieroDashBase *client,Time now,int pre_quality,int segment_count) override;
private:
    int64_t target_buffer_;
    BandwidthPredicator predicator_;
};
//Rate adaptation for dynamic adaptive streaming over HTTP in content distribution network
class SftmAlgorithm:public AdaptationAlgorithm{
public:
    SftmAlgorithm(int64_t target_buffer=20000):target_buffer_(target_buffer){}
    AlgorithmReply GetNextQuality(PieroDashBase *client,Time now,int pre_quality,int segment_count) override;
private:
    int64_t target_buffer_;
    bool rsft_exceeded_=false;
};
//Towards agile and smooth video adaptation in dynamic HTTP streaming
class SvaaAlgorithm:public AdaptationAlgorithm{
public:
    SvaaAlgorithm(int64_t target_buffer=20000);
    AlgorithmReply GetNextQuality(PieroDashBase *client,Time now,int pre_quality,int segment_count) override;
private:
    int64_t target_buffer_;
    int m_m_k_1;
    int m_m_k_2;
    int m_counter;
    BandwidthPredicator predicator_;
};
}

#include <stdint.h>
#include "piero_dash_proto.h"
namespace ns3{
class ReinforceAlgorithm:public AdaptationAlgorithm{
public:
    ReinforceAlgorithm(int agent_id);
    ~ReinforceAlgorithm();
    AlgorithmReply GetNextQuality(PieroDashClient *client,Time now,int pre_quality,int segment_count) override;
private:
    bool SendRequestMessage(int actions,const ThroughputData &throughput,int64_t buffer_ms);
    void GetReply(AlgorithmReply & reply);
    bool IsVarIntFull(const char *buffer, size_t size);
    void CloseFd();
    int agent_id_=0;
    int sockfd_=-1;
    int request_id_=0;
};
}
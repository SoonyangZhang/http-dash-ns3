#include <stdint.h>
#include "piero_dash_proto.h"
namespace ns3{
class ReinforceAlgorithm:public AdaptationAlgorithm{
public:
    ReinforceAlgorithm(int group_id,int agent_id);
    ~ReinforceAlgorithm();
    AlgorithmReply GetNextQuality(PieroDashClient *client,Time now,int pre_quality,int segment_count) override;
    void LastSegmentArrive(PieroDashClient *client) override;
private:
    bool SendRequestMessage(PieroDashClient *client,AlgorithmReply &reply,uint8_t last);
    void GetReply(AlgorithmReply & reply);
    bool IsVarIntFull(const char *buffer, size_t size);
    void CloseFd();
    int group_id_=0;
    int agent_id_=0;
    int sockfd_=-1;
    int request_id_=0;
};
}

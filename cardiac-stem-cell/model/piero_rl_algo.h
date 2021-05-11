#include <stdint.h>
#include "piero_misc.h"
namespace ns3{
class ReinforceAlgorithm:public AdaptationAlgorithm{
public:
    ReinforceAlgorithm(int group_id,int agent_id);
    ~ReinforceAlgorithm();
    AlgorithmReply GetNextQuality(PieroDashBase *client,Time now,int pre_quality,int index) override;
    void LastSegmentArrive(PieroDashBase *client) override;
private:
    bool SendRequestMessage(PieroDashBase *client,AlgorithmReply &reply,int index=-1,uint8_t last=0);
    void GetReply(AlgorithmReply & reply);
    bool IsVarIntFull(const char *buffer, size_t size);
    void CloseFd();
    int group_id_=0;
    int agent_id_=0;
    int sockfd_=-1;
    int request_id_=0;
    int last_quality_=kDefaultQuality;
};
}

#include "hunnan_channel.h"
#include "hunnan_net_device.h"
#include "hunnan_node.h"
#include "hunnan_flag.h"
#include "ns3/log.h"
namespace ns3{
NS_LOG_COMPONENT_DEFINE ("HunnanChannel");
NS_OBJECT_ENSURE_REGISTERED (HunnanChannel);
TypeId HunnanChannel::GetTypeId (void){
  static TypeId tid = TypeId ("ns3::HunnanChannel")
    .SetParent<Object> ()
    .SetGroupName ("Hunnan")
    .AddConstructor<HunnanChannel> ()
    .AddAttribute("Delay", "Propagation delay",
                   TimeValue (MilliSeconds (10)),
                   MakeTimeAccessor (&HunnanChannel::delay_),
                   MakeTimeChecker ());
    return tid;
}
HunnanChannel::HunnanChannel(){}
HunnanChannel::~HunnanChannel(){
#if (HUNNAN_MODULE_DEBUG)
    NS_LOG_INFO("HunnanChannel dtor"<<this);
#endif
}
void HunnanChannel::Attach(Ptr<HunnanNetDevice> dev){
    NS_ASSERT(n_device_<N_DEVICES);
    link_[n_device_++].src=dev;
    if(n_device_==N_DEVICES){
        link_[0].dst=link_[1].src;
        link_[1].dst=link_[0].src;
        link_[0].state=IDLE;
        link_[1].state=IDLE;
    }
}
bool HunnanChannel::TransmitStart(Ptr<Packet> p,Ptr<HunnanNetDevice> src,Time txtime){
    NS_ASSERT(link_[0].state!=INITIALIZING);
    NS_ASSERT(link_[1].state!=INITIALIZING);
    uint32_t wire=(src==link_[0].src?0:1);
    Simulator::ScheduleWithContext(link_[wire].dst->GetNode()->GetId(),txtime+delay_,
                &HunnanNetDevice::Receive,link_[wire].dst,p->Copy());
                
    return true;
}
void HunnanChannel::DoDispose (void){
    Object::DoDispose();
#if (HUNNAN_MODULE_DEBUG)
    NS_LOG_INFO(LOC<<"HunnanNetDevice DoDispose");
#endif
}
void HunnanChannel::DoInitialize (void){
#if (HUNNAN_MODULE_DEBUG)
    NS_LOG_INFO(LOC<<"HunnanNetDevice DoInitialize");
#endif
    Object::DoInitialize();
}
}

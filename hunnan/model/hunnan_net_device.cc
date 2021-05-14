#include "hunnan_node.h"
#include "hunnan_net_device.h"
#include "hunnan_channel.h"
#include "hunnan_flag.h"
#include "ns3/log.h"
namespace ns3{
NS_LOG_COMPONENT_DEFINE ("HunnanNetDevice");
NS_OBJECT_ENSURE_REGISTERED (HunnanNetDevice);
TypeId HunnanNetDevice::GetTypeId (void){
  static TypeId tid = TypeId ("ns3::HunnanNetDevice")
    .SetParent<Object> ()
    .SetGroupName ("Hunnan")
    .AddConstructor<HunnanNetDevice> ()
    .AddAttribute ("MaxSize", "Maxmium buffer Size",
                   UintegerValue (1500*10),
                   MakeUintegerAccessor (&HunnanNetDevice::SetBufferSize,
                                         &HunnanNetDevice::GetBufferSize),
                   MakeUintegerChecker<uint64_t> ())
    .AddAttribute ("DataRate", 
               "The default data rate for link",
               DataRateValue (DataRate ("32768b/s")),
               MakeDataRateAccessor (&HunnanNetDevice::bps_),
               MakeDataRateChecker ());
    return tid;
}
HunnanNetDevice::HunnanNetDevice(){}
HunnanNetDevice::~HunnanNetDevice(){
#if (HUNNAN_MODULE_DEBUG)
    NS_LOG_INFO("HunnanNetDevice dtor"<<this);
#endif
}
void HunnanNetDevice::SetNode(Ptr<HunnanNode> node){
    node_=node;
}
Ptr<HunnanNode> HunnanNetDevice::GetNode() const{
    return node_;
}
bool HunnanNetDevice::Attach(Ptr<HunnanChannel> ch){
    channel_=ch;
    channel_->Attach(this);
    link_up_=true;
    return true;
}
Ptr<HunnanChannel> HunnanNetDevice::GetChannel() const{
    return channel_;
}
void HunnanNetDevice::SetPacketDropTrace(Callback<void,Ptr<const Packet> > cb){
    packet_drop_trace_=cb;
}
bool HunnanNetDevice::Send(Ptr<Packet> packet){
    if(!IsLinkUp()){
        NS_ASSERT_MSG(0,"link is not up");
        return false;
    }
    uint32_t sz=packet->GetSize();
    bool reliable=false;
#if HUNNAN_DEVICE_NO_DROP
    reliable=true;
#endif 
    if(sz+buffered_bytes_>max_buffer_size_){
        if(!packet_drop_trace_.IsNull()){
            packet_drop_trace_(packet);
        }
        if(!reliable){
            NS_LOG_ERROR(LOC<<"buffer overflow and drop");
            return false;
        }
    }
    buffered_bytes_+=sz;
    buffer_.push_back(packet);
    if(READY==tx_state_){
        Ptr<Packet> p=buffer_.front();
        buffer_.pop_front();
        buffered_bytes_-=p->GetSize();
        bool ret=TransmitStart(p);
        return ret;
    }
    return true;
}
void HunnanNetDevice::Receive(Ptr<Packet> packet){
    NS_ASSERT(node_);
    node_->Receive(this,packet);
}
void HunnanNetDevice::DoDispose (void){
#if (HUNNAN_MODULE_DEBUG)
    NS_LOG_INFO(LOC<<"HunnanNetDevice DoDispose");
#endif
    node_=0;
    channel_=0;
    packet_drop_trace_=MakeNullCallback<void, Ptr<const Packet> >();
    Object::DoDispose();
}
void HunnanNetDevice::DoInitialize (void){
    Object::DoInitialize();
}
bool HunnanNetDevice::TransmitStart (Ptr<Packet> p){
    NS_ASSERT(channel_);
    tx_state_=BUSY;
    Time tx_time=bps_.CalculateBytesTxTime (p->GetSize ());
    channel_->TransmitStart(p,this,tx_time);
    Simulator::Schedule (tx_time, &HunnanNetDevice::TransmitComplete, this);
    return true;
}
void HunnanNetDevice::TransmitComplete(void){
    tx_state_=READY;
    if(buffer_.size()>0){
        Ptr<Packet> p=buffer_.front();
        buffer_.pop_front();
        buffered_bytes_-=p->GetSize();
        TransmitStart(p);
    }
}
Ptr<HunnanNetDevice> HunnanNetDeviceContainer::Get(uint32_t index) const{
    NS_ASSERT_MSG(index<devices_.size(),"out of range");
    return devices_[index];
}
void HunnanNetDeviceContainer::Add(Ptr<HunnanNetDevice> dev){
    devices_.push_back(dev);
}
uint32_t HunnanNetDeviceContainer::GetN() const{
    return devices_.size();
}

}

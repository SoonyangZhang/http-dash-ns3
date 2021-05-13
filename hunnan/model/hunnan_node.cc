#include "hunnan_node.h"
#include "hunnan_app.h"
#include "hunnan_net_device.h"
#include "hunnan_host_list.h"
#include "hunnan_flag.h"
#include "ns3/log.h"
namespace ns3{
NS_LOG_COMPONENT_DEFINE ("HunnanNode");
NS_OBJECT_ENSURE_REGISTERED (HunnanNode);
TypeId  HunnanNode::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::HunnanNode")
    .SetParent<Object> ()
    .SetGroupName("Hunnan")
    .AddConstructor<HunnanNode> ()
    ;
    return tid;
}
HunnanNode::HunnanNode(){
    Construct();
}
HunnanNode::~HunnanNode(){
#if defined(HUNNAN_MODULE_DEBUG)
    NS_LOG_INFO("HunnanNode dtor"<<this);
#endif
}
uint32_t HunnanNode::AddDevice(Ptr<HunnanNetDevice> dev){
    uint32_t index=devices_.size();
    devices_.push_back(dev);
    dev->SetNode(this);
    return index;
}
Ptr<HunnanNetDevice> HunnanNode::GetDevice(uint32_t index) const{
    NS_ASSERT_MSG(index<devices_.size(),"out of range");
    return devices_[index];
}
uint32_t HunnanNode::GetNDevices() const{
    return devices_.size();
}
uint32_t HunnanNode::AddApplication(Ptr<HunnanApplication> application){
    uint32_t index=applications_.size();
    applications_.push_back(application);
    application->SetNode(this);
    return index;
}
Ptr<HunnanApplication> HunnanNode::GetApplication(uint32_t index) const{
     NS_ASSERT_MSG(index<applications_.size(),"out of range");
     return applications_[index];
}
uint32_t HunnanNode::GetNApplications() const{
    return applications_.size();
}
bool HunnanNode::Send(Ptr<Packet> packet){
    NS_ASSERT(devices_.size()>0);
    Ptr<Packet> copy=packet->Copy();
    return devices_[0]->Send(copy);
}
void HunnanNode::Receive(Ptr<HunnanNetDevice> dev,Ptr<Packet> packet){
    if(applications_.size()>0){
        applications_[0]->Receive(packet);
    }
}
void HunnanNode::DoDispose (void){
#if defined(HUNNAN_MODULE_DEBUG)
    NS_LOG_INFO(LOC<<"HunnanNode DoDispose");
#endif
    for(auto i=devices_.begin();i!=devices_.end();i++){
        Ptr<HunnanNetDevice> dev=*i;
        dev->Dispose();
        *i = 0;
    }
    devices_.clear();
    for(auto i=applications_.begin();i!=applications_.end();i++){
        Ptr<HunnanApplication> application=*i;
        application->Dispose();
        *i = 0;
    }
    applications_.clear();
    Object::DoDispose();
}
void HunnanNode::DoInitialize (void){
    int sz=0;
    sz=devices_.size();
    for (int i=0;i<sz;i++){
        devices_[i]->Initialize();
    }
    sz=applications_.size();
    for(int i=0;i<sz;i++){
        applications_[i]->Initialize();
    }
    Object::DoInitialize();
}
void HunnanNode::Construct(void){
    uuid_=HunnanHostList::AddNode(this);
}

HunnanNodeContainer::HunnanNodeContainer(uint32_t size){
    if(size>0){
        Create(size);
    }
}
HunnanNodeContainer::~HunnanNodeContainer(){}
void HunnanNodeContainer::Create(uint32_t size){
    for(uint32_t i=0;i<size;i++){
        nodes_.push_back(CreateObject<HunnanNode>());
    }
}
Ptr<HunnanNode> HunnanNodeContainer::Get(uint32_t index){
    NS_ASSERT_MSG(index<nodes_.size(),"out of range");
    return nodes_[index];
}
uint32_t HunnanNodeContainer::GetN() const{
    return nodes_.size();
}

}

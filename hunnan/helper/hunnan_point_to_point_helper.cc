#include "hunnan_point_to_point_helper.h"
#include "hunnan_channel.h"
#include "hunnan_net_device.h"
#include "hunnan_node.h"
namespace ns3{
HunnanPointToPointHelper::HunnanPointToPointHelper(){
    device_factory_.SetTypeId("ns3::HunnanNetDevice");
    channel_factory_.SetTypeId("ns3::HunnanChannel");
}
HunnanPointToPointHelper::~HunnanPointToPointHelper(){}
void HunnanPointToPointHelper::SetDeviceAttribute(std::string n1, const AttributeValue &v1){
    device_factory_.Set(n1,v1);
}
void HunnanPointToPointHelper::SetChannelAttribute(std::string n1, const AttributeValue &v1){
    channel_factory_.Set(n1,v1);
}
HunnanNetDeviceContainer HunnanPointToPointHelper::Install(Ptr<HunnanNode> a,Ptr<HunnanNode> b){
    HunnanNetDeviceContainer container;
    Ptr<HunnanChannel> channel=channel_factory_.Create<HunnanChannel>();
    Ptr<HunnanNetDevice> devA=device_factory_.Create<HunnanNetDevice>();
    Ptr<HunnanNetDevice> devB=device_factory_.Create<HunnanNetDevice>();
    a->AddDevice(devA);
    b->AddDevice(devB);
    devA->Attach(channel);
    devB->Attach(channel);
    container.Add(devA);
    container.Add(devB);
    return container;
}
}

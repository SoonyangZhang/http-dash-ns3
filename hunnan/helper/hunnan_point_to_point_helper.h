#pragma once
#include <string>
#include "ns3/object-factory.h"
#include "ns3/ptr.h"
namespace ns3{
class HunnanNode;
class HunnanNetDevice;
class HunnanNetDeviceContainer;
class HunnanChannel;
class HunnanPointToPointHelper{
public:
    HunnanPointToPointHelper();
    ~HunnanPointToPointHelper();
    void SetDeviceAttribute(std::string n1, const AttributeValue &v1);
    void SetChannelAttribute(std::string n1, const AttributeValue &v1);
    HunnanNetDeviceContainer Install(Ptr<HunnanNode> a,Ptr<HunnanNode> b);
private:
    ObjectFactory device_factory_;
    ObjectFactory channel_factory_;
};
}

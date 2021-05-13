#include "hunnan_source_sink.h"
#include "hunnan_node.h"
#include "hunnan_flag.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
namespace ns3{
NS_LOG_COMPONENT_DEFINE ("HunnanSourceSink");
HunnanSourceApp::~HunnanSourceApp(){
#if defined(HUNNAN_MODULE_DEBUG)
    NS_LOG_INFO("HunnanSourceApp dtor"<<this);
#endif
}
void HunnanSourceApp::StartApplication(){
    if(node_){
        Ptr<Packet> p=Create<Packet>(1500);
        for(int i=0;i<10;i++){
            node_->Send(p);
        }
    }
}
void HunnanSourceApp::Receive(Ptr<Packet> packet){
    uint32_t ms=Simulator::Now().GetMilliSeconds();
    NS_LOG_INFO(ms<<" source "<<packet->GetSize());
}
HunnanSinkApp::~HunnanSinkApp(){
#if defined(HUNNAN_MODULE_DEBUG)
    NS_LOG_INFO("HunnanSinkApp dtor"<<this);
#endif
}
void HunnanSinkApp::StartApplication(){
    
}
void HunnanSinkApp::Receive(Ptr<Packet> packet){
    uint32_t ms=Simulator::Now().GetMilliSeconds();
    NS_LOG_INFO(ms<<" sink "<<packet->GetSize());
    if(node_){
        node_->Send(packet);
    }
}
}
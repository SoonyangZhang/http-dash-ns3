#include "hunnan_app.h"
#include "hunnan_node.h"
#include "hunnan_flag.h"
#include "ns3/simulator.h"
#include "ns3/log.h"
namespace ns3{
NS_LOG_COMPONENT_DEFINE ("HunnanApplication");
NS_OBJECT_ENSURE_REGISTERED (HunnanApplication);
TypeId  HunnanApplication::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::HunnanApplication")
    .SetParent<Object> ()
    .SetGroupName("Hunnan")
    .AddConstructor<HunnanApplication> ()
    ;
    return tid;
}
void HunnanApplication::SetStartTime (Time start){
    start_time_=start;
}
void HunnanApplication::SetStopTime (Time stop){
    stop_time_=stop;
}
Ptr<HunnanNode> HunnanApplication::GetNode () const{
    return node_;
}
void HunnanApplication::SetNode(Ptr<HunnanNode> node){
    node_=node;
}
bool HunnanApplication::Send(Ptr<Packet> packet){
    bool ret=false;
    if(node_){
        ret=node_->Send(packet);
    }
    return ret;
}

void HunnanApplication::Receive(Ptr<Packet> packet){}
void HunnanApplication::DoDispose (void){
    node_=0;
    Object::DoDispose();
}
void HunnanApplication::DoInitialize (void){
    start_event_ = Simulator::Schedule (start_time_, &HunnanApplication::StartApplication, this);
    if (stop_time_!= TimeStep (0)){
        stop_event_= Simulator::Schedule (stop_time_, &HunnanApplication::StopApplication, this);
    }
    Object::DoInitialize ();
}
void HunnanApplication::StartApplication(){}
void HunnanApplication::StopApplication(){}
}

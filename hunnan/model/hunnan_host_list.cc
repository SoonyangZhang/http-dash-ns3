#include <vector>
#include "ns3/ptr.h"
#include "ns3/object.h"
#include "ns3/nstime.h"
#include "ns3/simulator.h"
#include "ns3/log.h"
#include "hunnan_host_list.h"
#include "hunnan_node.h"
#include "hunnan_flag.h"
namespace ns3{
NS_LOG_COMPONENT_DEFINE ("HunnanHostList");
class HunnanHostListPriv: public Object{
public:
    static TypeId GetTypeId (void);
    HunnanHostListPriv();
    ~HunnanHostListPriv();
    static Ptr<HunnanHostListPriv> Get (void);
    uint32_t AddNode(Ptr<HunnanNode> node);
    HunnanHostList::Iterator Begin (void);
    HunnanHostList::Iterator End (void);
    Ptr<HunnanNode> GetNode (uint32_t n);
    uint32_t GetNNodes (void);
private:
    static Ptr<HunnanHostListPriv> *DoGet (void);
    static void Delete (void);
    virtual void DoDispose (void);
    std::vector<Ptr<HunnanNode>> nodes_;
};
NS_OBJECT_ENSURE_REGISTERED (HunnanHostListPriv);

TypeId HunnanHostListPriv::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::HunnanHostListPriv")
    .SetParent<Object> ()
    .SetGroupName("Hunnan")
  ;
  return tid;
}
HunnanHostListPriv::HunnanHostListPriv(){}
HunnanHostListPriv::~HunnanHostListPriv(){}
Ptr<HunnanHostListPriv> HunnanHostListPriv::Get (void){
    return *DoGet();
}
uint32_t HunnanHostListPriv::AddNode(Ptr<HunnanNode> node){
    uint32_t index = nodes_.size ();
    nodes_.push_back(node);
    Simulator::ScheduleWithContext (index, TimeStep (0), &HunnanNode::Initialize, node);
    return index;
}
HunnanHostList::Iterator HunnanHostListPriv::Begin (void){
    return nodes_.begin();
}
HunnanHostList::Iterator HunnanHostListPriv::End (void){
    return nodes_.end();
}
Ptr<HunnanNode> HunnanHostListPriv::GetNode (uint32_t n){
    NS_ASSERT_MSG(n<nodes_.size(),"out of range");
    return nodes_[n];
}
uint32_t HunnanHostListPriv::GetNNodes (void){
    return nodes_.size();
}
Ptr<HunnanHostListPriv> * HunnanHostListPriv::DoGet (void){
    static Ptr<HunnanHostListPriv> ptr = 0;
    if(0==ptr){
        ptr = CreateObject<HunnanHostListPriv>();
        Simulator::ScheduleDestroy (&HunnanHostListPriv::Delete);
    }
    return &ptr;
}
void HunnanHostListPriv::Delete (void){
    (*DoGet ()) = 0;
}
void HunnanHostListPriv::DoDispose (void){
    NS_LOG_FUNCTION (this);
    for(auto it=nodes_.begin();it!=nodes_.end();it++){
        Ptr<HunnanNode> node=*it;
        node->Dispose();
    }
    nodes_.erase (nodes_.begin (), nodes_.end ());
    Object::DoDispose ();
}
uint32_t HunnanHostList::AddNode(Ptr<HunnanNode> node){
    return HunnanHostListPriv::Get()->AddNode(node);
}
HunnanHostList::Iterator HunnanHostList::Begin (void){
    return HunnanHostListPriv::Get()->Begin();
}
HunnanHostList::Iterator HunnanHostList::End(void){
    return HunnanHostListPriv::Get()->End();
}
Ptr<HunnanNode> HunnanHostList::GetNode (uint32_t n){
    return HunnanHostListPriv::Get()->GetNode(n);
}
uint32_t HunnanHostList::GetNNodes(void){
    return HunnanHostListPriv::Get()->GetNNodes();
}
}

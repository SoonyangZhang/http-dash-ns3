#pragma once
#include "ns3/ptr.h"
#include "ns3/object.h"
#include "ns3/packet.h"
#include "ns3/nstime.h"
#include "ns3/callback.h"
#include "ns3/event-id.h"
namespace ns3{
class HunnanNode;
class HunnanApplication: public Object{
public:
    static TypeId GetTypeId (void);
    HunnanApplication(){}
    virtual ~HunnanApplication(){}
    void SetStartTime (Time start);
    void SetStopTime (Time stop);
    Ptr<HunnanNode> GetNode () const;
    void SetNode (Ptr<HunnanNode> node);
    bool Send(Ptr<Packet> packet);
protected:
    virtual void Receive(Ptr<Packet> packet);
    virtual void DoDispose (void);
    virtual void DoInitialize (void);
    virtual void StartApplication();
    virtual void StopApplication();
    Ptr<HunnanNode> node_;
private:
    Time start_time_=Time(0);
    Time stop_time_=Time(0);
    EventId start_event_;
    EventId stop_event_;
friend class HunnanNode;
};
}

#pragma once
#include <vector>
#include "ns3/ptr.h"
#include "ns3/object.h"
#include "ns3/packet.h"
namespace ns3{
class HunnanNetDevice;
class HunnanApplication;
class HunnanNode :public Object{
public:
    static TypeId GetTypeId (void);
    HunnanNode();
    virtual ~HunnanNode();
    uint32_t AddDevice(Ptr<HunnanNetDevice> dev);
    Ptr<HunnanNetDevice> GetDevice(uint32_t index) const;
    uint32_t GetNDevices() const;
    uint32_t AddApplication(Ptr<HunnanApplication> application);
    Ptr<HunnanApplication> GetApplication(uint32_t index) const;
    uint32_t GetNApplications() const;
    uint32_t GetId(void) const {return uuid_;}
    bool Send(Ptr<Packet> packet);
    void Receive(Ptr<HunnanNetDevice> dev,Ptr<Packet> packet);
protected:
    virtual void DoDispose (void);
    virtual void DoInitialize (void);
private:
    void Construct(void);
    uint32_t uuid_=0;
    std::vector<Ptr<HunnanNetDevice>> devices_;
    std::vector<Ptr<HunnanApplication>> applications_;
};
class HunnanNodeContainer{
public:
    HunnanNodeContainer(uint32_t size=0);
    ~HunnanNodeContainer();
    void Create(uint32_t size);
    Ptr<HunnanNode> Get(uint32_t index);
    uint32_t GetN() const;
private:
    std::vector<Ptr<HunnanNode>> nodes_;
};
}

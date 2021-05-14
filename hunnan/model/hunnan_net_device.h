#pragma once
#include <deque>
#include <vector>
#include "ns3/core-module.h"
#include "ns3/data-rate.h"
#include "ns3/packet.h"
namespace ns3{
class HunnanChannel;
class HunnanNode;
class HunnanNetDevice :public Object{
public:
    enum TxState{
        READY,
        BUSY,
    };
    static TypeId GetTypeId (void);
    HunnanNetDevice();
    virtual ~HunnanNetDevice();
    bool Send(Ptr<Packet> packet);
    void Receive(Ptr<Packet> packet);
    void SetNode(Ptr<HunnanNode> node);
    Ptr<HunnanNode> GetNode() const;
    bool Attach(Ptr<HunnanChannel> ch);
    Ptr<HunnanChannel> GetChannel() const;
    DataRate GetRate() const {return bps_;}
    void SetRate(DataRate bps) {bps_=bps;}
    void SetBufferSize(uint64_t size) {max_buffer_size_=size;}
    uint64_t GetBufferSize() const {return max_buffer_size_;}
    bool IsLinkUp (void) const {return link_up_;}
    void SetPacketDropTrace(Callback<void,Ptr<const Packet> > cb);
protected:
    virtual void DoDispose (void);
    virtual void DoInitialize (void);
    Ptr<HunnanNode> node_;
    Ptr<HunnanChannel> channel_;
private:
    bool TransmitStart (Ptr<Packet> p);
    void TransmitComplete (void);
    std::deque<Ptr<Packet>> buffer_;
    uint64_t buffered_bytes_=0;
    uint64_t max_buffer_size_=0;
    DataRate bps_;
    TxState tx_state_=READY;
    bool link_up_=false;
    Callback<void,Ptr<const Packet> > packet_drop_trace_;
};
class HunnanNetDeviceContainer{
public:
    HunnanNetDeviceContainer(){}
    ~HunnanNetDeviceContainer(){}
    Ptr<HunnanNetDevice> Get(uint32_t index) const;
    void Add(Ptr<HunnanNetDevice> dev);
    uint32_t GetN() const;
private:
    std::vector<Ptr<HunnanNetDevice>> devices_;
};
}

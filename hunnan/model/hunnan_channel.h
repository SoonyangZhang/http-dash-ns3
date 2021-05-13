#pragma once
#include "ns3/core-module.h"
#include "ns3/nstime.h"
#include "ns3/packet.h"
namespace ns3{
class HunnanNetDevice;
class HunnanChannel: public Object{
public:
    static TypeId GetTypeId (void);
    HunnanChannel();
    virtual ~HunnanChannel();
    void Attach(Ptr<HunnanNetDevice> dev);
    bool TransmitStart(Ptr<Packet> p,Ptr<HunnanNetDevice> src,Time txtime);
    Time GetDelay(void) const {return delay_;}
protected:
    virtual void DoDispose (void);
    virtual void DoInitialize (void);
private:
    enum WireState{
        /** Initializing state */
        INITIALIZING,
        /** Idle state (no transmission from NetDevice) */
        IDLE,
        /** Transmitting state (data being transmitted from NetDevice. */
        TRANSMITTING,
        /** Propagating state (data is being propagated in the channel. */
        PROPAGATING
    };
    class Link{
    public:
    /** \brief Create the link, it will be in INITIALIZING state
    *
    */
    Link() : state (INITIALIZING), src (0), dst (0) {}
    
    WireState                  state; //!< State of the link
    Ptr<HunnanNetDevice> src;   //!< First NetDevice
    Ptr<HunnanNetDevice> dst;   //!< Second NetDevice
    };
    static const int N_DEVICES = 2;
    Time          delay_;  
    Link    link_[N_DEVICES]; //!< Link model
    int n_device_=0;
};
}
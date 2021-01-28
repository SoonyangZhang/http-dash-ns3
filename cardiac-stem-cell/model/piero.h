#pragma once
#include <stdint.h>
#include <map>
#include <deque>
#include <string>
#include <fstream>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
/*
Piero Anversa is a notorious reseacher on stem cell. 
I use his name for this project.
The throughput and delay of a channel is based on collected trace.
Nothing is real, and do not take it seriously.
*/
namespace ns3{
class PieroPacket{
public:
    PieroPacket():PieroPacket(0,0,0,0,0){}
    PieroPacket(int src_arg,int dst_arg,int seq_arg,int sent_arg,int length_arg)
    :src(src_arg),dst(dst_arg),seq(seq_arg),sent_time(sent_arg),length(length_arg),extension(0){}
    virtual ~PieroPacket(){}
    int src;
    int dst;
    int seq;
    int sent_time;
    int length;
    uint8_t extension:1;
};
class PieroSink{
public:
    virtual ~PieroSink(){}
    virtual void OnPacket(PieroPacket *packet)=0;
};
// add the delay to the packet then deliver it to destination.
struct UniqueKey{
    UniqueKey(Time future_arg,int64_t id_arg):future(future_arg),id(id_arg){}
    Time future;
    int64_t id;
bool operator <(const UniqueKey &key) const{
    if(future<key.future){
        return true;
    }
    else{
        return id<key.id;
    }
}
};
class PieroUniChannel{
public:
    PieroUniChannel(PieroSink *sink):sink_(sink){}
    ~PieroUniChannel();
    void SetSinkNull(){
        sink_=nullptr;
    }
    void AddPacket(PieroPacket *packet,Time delay);
    void OnTimerEvent();
private:
    PieroSink *sink_=nullptr;
    EventId timer_;
    int64_t id_=0;
    std::map<UniqueKey,PieroPacket*> packets_;
};
class PieroBiChannel;
class PieroSocket :public Object{
public:
    PieroSocket(PieroBiChannel *channel):channel_(channel){}
    void SetChannelNull(){channel_=nullptr;}
    ~PieroSocket(){}
    void Send(PieroPacket *packet,Time delay);
    void Forward(PieroPacket *packet);
    typedef Callback<void,PieroSocket *,PieroPacket *> RecvCallback;
    void SetRecvCallback(RecvCallback cb){delivery_=cb;}
private:
    PieroBiChannel *channel_=nullptr;
    RecvCallback delivery_;
};
//A<=====>B
class PieroChannelToSink:public Object,public PieroSink{
public:
    PieroChannelToSink(Ptr<PieroSocket> socket):channel_(this),socket_(socket){}
    ~PieroChannelToSink() override;
    void OnPacket(PieroPacket *packet) override;
    void Send(PieroPacket *packet,Time delay);
private:
    PieroUniChannel channel_;
    Ptr<PieroSocket> socket_;
};
class PieroBiChannel:public Object{
public:
    PieroBiChannel();
    ~PieroBiChannel();
    Ptr<PieroSocket> GetSocketA() {return a_;}
    Ptr<PieroSocket> GetSocketB() {return b_;}
private:
    friend class PieroSocket;
    void SendPacket(PieroSocket *socket,PieroPacket *packet,Time delay);
    Ptr<PieroChannelToSink> link_a_;
    Ptr<PieroChannelToSink> link_b_;
    Ptr<PieroSocket> a_;
    Ptr<PieroSocket> b_;
};
int CountLines(std::string &name);
enum RateTraceType{
    NONE,
    TIME_BW,
    BW,
};
enum RateUnit{
    BW_bps,
    BW_Kbps,
    BW_Mbps,
};
enum TimeUnit{
    TIME_MS,
    TIME_S,
};
class StopBroadcast{
public:
    class Visitor{
    public:
        virtual ~Visitor(){}
        virtual void OnStop()=0;
    };
    StopBroadcast(){}
    ~StopBroadcast(){}
    void Fire();
    void Register(Visitor *visitor);
private:
    std::deque<Visitor*> visitors_;
};
class PieroTraceChannel:public Object,public StopBroadcast::Visitor{
public:
    PieroTraceChannel(Ptr<PieroSocket> socket,StopBroadcast *broadcast);
    ~PieroTraceChannel();
    void SendData(int64_t bytes);
    void SendMessage(PieroPacket *packet);
    void ReadPacketFromSocket(PieroSocket *socket,PieroPacket *packet);
    void SetBandwidthTrace(std::string &name,Time interval,
    RateTraceType type=RateTraceType::BW,
    TimeUnit time_unit=TimeUnit::TIME_MS,
    RateUnit rate_unit=RateUnit::BW_bps);
    void SetSeed(int stream);
    void SetDelay(Time delay) {channel_delay_=delay;}
    typedef Callback<void,PieroTraceChannel *,PieroPacket *> RecvCallback;
    void SetRecvCallback(RecvCallback cb){notify_read_=cb;}
    typedef Callback<void,PieroTraceChannel *> NotifyWriteCallback;
    void SetNotifyWriteCallBack(NotifyWriteCallback cb) {notify_write_=cb;}
    void OnSendTimerEvent();
    void OnBandwidthTimerEvent();
    Time GetTraceTime(int i) const {return trace_time_[i%2];}
    DataRate GetTraceRate(int i) const {return trace_rate_[i%2];}
    bool UpdateBandwidth(uint8_t index);
    void OnStop() override {running_=false;}
private:
    void InnerSend();
    Ptr<PieroSocket> socket_;
    Time trace_time_[2];
    DataRate trace_rate_[2];
    uint8_t index_=0;
    RateTraceType type_=RateTraceType::NONE;
    int64_t time_unit_=1;
    int64_t rate_unit_=1;
    int lines_=0;    
    Time channel_delay_;
    RecvCallback notify_read_;
    NotifyWriteCallback notify_write_;
    int64_t remain_bytes_=0;
    EventId send_timer_;
    EventId bandwidth_timer_;
    std::fstream bw_trace_;
    std::deque<PieroPacket *> messages_;
    Ptr<UniformRandomVariable> offset_generator_;
    int seq_=0;
    bool running_=true;
};
}

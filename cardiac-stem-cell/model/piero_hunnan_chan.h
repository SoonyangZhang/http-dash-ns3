#pragma  once
#include "ns3/nstime.h"
#include "ns3/data-rate.h"
#include "ns3/piero_header.h"
#include "ns3/hunnan-module.h"
namespace ns3{
class HunnanClientChannel: public HunnanApplication{
public:
    HunnanClientChannel(Time max_processing_delay);
    ~HunnanClientChannel();
    bool RequestChunk(uint32_t id,uint32_t sz);
    void FireStopSignal();
    void GetStats(uint32_t &bytes,Time &start,Time &stop);
    DataRate GetEstimateBandwidth() const;
    void SetDownloadDoneCallback(Callback<void,Ptr<HunnanClientChannel>,int> cb);
    void SetRecvCallBack(Callback<void,Ptr<HunnanClientChannel>,int> cb);
private:
    virtual void DoDispose (void);
    void Receive(Ptr<Packet> packet) override;
    void SendToNetwork(Ptr<Packet> packet);
    void OnHealthEvent();
    void UpdateBandwidth(Time now,int bytes,bool done);
    Time mpt_;//link maxmium processing delay
    uint32_t chunk_size_=0;
    uint32_t chunk_recv_old_=0;
    uint32_t chunk_recv_=0;
    int32_t last_packet_seq_=-1;
    Time chunk_start_time_;
    Time chunk_end_time_;
    EventId health_timer_;
    bool dowloading_=false;
    Callback<void,Ptr<HunnanClientChannel>,int> download_done_cb_;
    Callback<void,Ptr<HunnanClientChannel>,int>  recv_packet_cb_;
#if defined (PIERO_HEADER_DBUG)
    Time last_packet_time_=Time(0);
#endif
    bool bw_resample_=false;
    Time bw_last_time_=Time(0);
    uint64_t bw_last_bytes_=0;
    uint64_t sum_bytes_=0;
    DataRate bw_est_;
};
class HunnanServerChannel: public HunnanApplication{
public:
    HunnanServerChannel();
    ~HunnanServerChannel();
    void SetRecvDataStub(Callback<void,Ptr<Packet> > cb);
    void SendToNetwork(Ptr<Packet> packet);
private:
    virtual void DoDispose (void);
    void Receive(Ptr<Packet> packet) override;
    Callback<void,Ptr<Packet> > recv_data_stub_;
};
}

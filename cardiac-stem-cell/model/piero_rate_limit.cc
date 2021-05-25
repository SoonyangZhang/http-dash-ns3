#include <time.h>
#include "piero_rate_limit.h"
#include "piero_misc.h"
#include "piero_header.h"
#include "piero_udp_chan.h"
#include "ns3/simulator.h"
#include "piero_flag.h"
#include "ns3/log.h"
namespace ns3{
const char *piero_rate_limit_name="piero_rate_limit";
NS_LOG_COMPONENT_DEFINE(piero_rate_limit_name);
#define LOC piero_rate_limit_name<<__LINE__<<":"
const int64_t kMinChannelRate=50000;//50 kbps;
PieroRateLimitBase::PieroRateLimitBase(Time start,Time stop,DatasetDescriptation *des,DataRate init_rate){
    start_time_=start;
    stop_time_=stop;
    bandwidth_=init_rate;
    random_=CreateObject<UniformRandomVariable>();
    random_->SetStream(time(NULL));
    if(des){
        file_lines_=CountFileLines(des->name);
        bw_trace_.open(des->name);
        if(!bw_trace_){
            NS_ASSERT(0);
        }
        if(TimeUnit::TIME_S==des->time_unit){
            time_unit_=1000;
        }
        if(RateUnit::BW_Kbps==des->rate_unit){
            rate_unit_=1000;
        }else if(RateUnit::BW_Mbps==des->rate_unit){
            rate_unit_=1000000;
        }
        bool success=ReadBandwidthFromFile(0)&&ReadBandwidthFromFile(1);
        NS_ASSERT(success);
    }
}
PieroRateLimitBase::~PieroRateLimitBase(){
    if(bw_trace_.is_open()){
        bw_trace_.close();
    }
}
void PieroRateLimitBase::SetSeed(int stream){
    random_->SetStream(stream);
}
void PieroRateLimitBase::DoDispose (void){
    Object::DoDispose();
}
void PieroRateLimitBase::DoInitialize (void){
    start_event_=Simulator::Schedule(start_time_,&PieroRateLimitBase::StartApplication,this);
    if(stop_time_!=Time(0)){
        stop_event_=Simulator::Schedule(stop_time_,&PieroRateLimitBase::StopApplication,this);
    }
    Object::DoInitialize();
}
void PieroRateLimitBase::OnIncomingPacket(Ptr<Packet> packet){
    if(seen_stop_){
        return;
    }
    PieroMessageHeader header;
    packet->RemoveHeader(header);
    if(PieroUdpMessageType::PUM_REQ_TYPE==header.GetType()){
        PieroChunkRequest *req=header.PeekChunkRequest();
        uint32_t id=req->id;
        uint32_t bytes=req->sz;
        //NS_LOG_INFO(LOC<<id<<" "<<bytes);
        if(pacer_.IsRunning()||request_q_.size()>0){
            request_q_.push_back(std::make_pair(id,bytes));
        }else{
            chunk_pos_=id;
            request_bytes_=bytes;
            send_bytes_=0;
            packet_seq_=0;
            pacer_=Simulator::ScheduleNow(&PieroRateLimitBase::OnPacerEvent,this);
        }
    }else if(PieroUdpMessageType::PUM_STOP_TYPE==header.GetType()){
        seen_stop_=true;
        if(pacer_.IsExpired()){
            bandwidth_timer_.Cancel();
            NS_LOG_INFO(LOC<<"task done");
        }
    }
}
void PieroRateLimitBase::StartApplication(){
    if(file_lines_>1){
        Time next=trace_time_[1]-trace_time_[0];
        bandwidth_=trace_rate_[0];
        //NS_LOG_INFO(LOC<<"trace rate "<<bandwidth_<<" "<<next.GetMilliSeconds());
        bandwidth_timer_=Simulator::Schedule(next,&PieroRateLimitBase::OnUpdateBandwidthEvent,this);
    }
}
void PieroRateLimitBase::StopApplication(){
    pacer_.Cancel();
    bandwidth_timer_.Cancel();
}
void PieroRateLimitBase::OnPacerEvent(){
    if(pacer_.IsExpired()&&request_bytes_>0){
        if(send_bytes_<request_bytes_){
            uint32_t pkt_sz=std::min((uint32_t)kPieroPacketSize,request_bytes_-send_bytes_);
            send_bytes_+=pkt_sz;
            Ptr<Packet> p=Create<Packet>(pkt_sz);
            #if (PIERO_HEADER_DBUG)
            uint32_t ms=Simulator::Now().GetMilliSeconds();
            PieroChunkResponce res;
            res.id=chunk_pos_;
            res.seq=packet_seq_;
            res.event_time=ms;
            PieroMessageTag tag(PieroUdpMessageType::PUM_RES_TYPE);
            tag.SetChunkResponce(res);
            p->AddPacketTag(tag);
            #endif
            NS_ASSERT(p->GetSize()==pkt_sz);
            packet_seq_++;
            SendToNetwork(p);
            Time next=bandwidth_.CalculateBytesTxTime(pkt_sz);
            pacer_=Simulator::Schedule(next,&PieroRateLimitBase::OnPacerEvent,this);
        }else if(send_bytes_==request_bytes_){
            if(request_q_.size()>0){
                ProcessRequestQueue();
            }else{
                if(seen_stop_){
                    NS_LOG_INFO(LOC<<"task done");
                    bandwidth_timer_.Cancel();
                }
            }
        }
    }
}
void PieroRateLimitBase::OnUpdateBandwidthEvent(){
    if(file_lines_>1&&bandwidth_timer_.IsExpired()){
        trace_time_[0]=trace_time_[1];
        trace_rate_[0]=trace_rate_[1];
        if(ReadBandwidthFromFile(1)){
            Time next=trace_time_[1]-trace_time_[0];
            bandwidth_=trace_rate_[0];
            bandwidth_timer_=Simulator::Schedule(next,&PieroRateLimitBase::OnUpdateBandwidthEvent,this);
        }else{
            //read eof clear first;
            bw_trace_.clear();
            bw_trace_.seekg(0, std::ios::beg);
            int offset=random_->GetInteger(0,file_lines_-2);
            for(int i=0;i<offset;i++){
                std::string buffer;
                getline(bw_trace_,buffer);
            }
            bool success=ReadBandwidthFromFile(0)&&ReadBandwidthFromFile(1);
            NS_ASSERT(success);
            Time next=trace_time_[1]-trace_time_[0];
            bandwidth_=trace_rate_[0];
            bandwidth_timer_=Simulator::Schedule(next,&PieroRateLimitBase::OnUpdateBandwidthEvent,this);
        }
#if (PIERO_MODULE_DEBUG)
        NS_LOG_INFO(LOC<<"trace rate "<<bandwidth_<<PIERO_MODULE_DEBUG);
#endif
    }
}
void PieroRateLimitBase::ProcessRequestQueue(){
    auto info=request_q_.front();
    request_q_.pop_front();
    chunk_pos_=info.first;
    request_bytes_=info.second;
    send_bytes_=0;
    packet_seq_=0;
    NS_LOG_FUNCTION(this);
    pacer_=Simulator::ScheduleNow(&PieroRateLimitBase::OnPacerEvent,this);
}
bool PieroRateLimitBase::ReadBandwidthFromFile(uint32_t index){
    bool ret=false;
    if(bw_trace_.is_open()&&!bw_trace_.eof()){
        std::string buffer;
        getline(bw_trace_,buffer);
            std::vector<std::string> numbers;
            BufferSplit(buffer,numbers);
            if(numbers.size()<2){
                return ret;
            }
            int64_t ms=time_unit_*std::stod(numbers[0]);
            trace_time_[index]=MilliSeconds(ms);
            double bw=std::stod(numbers[1]);
            int64_t bps=rate_unit_*bw;
            if(bps<kMinChannelRate){
                bps=kMinChannelRate;
            }
            trace_rate_[index]=DataRate(bps);
            //NS_LOG_INFO(trace_rate_[index]);
            ret=true;
    }
    return ret;
}
}

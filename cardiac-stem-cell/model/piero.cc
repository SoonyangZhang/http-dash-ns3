#include <limits>
#include "ns3/log.h"
#include "piero.h"
#include <stdio.h>
#include <vector>
#include <cctype>
namespace ns3{
NS_LOG_COMPONENT_DEFINE("piero");
const int kPacketSize=1500;
int CountLines(std::string &name){
    FILE *fp=fopen(name.c_str(), "r");
    char c;
    int lines=0;
    if(!fp){
        return lines;
    }
    for (c = getc(fp); c != EOF; c = getc(fp)){
        if (c == '\n'){
            lines = lines + 1;
        }
    }
    fclose(fp);
    return lines;
}
void Split(std::string &line,std::vector<std::string>&numbers){
    int n=line.size();
    int start=-1;
    int stop=-1;
    for(int i=0;i<n;i++){
        bool success=isdigit(line[i])||(line[i]=='.');
        if(start==-1&&success){
            start=i;
        }
        if((start!=-1)&&(!success)){
            stop=i;
        }
        if(i==n-1&&success){
            stop=i;
        }
        if(start>=0&&stop>=start){
            std::string one=line.substr(start,stop);
            numbers.push_back(one);
            start=-1;
            stop=-1;
        }
    }
}
PieroUniChannel::~PieroUniChannel(){
    while(!packets_.empty()){
        auto it=packets_.begin();
        PieroPacket* packet=it->second;
        packets_.erase(it);
        delete packet;
    }
}
void PieroUniChannel::AddPacket(PieroPacket *packet,Time delay){
    Time now=Simulator::Now();
    Time future=now+delay;
    Time current_future=Time::Max();
    if(!packets_.empty()){
        auto it=packets_.begin();
        current_future=it->first.future;
    }
    packets_.insert(std::make_pair(UniqueKey(future,id_),packet));
    id_++;
    if(future<current_future){
        if(timer_.IsRunning()){
            timer_.Cancel();
        }
        timer_=Simulator::Schedule(delay,&PieroUniChannel::OnTimerEvent,this);
    }
}
void PieroUniChannel::OnTimerEvent(){
    Time now=Simulator::Now();
    while(!packets_.empty()){
        auto it=packets_.begin();
        Time future=it->first.future;
        PieroPacket* packet=it->second;
        if(now>=future){
            packets_.erase(it);
            if(sink_){
                sink_->OnPacket(packet);
            }else{
                delete packet;
            }
        }else{
            break;
        }
    }
    if(!packets_.empty()){
        auto it=packets_.begin();
        Time future=it->first.future;
        NS_ASSERT(future>now);
        Time next=future-now;
        timer_=Simulator::Schedule(next,&PieroUniChannel::OnTimerEvent,this);
    }
}
void PieroSocket::Send(PieroPacket *packet,Time delay){
    if(channel_){
        channel_->SendPacket(this,packet,delay);
    }else{
        delete packet;
    }
}
void PieroSocket::Forward(PieroPacket *packet){
    if(!delivery_.IsNull()){
        delivery_(this,packet);
    }else{
        delete packet;
    }
}
PieroChannelToSink::~PieroChannelToSink(){
    channel_.SetSinkNull();
}
void PieroChannelToSink::Send(PieroPacket *packet,Time delay){
    channel_.AddPacket(packet,delay);
}
void PieroChannelToSink::OnPacket(PieroPacket *packet){
    socket_->Forward(packet);
}
PieroBiChannel::PieroBiChannel(){
    a_=CreateObject<PieroSocket>(this);
    b_=CreateObject<PieroSocket>(this);
    link_a_=CreateObject<PieroChannelToSink>(a_);
    link_b_=CreateObject<PieroChannelToSink>(b_);
}
PieroBiChannel::~PieroBiChannel(){
    if(a_){
        a_->SetChannelNull();
    }
    if(b_){
        b_->SetChannelNull();
    }
}
void PieroBiChannel::SendPacket(PieroSocket *socket,PieroPacket *packet,Time delay){
    if(a_==socket){
        link_b_->Send(packet,delay);
    }else if(b_=socket){
        link_a_->Send(packet,delay);
    }else{
        delete packet;
    }
}
void StopBroadcast::Fire(){
    for(auto it=visitors_.begin();it!=visitors_.end();it++){
        Visitor* element=(*it);
        element->OnStop();
    }
    visitors_.clear();
}
void StopBroadcast::Register(Visitor *visitor){
    bool exist=false;
    for(auto it=visitors_.begin();it!=visitors_.end();it++){
        if(visitor==(*it)){
            exist=true;
            break;
        }
    }
    if(!exist){
        visitors_.push_back(visitor);
    }    
}
PieroTraceChannel::PieroTraceChannel(Ptr<PieroSocket> socket,StopBroadcast *broadcast):
socket_(socket),channel_delay_(MilliSeconds(100)){
    if(broadcast){
        broadcast->Register(this);        
    }
    trace_rate_[0]=DataRate(2000000);
    trace_rate_[1]=trace_rate_[0];
    offset_generator_=CreateObject<UniformRandomVariable>();
    socket->SetRecvCallback(MakeCallback(&PieroTraceChannel::ReadPacketFromSocket,this));
}
PieroTraceChannel::~PieroTraceChannel(){
    if(bw_trace_.is_open()){
        bw_trace_.close();
    }
    while(!messages_.empty()){
        auto it=messages_.begin();
        PieroPacket *packet=(*it);
        messages_.erase(it);
        delete packet;
    }
}
void PieroTraceChannel::SendData(int64_t bytes){
   remain_bytes_+=bytes;
   if(!send_timer_.IsRunning()){
       send_timer_=Simulator::ScheduleNow(&PieroTraceChannel::OnSendTimerEvent,this);
   }
}
void PieroTraceChannel::SendMessage(PieroPacket *packet){
    messages_.push_back(packet); 
    if(!send_timer_.IsRunning()){
        send_timer_=Simulator::ScheduleNow(&PieroTraceChannel::OnSendTimerEvent,this);
    }
}
void PieroTraceChannel::ReadPacketFromSocket(PieroSocket *socket,PieroPacket *packet){
    if(!notify_read_.IsNull()){
        notify_read_(this,packet);
    }else{
        delete packet;
    }
}
void PieroTraceChannel::SetBandwidthTrace(DatasetDescriptation &des,Time interval){
    if(bw_trace_.is_open()) {return ;}
    des_=des;
    lines_=CountLines(des.name);
    trace_time_[0]=Time(0);
    trace_time_[1]=interval;
    type_=des.type;
    NS_ASSERT(lines_!=0);
    bw_trace_.open(des.name);
    if(!bw_trace_){
        NS_ASSERT(0);
    }
    if(TimeUnit::TIME_S==des.time_unit){
        time_unit_=1000;
    }
    if(RateUnit::BW_Kbps==des.rate_unit){
        rate_unit_=1000;
    }else if(RateUnit::BW_Mbps==des.rate_unit){
        rate_unit_=1000000;
    }
    bool success=UpdateBandwidth(0)&&UpdateBandwidth(1);
    NS_ASSERT(success);
    Time temp=trace_time_[1]-trace_time_[0];
    NS_LOG_INFO("first rate "<<trace_rate_[0]<<" "<<temp.GetMilliSeconds());
    Time next=trace_time_[1]-trace_time_[0];
    bandwidth_timer_=Simulator::Schedule(next,&PieroTraceChannel::OnBandwidthTimerEvent,this);
}
void PieroTraceChannel::SetSeed(int stream){
    offset_generator_->SetStream(stream);
}
void PieroTraceChannel::OnSendTimerEvent(){
    if(remain_bytes_>0||!messages_.empty()){
        InnerSend();
    }
    if((remain_bytes_==0)&&(!notify_write_.IsNull())){
        notify_write_(this);
    }
}
void PieroTraceChannel::OnBandwidthTimerEvent(){
    trace_time_[0]=trace_time_[1];
    trace_rate_[0]=trace_rate_[1];
    Time now=Simulator::Now();
    if(!running_){
        return ;
    }
    if(UpdateBandwidth(1)){
        Time next=trace_time_[1]-trace_time_[0];
        bandwidth_timer_=Simulator::Schedule(next,&PieroTraceChannel::OnBandwidthTimerEvent,this);
    }else{
        //read eof clear first;
        bw_trace_.clear();
        bw_trace_.seekg(0, std::ios::beg);
        int offset=offset_generator_->GetInteger(0,lines_-2);
        for(int i=0;i<offset;i++){
            std::string buffer;
            getline(bw_trace_,buffer);
        }
        bool success=UpdateBandwidth(0)&&UpdateBandwidth(1);
        if(!success&&des_.name.size()>0){
            NS_LOG_ERROR(des_.name<<" "<<offset);
        }
        NS_ASSERT(success);
        Time next=trace_time_[1]-trace_time_[0];
        bandwidth_timer_=Simulator::Schedule(next,&PieroTraceChannel::OnBandwidthTimerEvent,this);
    }
}
void PieroTraceChannel::InnerSend(){
    if(send_timer_.IsExpired()){
        if(socket_&&(remain_bytes_>0||!messages_.empty())){
            int now=Simulator::Now().GetMilliSeconds();
            Time next=Time(0);
            PieroPacket *packet=nullptr;
            if(!messages_.empty()){
                packet=messages_.front();
                packet->seq=seq_;
                seq_++;
                messages_.pop_front();
                next=trace_rate_[0].CalculateBytesTxTime(packet->length);
            }else if(remain_bytes_>0){
                int sent=(int)std::min((int64_t)kPacketSize,remain_bytes_);
                next=trace_rate_[0].CalculateBytesTxTime(sent);
                remain_bytes_-=sent;
                packet=new PieroPacket(0,0,seq_,now,sent);
                seq_++;
            }
            if(packet){
                socket_->Send(packet,channel_delay_);
                send_timer_=Simulator::Schedule(next,&PieroTraceChannel::OnSendTimerEvent,this);            
            }
        }else{
            if(!socket_){
                while(!messages_.empty()){
                    auto it=messages_.begin();
                    PieroPacket *packet=(*it);
                    messages_.erase(it);
                    delete packet;
                }
            }
        }        
    }
}
bool PieroTraceChannel::UpdateBandwidth(uint8_t index){
    bool ret=false;
    if(bw_trace_.is_open()&&!bw_trace_.eof()){
        std::string buffer;
        getline(bw_trace_,buffer);
        if(RateTraceType::TIME_BW==type_){
            std::vector<std::string> numbers;
            Split(buffer,numbers);
            if(numbers.size()<2){
                return ret;
            }
            int64_t ms=time_unit_*std::stod(numbers[0]);
            trace_time_[index]=MilliSeconds(ms);
            double bw=std::stod(numbers[1]);
            int64_t bps=rate_unit_*bw;
            trace_rate_[index]=DataRate(bps);
        }else{
            if(buffer.size()==0){
                return ret;
            }
            int64_t bps=rate_unit_*std::stod(buffer);
            trace_rate_[index]=DataRate(bps);           
        }
        ret=true;
    }
    return ret;
}
}

#include <string>
#include <memory.h>
#include "ns3/log.h"
#include "piero_header.h"
namespace ns3{
static std::string MessageTypeToString(uint8_t type) {
  switch (type) {
    case PieroUdpMessageType::PUM_MIN_TYPE:
      return "pum_min_type";
    case PieroUdpMessageType::PUM_REQ_TYPE:
      return "pum_req_type";
    case PieroUdpMessageType::PUM_RES_TYPE:
      return "pum_res_type";
    case PieroUdpMessageType::PUM_STOP_TYPE:
      return "pum_stop_type";
  }
  return "???";
}
int cal_varint_length(uint64_t number){
    int length=0;
    if(number<=UINT64_C(0x7f)){
        length=1;
    }else if(number<=UINT64_C(0x3fff)){
        length=2;
    }else if(number<=UINT64_C(0x1fffff)){
        length=3;
    }else if(number<=UINT64_C(0xfffffff)){
        length=4;
    }else if(number<=UINT64_C(0x7ffffffff)){
        length=5;
    }else if(number<=UINT64_C(0x3ffffffffff)){
        length=6;
    }else if(number<=UINT64_C(0x1ffffffffffff)){
        length=7;
    }else if(number<=UINT64_C(0xffffffffffffff)){
        length=8;
    }
    return length;
}
PieroMessageHeader::PieroMessageHeader(uint8_t type):type_(type){
    memset(&chunk_request_,0,sizeof(chunk_request_));
}
TypeId PieroMessageHeader::GetTypeId ()
{
    static TypeId tid = TypeId ("PieroMessageHeader")
                        .SetParent<Header> ()
                        .AddConstructor<PieroMessageHeader>();
    return tid;
}
TypeId PieroMessageHeader::GetInstanceTypeId (void) const{
    return GetTypeId();
}
void PieroMessageHeader::Print(std::ostream &os) const{
    os<<MessageTypeToString(type_);
}
uint32_t PieroMessageHeader::GetSerializedSize (void) const{
    switch (type_){
        case PieroUdpMessageType::PUM_REQ_TYPE:{
            uint32_t sz=sizeof(type_)+
            cal_varint_length(chunk_request_.id)+cal_varint_length(chunk_request_.sz);
            return sz;
        }
        case PieroUdpMessageType::PUM_STOP_TYPE:{
            return 1;
        }
    }
    return 0;
}
void PieroMessageHeader::Serialize (Buffer::Iterator start) const{
    start.WriteU8(type_);
    if(PieroUdpMessageType::PUM_REQ_TYPE==type_){
        uint64_t value[2];
        value[0]=chunk_request_.id;
        value[1]=chunk_request_.sz;
        for(int i=0;i<(int)(sizeof(value)/sizeof(value[0]));i++){
            int occupy=cal_varint_length(value[i]);
            NS_ASSERT(occupy>0);
            int64_t next=value[i];
            uint8_t first=0;
            for(int j=0;j<occupy;j++){
                first=next%128;
                next=next/128;
                if(next>0){
                    first|=128;
                }
                start.WriteU8(first);
            }
            NS_ASSERT(0==next);
        }
    }
}
uint32_t PieroMessageHeader::Deserialize (Buffer::Iterator start){
    type_=start.ReadU8();
    if(PieroUdpMessageType::PUM_REQ_TYPE==type_){
        uint64_t value[2];
        for (int i=0;i<(int)(sizeof(value)/sizeof(value[0]));i++){
            uint64_t remain=0;
            uint64_t remain_multi=1;
            uint8_t key=0;
            do{
                key=start.ReadU8();
                remain+=(key&127)*remain_multi;
                remain_multi*=128;
            }while(key&128);
            value[i]=remain;
        }
        chunk_request_.id=value[0];
        chunk_request_.sz=value[1];
    }
    return GetSerializedSize();
}
void PieroMessageHeader::SetChunkRequest(PieroChunkRequest &req){
    chunk_request_=req;
}

PieroMessageTag::PieroMessageTag(uint8_t type):type_(type){
    memset(&chunk_response_,0,sizeof(chunk_response_));
}
TypeId PieroMessageTag::GetTypeId ()
{
    static TypeId tid = TypeId ("PieroMessageTag")
                        .SetParent<Tag> ()
                        .AddConstructor<PieroMessageTag>();
    return tid;
}
TypeId PieroMessageTag::GetInstanceTypeId (void) const{
    return GetTypeId();
}
uint32_t PieroMessageTag::GetSerializedSize (void) const{
    uint32_t sz=0;
    if(PieroUdpMessageType::PUM_RES_TYPE==type_){
        sz=sizeof(type_)+cal_varint_length(chunk_response_.id)+
        cal_varint_length(chunk_response_.seq)+cal_varint_length(chunk_response_.event_time);
    }
    return sz;
}
void PieroMessageTag::Serialize (TagBuffer start) const{
    start.WriteU8(type_);
    if(PieroUdpMessageType::PUM_RES_TYPE==type_){
        uint64_t value[3];
        value[0]=chunk_response_.id;
        value[1]=chunk_response_.seq;
        value[2]=chunk_response_.event_time;
        for(int i=0;i<(int)(sizeof(value)/sizeof(value[0]));i++){
            int occupy=cal_varint_length(value[i]);
            NS_ASSERT(occupy>0);
            int64_t next=value[i];
            uint8_t first=0;
            for(int j=0;j<occupy;j++){
                first=next%128;
                next=next/128;
                if(next>0){
                    first|=128;
                }
                start.WriteU8(first);
            }
            NS_ASSERT(0==next);
        }
    }
}
void PieroMessageTag::Deserialize (TagBuffer start){
    type_=start.ReadU8();
    if(PieroUdpMessageType::PUM_RES_TYPE==type_){
        uint64_t value[3];
        for (int i=0;i<(int)(sizeof(value)/sizeof(value[0]));i++){
            uint64_t remain=0;
            uint64_t remain_multi=1;
            uint8_t key=0;
            do{
                key=start.ReadU8();
                remain+=(key&127)*remain_multi;
                remain_multi*=128;
            }while(key&128);
            value[i]=remain;
        }
        chunk_response_.id=value[0];
        chunk_response_.seq=value[1];
        chunk_response_.event_time=value[2];
    }
}
void PieroMessageTag::Print (std::ostream &os) const{
    os<<MessageTypeToString(type_);
}
void PieroMessageTag::SetChunkResponce(PieroChunkResponce &res){
    chunk_response_=res;
}

}

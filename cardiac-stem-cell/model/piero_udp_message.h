#pragma once
#include <stdint.h>
#include <string>
#include "ns3/header.h"
#include "ns3/tag.h"
namespace ns3{
#define PIEROMSGDEBUG 1
int cal_varint_length(uint64_t number);
enum PieroUdpMessageType:uint8_t{
    PUM_MIN_TYPE,
    PUM_REQ_TYPE,
    PUM_RES_TYPE,
    PUM_STOP_TYPE,
};
struct PieroChunkRequest{
    uint32_t id;
    uint32_t sz;
};
struct PieroChunkResponce{
    uint32_t id;
    uint32_t seq; // seq in a chunk
    uint32_t event_time;
};
class PieroMessageHeader :public Header{
public:
    PieroMessageHeader():PieroMessageHeader(PieroUdpMessageType::PUM_MIN_TYPE){}
    PieroMessageHeader(uint8_t type);
    
    static TypeId GetTypeId (void);
    TypeId GetInstanceTypeId (void) const override;
    void Print(std::ostream &os) const override;
    uint32_t GetSerializedSize (void) const override;
    void Serialize (Buffer::Iterator start) const override;
    uint32_t Deserialize (Buffer::Iterator start) override;
    
    void SetChunkRequest(PieroChunkRequest &req);
    uint8_t GetType() const {return type_;}
    PieroChunkRequest *PeekChunkRequest() {return &chunk_request_;}
private:
    struct{
        uint8_t type_;
        union{
            PieroChunkRequest chunk_request_;
        };
    };
};
class PieroMessageTag:public Tag{
public:
    PieroMessageTag():PieroMessageTag(PieroUdpMessageType::PUM_MIN_TYPE){}
    PieroMessageTag(uint8_t type);
    
    static TypeId GetTypeId (void);
    TypeId GetInstanceTypeId (void) const override;
    uint32_t GetSerializedSize (void) const override;
    void Serialize (TagBuffer start) const override;
    void Deserialize (TagBuffer start) override;
    void Print (std::ostream &os) const override;
    
    void SetChunkResponce(PieroChunkResponce &res);
    uint8_t GetType() const {return type_;}
    PieroChunkResponce *PeekChunkResponce() {return &chunk_response_;}
private:
    struct{
        uint8_t type_;
        union{
            PieroChunkResponce chunk_response_;
        };
    };
};
}

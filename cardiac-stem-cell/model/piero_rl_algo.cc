#include <stdio.h>
#include <errno.h>   // for errno and strerror_r
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "piero_rl_algo.h"
#include "piero_dash_client.h"
#include "piero_byte_codec.h"
#include "ns3/core-module.h"
#include "ns3/log.h"
namespace ns3{
NS_LOG_COMPONENT_DEFINE("piero_rl");
const char *g_rl_server_ip="127.0.0.1";
uint16_t g_rl_server_port=1234;
const int kBufferSize=1500;
enum MessageType{
    RL_REQUEST,
    RL_RESPONSE,
};
ReinforceAlgorithm::ReinforceAlgorithm(int group_id,int agent_id):
group_id_(group_id),
agent_id_(agent_id){
    NS_LOG_INFO(g_rl_server_ip<<":"<<g_rl_server_port);
    struct sockaddr_in servaddr;
    // assign IP, PORT 
    servaddr.sin_family = AF_INET; 
    servaddr.sin_addr.s_addr = inet_addr(g_rl_server_ip); 
    servaddr.sin_port = htons(g_rl_server_port);     
    if ((sockfd_= socket(AF_INET, SOCK_STREAM, 0)) < 0){
        printf("\n Error : Could not create socket \n");
        return ;
    }
    if (connect(sockfd_,(struct sockaddr *)&servaddr,sizeof(servaddr)) != 0) { 
        NS_LOG_ERROR("connection with the server failed"); 
        CloseFd();
        return ; 
    }
    /*if(sockfd_>0){
        set_nonblocking(sockfd_);
    }*/
}
ReinforceAlgorithm::~ReinforceAlgorithm(){
    CloseFd();
}
AlgorithmReply ReinforceAlgorithm::GetNextQuality(PieroDashClient *client,Time now,int pre_quality,int segment_count){
    AlgorithmReply reply;
    if(sockfd_>0){
        SendRequestMessage(client,reply,0);
    }else{
        reply.terminate=true;
    }
    return reply;
}
void ReinforceAlgorithm::LastSegmentArrive(PieroDashClient *client){
    if(sockfd_>0){
         AlgorithmReply reply;
         SendRequestMessage(client,reply,1);
    }
}
//Request:type(uint8_t)+request_id(int)+group_id(int)+agent_id(int)+actions(int)+bytes(uint64_t)+time_ms(uint64_t)+buffer(uint64_t)
bool ReinforceAlgorithm::SendRequestMessage(PieroDashClient *client,AlgorithmReply &reply,uint8_t last){
    const VideoData & video_data=client->get_video_data();
    const ThroughputData &throughput=client->get_throughput();
    int64_t buffer_ms=client->get_buffer_level_ms();
    int actions=video_data.representation.size();
    const std::deque<std::pair<double,double>> &qoe=client->get_reward();
    double r=0.0;
    {
        size_t n=qoe.size();
        if(n>0){
            r=qoe.at(n-1).first;
        }
    }
    uint64_t r_buffer=0;
    memcpy((void*)&r_buffer,(void*)&r,sizeof(uint64_t));
    char buffer[kBufferSize];
    DataWriter writer(buffer,kBufferSize);
    uint8_t type=RL_REQUEST;
    uint64_t bytes=0;
    uint64_t time_ms=0;
    uint64_t sum=sizeof(type)+sizeof(last)+sizeof(int)+sizeof(int)+sizeof(int)+sizeof(int)+
                 sizeof(bytes)+sizeof(time_ms)+sizeof(buffer_ms)+
                 sizeof(uint64_t);
    {
        size_t n=throughput.bytesReceived.size();
        if(n>0){
            bytes=throughput.bytesReceived.at(n-1);
            Time start=throughput.transmissionStart.at(n-1);
            Time end=throughput.transmissionEnd.at(n-1);
            time_ms=(end-start).GetMilliSeconds();
        }
    }
    bool success=writer.WriteVarInt(sum)&&
                 writer.WriteUInt8(type)&&
                 writer.WriteUInt8(last)&&
                 writer.WriteUInt32(request_id_)&&
                 writer.WriteUInt32(group_id_)&&
                 writer.WriteUInt32(agent_id_)&&
                 writer.WriteUInt32(actions)&&
                 writer.WriteUInt64(bytes)&&
                 writer.WriteUInt64(time_ms)&&
                 writer.WriteUInt64(buffer_ms)&&
                 writer.WriteUInt64(r_buffer);
    if(success){
        write(sockfd_,buffer,writer.length());
        if(!last){
            GetReply(reply);
            if(reply.nextQuality>=actions){
                reply.nextQuality=actions-1;
            }
        }
    }
    request_id_++;
    if(last){
        CloseFd();
    }
    return success;
}
void ReinforceAlgorithm::GetReply(AlgorithmReply &reply){
    char buffer[kBufferSize];
    int offset=0;
    int n=0;
    uint64_t sum=0;
    uint8_t type=0;
    uint8_t action=0;
    uint8_t terminate=0;
    uint32_t next_ms=0;
    bool full=false;
    while(true){
        n =read(sockfd_, buffer+offset,kBufferSize-offset);
        if (n>0){
            offset+=n;
            if(IsVarIntFull(buffer,offset)){
                DataReader reader(buffer,offset);
                reader.ReadVarInt(&sum);
                if(sum>=reader.BytesRemaining()){
                    full=true;
                    break;
                }
            }
        }else{
            break;
        }
    }
    DataReader reader(buffer,offset);
    bool success=reader.ReadVarInt(&sum)&&
                 reader.ReadUInt8(&type)&&
                 reader.ReadUInt8(&action)&&
                 reader.ReadUInt8(&terminate);
                 reader.ReadUInt32(&next_ms);
    if(success&&full){
        reply.nextQuality=action;
        reply.nextDownloadDelay=MilliSeconds(next_ms);
        NS_LOG_INFO(group_id_<<" "<<agent_id_<<" "<<request_id_<<" "<<(int)action);
        if(terminate){
            reply.terminate=true;
        }
    }else{
        NS_LOG_ERROR(__FUNCTION__<<" decode error");
        reply.terminate=true;
    }
}
bool ReinforceAlgorithm::IsVarIntFull(const char *buffer, size_t size){
    bool full=false;
    for(size_t i=0;i<size;i++){
        if((buffer[i]&128)==0){
            full=true;
            break;
        }
    }
    return full;
}
void ReinforceAlgorithm::CloseFd(){
    if(sockfd_>0){
        close(sockfd_);
        sockfd_=-1;
    }
}
}

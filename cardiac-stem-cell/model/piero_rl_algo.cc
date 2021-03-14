#include <stdio.h>
#include <errno.h>   // for errno and strerror_r
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h> //for sockaddr_in
#include <arpa/inet.h>  //inet_addr  
#include <netinet/tcp.h> //TCP_NODELAY
#include <fcntl.h>
#include "piero_rl_algo.h"
#include "piero_dash_client.h"
#include "piero_byte_codec.h"
#include "ns3/core-module.h"
#include "ns3/log.h"
namespace ns3{
NS_LOG_COMPONENT_DEFINE("piero_rl");
const char *g_rl_server_ip="127.0.0.1";
uint16_t g_rl_server_port=1234;
const int kBufferSize=2000;
const int kMSS=1492;
const char kPaddingData[1500]={0};
enum MessageType{
    RL_REQUEST,
    RL_RESPONSE,
};
void set_nonblocking(int fd){
  int flags = fcntl(fd, F_GETFL, 0);
  char buf[128];
  if (flags == -1) {
    int saved_errno = errno;
    strerror_r(saved_errno, buf, sizeof(buf));
    printf("%s,%s\n",__FUNCTION__,buf);
  }
  if (!(flags & O_NONBLOCK)) {
    //int saved_flags = flags;
    flags = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    if (flags == -1) {
      // bad.
        int saved_errno = errno;
        strerror_r(saved_errno, buf, sizeof(buf));
        printf("%s,%s\n",__FUNCTION__,buf);
    }
  }
}
ReinforceAlgorithm::ReinforceAlgorithm(int group_id,int agent_id):
group_id_(group_id),
agent_id_(agent_id){
    NS_LOG_INFO(g_rl_server_ip<<":"<<g_rl_server_port);
    struct sockaddr_in servaddr;
    // assign IP, PORT 
    servaddr.sin_family = AF_INET; 
    servaddr.sin_addr.s_addr = inet_addr(g_rl_server_ip); 
    servaddr.sin_port = htons(g_rl_server_port);
    int flag = 1;
    if ((sockfd_= socket(AF_INET, SOCK_STREAM, 0)) < 0){
        printf("\n Error : Could not create socket \n");
        return ;
    }
    setsockopt (sockfd_, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(flag));
    if (connect(sockfd_,(struct sockaddr *)&servaddr,sizeof(servaddr)) != 0) { 
        NS_LOG_ERROR("connection with the server failed"); 
        CloseFd();
        return ; 
    }
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
    uint64_t send_time=TimeMillis();
    int info_size=sizeof(type)+sizeof(last)+sizeof(int)+sizeof(send_time)+
                 sizeof(int)+sizeof(int)+sizeof(int)+
                 sizeof(bytes)+sizeof(time_ms)+sizeof(buffer_ms)+
                 sizeof(uint64_t);
    uint64_t sum=info_size;
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
                 writer.WriteUInt64(send_time)&&
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
const uint8_t NS_MESSAGE_LABEL=0x01;
PieroUdpClient::PieroUdpClient(std::string& group_id_str,std::string& agent_id_str){
    group_id_=std::stoi(group_id_str);
    agent_id_=std::stoi(agent_id_str);
}
PieroUdpClient::~PieroUdpClient(){
    CloseFd();
}
bool PieroUdpClient::Init(const char *ip,uint16_t port){
    bool success=true;
    struct sockaddr_in servaddr;
    servaddr.sin_family = AF_INET; 
    servaddr.sin_addr.s_addr = inet_addr(ip);
    servaddr.sin_port = htons(port);
    int flag = 1; 
    if ((sockfd_= socket(AF_INET, SOCK_STREAM, 0)) < 0){
        NS_LOG_ERROR("Error : Could not create socket");
        success=false;
        return success;
    }
    setsockopt (sockfd_, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(flag));
    if (connect(sockfd_,(struct sockaddr *)&servaddr,sizeof(servaddr)) != 0) { 
        NS_LOG_ERROR("connection with the server failed"); 
        CloseFd();
        success=false;
        return success;
    }
    return success;
}
bool PieroUdpClient::HasReply(){
    int n=0;
    char buffer[1500];
    bool has_reply=false;
    Report();
    n =read(sockfd_, buffer,1500);
    if(n>0){
        has_reply=true;
    }
    CloseFd();
    return has_reply;
}
void PieroUdpClient::CloseFd(){
    if(sockfd_>0){
        close(sockfd_);
        sockfd_=-1;
    }
}
void PieroUdpClient::Report(){
    char buffer[64];
    int n=0;
    DataWriter writer(buffer,64);
    bool success=writer.WriteUInt8(NS_MESSAGE_LABEL)&&
                writer.WriteUInt32(group_id_)&&
                writer.WriteUInt32(agent_id_);
    n=write(sockfd_,buffer,writer.length());
}
}

#pragma once
#include <stdint.h>
#include <string>
#include <vector>
#include "ns3/ptr.h"
#include "ns3/object.h"
#include "ns3/random-variable-stream.h"
namespace ns3{
namespace {
    const int kPieroPathXDim=2;
    const int kPieroPathYDim=3;
}
class DashChunkSelector;
struct PieroPathInfo{
    PieroPathInfo();
    bool AddId(int x,int id,bool is_default=false);
    int GetDefaultId(int x) const;
    int GetLength(int x) const;
    int id_[kPieroPathXDim][kPieroPathYDim];
    int default_index_[kPieroPathXDim];
    int length_[kPieroPathXDim];
};
enum DispatchType:uint8_t{
    DEF_CHUNK_SPLIT,
    DEF_CHUNK_UNSPLIT,
    EPS_CHUNK_SPLIT,
    EPS_CHUNK_UNSPLIT,
    UCB_CHUNK_SPLIT,
    UCB_CHUNK_UNSPLIT,
};
std::string DispatchTypeToString(uint8_t type);
class ChunkDispatchInterface{
public:
    enum Mode{
        CHUNK_SPLIT,
        CHUNK_UNSPLIT,
    };
    virtual ~ChunkDispatchInterface(){}
    void DispatchRequest(int chunk_id,uint64_t bytes);
    void RegisterPathInfo(PieroPathInfo& info);
    virtual DispatchType GetType()=0;
protected:
    virtual void DispatchWithSplit(int chunk_id,uint64_t bytes)=0;
    virtual void DispatchNoSplit(int chunk_id,uint64_t bytes)=0;
    void SplitChunk(std::vector<int> & id_vec,std::vector<float> &ratio_vec,int chunk_id,uint64_t bytes);
    DashChunkSelector *source_;
    Mode mode_;
    PieroPathInfo info_;
};
class DefaultChunkDispatch: public ChunkDispatchInterface{
public:
    DefaultChunkDispatch(DashChunkSelector *source,Mode mode=CHUNK_UNSPLIT);
    DispatchType GetType() override;
private:
    void DispatchWithSplit(int chunk_id,uint64_t bytes) override;
    void DispatchNoSplit(int chunk_id,uint64_t bytes) override;
};
class EpsilonChunkDispatch: public ChunkDispatchInterface{
public:
    EpsilonChunkDispatch(DashChunkSelector *source,Mode mode=CHUNK_UNSPLIT);
    DispatchType GetType() override;
    void SetRandomStream(int64_t stream);
    void SetEpsilon(float v);
private:
    void DispatchWithSplit(int chunk_id,uint64_t bytes) override;
    void DispatchNoSplit(int chunk_id,uint64_t bytes) override;
    float epsilon_;
    Ptr<UniformRandomVariable> random_;
    int times_=0;
};
class UcbChunkDispatch: public ChunkDispatchInterface{
public:
    UcbChunkDispatch(DashChunkSelector *source,Mode mode=CHUNK_UNSPLIT);
    DispatchType GetType() override;
    void SetRandomStream(int64_t stream);
private:
    void DispatchWithSplit(int chunk_id,uint64_t bytes) override;
    void DispatchNoSplit(int chunk_id,uint64_t bytes) override;
    Ptr<UniformRandomVariable> random_;
    int pull_count_[kPieroPathXDim][kPieroPathYDim];
    int times_=0;
};
}

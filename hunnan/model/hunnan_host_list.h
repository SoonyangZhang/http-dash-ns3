#pragma once
#include <stdint.h>
#include <vector>
#include "ns3/ptr.h"
namespace ns3{
class HunnanNode;
class HunnanHostList{
public:
    typedef std::vector<Ptr<HunnanNode>>::const_iterator Iterator;
    static uint32_t AddNode(Ptr<HunnanNode> node);
    static Iterator Begin (void);
    static Iterator End (void);
    static Ptr<HunnanNode> GetNode (uint32_t n);
    static uint32_t GetNNodes (void);
};
}
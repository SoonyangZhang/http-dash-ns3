#pragma once
#include "hunnan_app.h"
namespace ns3{
class HunnanSourceApp: public HunnanApplication{
public:
    HunnanSourceApp(){}
    ~HunnanSourceApp();
private:
    void StartApplication() override;
    void Receive(Ptr<Packet> packet) override;
};
class HunnanSinkApp: public HunnanApplication{
public:
    HunnanSinkApp(){}
    ~HunnanSinkApp();
private:
    void StartApplication() override;
    void Receive(Ptr<Packet> packet) override;
};
}
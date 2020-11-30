#ifndef __L3_RELAY_UNIT_H
#define __L3_RELAY_UNIT_H

#include "inet/common/INETDefs.h"
#include "common/FlowSensor.h"
#include <set>
#include <map>

namespace commons {

class INET_API L3RelayUnit : public cSimpleModule
{
  public:
    L3RelayUnit();
    virtual ~L3RelayUnit();

  protected:
    commons::AbstractFlowSensor* flowSensor;
    std::map<int, int> pauseMsgController;

    virtual void initialize() override;
    virtual void handleMessage(cMessage* msg) override;

    virtual void handleMessageIcmp(cMessage* msg){}
    virtual void handleMessageIp(cMessage* msg);
    virtual void handleMessageIf(cMessage* msg);
    virtual void handleMessageIfOob(cMessage* msg);

  private:
    void getIngressIfIds(int egressIfId, std::set<int>& ingressIfIds);
    bool sendPauseMsg(int ingressIfId, bool pause);
};

} // namespace commons

#endif // #ifndef __L3_RELAY_UNIT_H

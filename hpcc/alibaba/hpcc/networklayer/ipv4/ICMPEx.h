#ifndef __ICMP_EX_H
#define __ICMP_EX_H

#include "inet/networklayer/ipv4/ICMP.h"
#include "alibaba/hpcc/common/AckMessage_m.h"

namespace hpcc {

enum L3FCNICMPType {
    ICMP_L3FCN_RATE = 30,
    ICMP_L3FCN_ACK = 31
};

class INET_API ICMPEx : public inet::ICMP
{
  public:
    ICMPEx();

  protected:
    virtual void handleMessage(cMessage* msg) override;
    virtual void processICMPMessage(inet::ICMPMessage* icmpmsg) override;

    void sendAck(AckMessage* ackMsg);

  private:
    int numAcksSent;
    int numAcksReceived;
};

} // namespace hpcc

#endif // #ifndef __ICMP_EX_H
